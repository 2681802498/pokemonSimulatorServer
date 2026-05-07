package room

import (
	"context"
	"encoding/json"
	"fmt"
	"go-server/configs"
	"go-server/internal/model"
	"go-server/internal/protocol"
	"go-server/internal/rpc"
	"go-server/internal/store"
	"log"
	"math/rand"
	"sync"
	"time"
)

type RoomStatus int

const (
	RoomWaiting RoomStatus = iota
	RoomPlaying
	RoomFinished
)

type GameRoom struct {
	ID           string
	Sessions     map[string]*Session // key 为 PlayerID
	Status       RoomStatus
	NodeID       int
	ReadyPlayers map[string]bool // 记录玩家是否已选择宝可梦
	mu           sync.Mutex
}

type RoomManager struct {
	Rooms       map[string]*GameRoom
	AllSessions map[string]*Session
	Store       *store.RedisStore
	mu          sync.RWMutex
}

type RoomInfo struct {
	RoomID string `json:"room_id"`
	IsFull bool   `json:"is_full"`
}

// 新建一个房间管理器，负责管理所有房间和玩家会话，并与 Redis 存储交互以实现状态持久化和恢复
func NewRoomManager(redisStore *store.RedisStore) *RoomManager {
	return &RoomManager{
		Rooms:       make(map[string]*GameRoom),
		AllSessions: make(map[string]*Session),
		Store:       redisStore,
		mu:          sync.RWMutex{},
	}
}

func (rm *RoomManager) RestoreFromStore(ctx context.Context, nodeID int) error {
	return nil
}

// 从redis中恢复房间（等待状态的房间）状态，利用C++节点ID进行过滤，确保只恢复属于当前节点的房间
func (rm *RoomManager) ReassignWaitingRoomsFromNode(deadNodeID int) (moved int, skipped int) {
	if deadNodeID <= 0 {
		return 0, 0
	}

	rm.mu.RLock()
	targetRooms := make([]*GameRoom, 0)
	for _, r := range rm.Rooms {
		r.mu.Lock()
		shouldMove := r.NodeID == deadNodeID && r.Status == RoomWaiting
		r.mu.Unlock()
		if shouldMove {
			targetRooms = append(targetRooms, r)
		}
	}
	rm.mu.RUnlock()

	for _, r := range targetRooms {
		node, err := rpc.Mgr.GetNodeByRoomID(r.ID)
		if err != nil {
			log.Printf("[Failover] 房间 %s 重分配失败：找不到可用节点 err=%v", r.ID, err)
			skipped++
			continue
		}

		r.mu.Lock()
		if r.Status != RoomWaiting || r.NodeID != deadNodeID {
			r.mu.Unlock()
			skipped++
			continue
		}
		oldNodeID := r.NodeID
		r.NodeID = node.ID
		r.mu.Unlock()

		log.Printf("[Failover] 房间 %s 节点重分配: %d -> %d", r.ID, oldNodeID, node.ID)
		rm.persistRoomSnapshot(r)
		moved++
	}

	return moved, skipped
}

// 创建房间
func (rm *RoomManager) CreateRoom(s *Session) (string, bool) {
	rm.mu.Lock()
	defer rm.mu.Unlock()

	var roomID string
	for {
		roomID = generateRoomID()
		if _, exists := rm.Rooms[roomID]; !exists {
			break
		}
	}

	node, err := rpc.Mgr.GetNodeByRoomID(roomID)
	if err != nil {
		s.SendResponse("create_room_res", protocol.CodeCppRPCError, "算力集群暂不可用", nil)
		return "", false
	}

	//新建房间
	newRoom := &GameRoom{
		ID:           roomID,
		Status:       RoomWaiting,
		Sessions:     make(map[string]*Session),
		NodeID:       node.ID,
		ReadyPlayers: make(map[string]bool),
	}
	rm.Rooms[roomID] = newRoom

	//加入玩家，并在加入前给房间加锁
	newRoom.mu.Lock()
	s.RoomID = roomID
	newRoom.Sessions[s.Player.ID] = s
	newRoom.ReadyPlayers[s.Player.ID] = false
	newRoom.mu.Unlock()

	log.Printf("房间 [%s] 已创建, 玩家 [%s] 加入, 预分配至 C++ 节点 [%s]", roomID, s.Player.ID, node.Addr)
	s.SendResponse("create_room_res", protocol.CodeSuccess, "房间创建成功", map[string]string{"room_id": roomID})
	rm.persistSessionBinding(s.Player.ID, roomID)
	rm.persistRoomSnapshot(newRoom)

	return roomID, true
}

// 加入房间
func (rm *RoomManager) JoinRoom(roomID string, s *Session) (bool, string) {
	rm.mu.Lock()
	room, exists := rm.Rooms[roomID]
	if !exists {
		s.SendResponse("join_room_res", protocol.CodeRoomNotExist, "房间不存在,请检查房间编号", nil)
		rm.mu.Unlock()
		return false, "Room does not exist"
	}
	rm.mu.Unlock()

	room.mu.Lock()

	if _, ok := room.Sessions[s.Player.ID]; ok {
		s.SendResponse("join_room_res", protocol.CodeSendDuplicate, "你已经在房间里了", nil)
		room.mu.Unlock()
		return false, "Duplicate cmd message"
	}

	if room.Status != RoomWaiting {
		s.SendResponse("join_room_res", protocol.CodeRoomFull, "房间已开始或已结束", nil)
		room.mu.Unlock()
		return false, "Room started or finished"
	}

	if len(room.Sessions) >= configs.MaxPlayersPerRoom {
		s.SendResponse("join_room_res", protocol.CodeRoomFull, "房间已满", nil)
		room.mu.Unlock()
		return false, "Room is full"
	}

	s.RoomID = roomID
	room.Sessions[s.Player.ID] = s
	room.ReadyPlayers[s.Player.ID] = false // 初始化为未准备
	room.mu.Unlock()

	s.SendResponse("join_room_res", protocol.CodeSuccess, "成功加入房间", nil)
	room.BroadcastToRoom("join_room", protocol.CodeSuccess, fmt.Sprintf("玩家%s加入了房间", s.Player.ID), nil)
	rm.persistSessionBinding(s.Player.ID, roomID)
	rm.persistRoomSnapshot(room)

	return true, ""
}

// 离开房间
func (rm *RoomManager) LeaveRoom(roomID string, s *Session) {
	rm.mu.RLock()
	r, ok := rm.Rooms[roomID]
	rm.mu.RUnlock()

	if !ok {
		s.SendResponse("leave_room_res", protocol.CodeRoomNotExist, "房间不存在", nil)
		return
	}

	r.mu.Lock()
	delete(r.Sessions, s.Player.ID)
	delete(r.ReadyPlayers, s.Player.ID)
	remainingCount := len(r.Sessions)
	r.mu.Unlock()

	s.RoomID = ""
	rm.deleteSessionBinding(s.Player.ID)

	if remainingCount == 0 {
		rm.DeleteRoom(roomID)
	} else {
		msg := "Player Left! (playerID: " + s.Player.ID + ")"
		rm.Rooms[roomID].BroadcastToRoom(msg, protocol.CodeSuccess, fmt.Sprintf("玩家%s已离开", s.Player.ID), nil)

		r.mu.Lock()
		if r.Status == RoomPlaying {
			r.Status = RoomFinished
			r.mu.Unlock()
			rm.Rooms[roomID].BroadcastToRoom("room_status_change", protocol.CodeSuccess, "游戏因玩家离开而停止", nil)
			rm.persistRoomSnapshot(r)
		} else {
			r.mu.Unlock()
			rm.persistRoomSnapshot(r)
		}
	}
}

// 删除房间
func (rm *RoomManager) DeleteRoom(roomID string) {
	log.Printf("开始销毁房间")
	rm.mu.Lock()
	r, exists := rm.Rooms[roomID]
	if !exists {
		rm.mu.Unlock()
		return
	}

	status := r.Status
	targetNodeID := r.NodeID

	delete(rm.Rooms, roomID)
	rm.mu.Unlock()
	rm.deleteRoomSnapshot(roomID)

	if status != RoomWaiting {
		node, ok := rpc.Mgr.GetNodeByID(targetNodeID)
		if ok {
			go func() {
				ctx, cancel := context.WithTimeout(context.Background(), time.Second*3)
				defer cancel()
				resp, err := rpc.CallDestroyRoom(ctx, node.Client, roomID)
				if err != nil {
					log.Printf("[Room] 战斗中销毁失败, RoomID: %s, Error: %v", roomID, err)
				}

				if resp.Code != 0 {
					log.Printf("[Room] C++ 逻辑删除失败: %s (Code: %d)", resp.Message, resp.Code)
				} else {
					log.Printf("[Room] ✅ C++ 成功清理房间: %s", roomID)
				}
			}()
		}
	} else {
		log.Printf("[Room] 房间 %s 在等待中销毁，无需通知 C++", roomID)
	}
}

// 获取房间列表
func (rm *RoomManager) GetRooms() []RoomInfo {
	rm.mu.RLock()
	defer rm.mu.RUnlock()

	roomList := make([]RoomInfo, 0, len(rm.Rooms))

	for id, r := range rm.Rooms {
		r.mu.Lock()
		full := len(r.Sessions) >= 2
		r.mu.Unlock()

		roomList = append(roomList, RoomInfo{
			RoomID: id,
			IsFull: full,
		})
	}

	return roomList
}

// 生成房间ID，6位数字字符串，理论上支持100万房间，实际使用中会有重复概率，但可以接受，之后会再新建房间时候进行检查
func generateRoomID() string {
	return fmt.Sprintf("%06d", rand.Intn(1000000))
}

// 查找玩家会话
func (rm *RoomManager) FindSession(uid string) (*Session, bool) {
	rm.mu.RLock()
	defer rm.mu.RUnlock()
	s, ok := rm.AllSessions[uid]
	return s, ok
}

// 注册玩家会话到对话管理器中，供后续查找和管理使用
func (rm *RoomManager) RegisterSession(uid string, s *Session) {
	rm.mu.Lock()
	defer rm.mu.Unlock()
	rm.AllSessions[uid] = s
}

// 清除断线玩家
func (rm *RoomManager) CleanUpIfStillOffline(uid string, roomID string) {
	rm.mu.Lock()

	s, exists := rm.AllSessions[uid]
	if !exists || s.Conn != nil {
		rm.mu.Unlock()
		return
	}

	log.Printf("玩家 [%s] 超时未重连，执行彻底清理", uid)
	delete(rm.AllSessions, uid)

	rm.mu.Unlock()
	if roomID != "" {
		rm.LeaveRoom(roomID, s)
	}
}

// 生成一个 ReconnectResponse 来帮助玩家重新连接，返回房间，房间状态和当前房间内玩家
func (r *GameRoom) GetSnapshot(playerID string) *protocol.ReconnectResponse {
	r.mu.Lock()
	defer r.mu.Unlock()

	players := make([]model.Player, 0, len(r.Sessions))
	for _, session := range r.Sessions {
		players = append(players, *session.Player)
	}

	resp := &protocol.ReconnectResponse{
		RoomID:    r.ID,
		RoomState: int(r.Status),
		Players:   players,
	}

	resp.GameData = json.RawMessage(`{"status": "waiting_cpp_integration"}`)

	return resp
}

// 返回用户的房间内玩家列表，供前端展示用，包含玩家ID和昵称等基本信息，不包含敏感数据
func (r *GameRoom) GetPlayersInRoom() []model.Player {
	r.mu.Lock()
	defer r.mu.Unlock()

	players := make([]model.Player, 0, len(r.Sessions))
	for _, session := range r.Sessions {
		players = append(players, *session.Player)
	}
	return players
}

// 同步房间状态给玩家
func (rm *RoomManager) SyncRoomState(s *Session) {
	r := rm.Rooms[s.RoomID]
	if r == nil {
		s.SendResponse("reconnect_error", 404, "房间已销毁", nil)
		return
	}

	snapshot := r.GetSnapshot(s.Player.ID)

	s.SendResponse("reconnect_res", protocol.CodeSuccess, "状态恢复成功", snapshot)

	r.BroadcastToRoom("player_reconnect", protocol.CodeSuccess, fmt.Sprintf("玩家 %s 重新连接", s.Player.ID), map[string]interface{}{
		"uid": s.Player.ID,
	})
}

// 保存房间快照，利用房间GameRoom里的数据
func (rm *RoomManager) persistRoomSnapshot(r *GameRoom) {
	if rm.Store == nil || r == nil {
		return
	}

	snapshot := rm.buildRoomSnapshot(r)
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	if err := rm.Store.SaveRoomSnapshot(ctx, snapshot); err != nil {
		log.Printf("[Redis] 保存房间快照失败 room=%s err=%v", r.ID, err)
	}
}

// 构建快照，将房间内的玩家列表、准备状态、选择的宝可梦等信息打包成一个结构体，供持久化存储使用
func (rm *RoomManager) buildRoomSnapshot(r *GameRoom) store.RoomSnapshot {
	r.mu.Lock()
	defer r.mu.Unlock()

	players := make([]string, 0, len(r.Sessions))
	readyPlayers := make(map[string]bool, len(r.ReadyPlayers))
	selectedPokemon := make(map[string][]map[string]interface{}, len(r.Sessions))

	for playerID, session := range r.Sessions {
		players = append(players, playerID)
		readyPlayers[playerID] = r.ReadyPlayers[playerID]

		copiedTeam := make([]map[string]interface{}, 0, len(session.SelectedPokemon))
		for _, pokemon := range session.SelectedPokemon {
			copiedPokemon := make(map[string]interface{}, len(pokemon))
			for key, value := range pokemon {
				copiedPokemon[key] = value
			}
			copiedTeam = append(copiedTeam, copiedPokemon)
		}
		selectedPokemon[playerID] = copiedTeam
	}

	return store.RoomSnapshot{
		RoomID:          r.ID,
		Status:          int(r.Status),
		NodeID:          r.NodeID,
		Players:         players,
		ReadyPlayers:    readyPlayers,
		SelectedPokemon: selectedPokemon,
		UpdatedAt:       time.Now().Unix(),
	}
}

// 保存会话绑定信息，调用SaveSessionBinding
func (rm *RoomManager) persistSessionBinding(userID, roomID string) {
	if rm.Store == nil || userID == "" || roomID == "" {
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	if err := rm.Store.SaveSessionBinding(ctx, userID, roomID); err != nil {
		log.Printf("[Redis] 保存会话绑定失败 user=%s room=%s err=%v", userID, roomID, err)
	}
}

// 删除会话绑定信息，调用DeleteSessionBinding
func (rm *RoomManager) deleteSessionBinding(userID string) {
	if rm.Store == nil || userID == "" {
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	if err := rm.Store.DeleteSessionBinding(ctx, userID); err != nil {
		log.Printf("[Redis] 删除会话绑定失败 user=%s err=%v", userID, err)
	}
}

// 删除房间快照，调用DeleteRoomSnapshot
func (rm *RoomManager) deleteRoomSnapshot(roomID string) {
	if rm.Store == nil || roomID == "" {
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	if err := rm.Store.DeleteRoomSnapshot(ctx, roomID); err != nil {
		log.Printf("[Redis] 删除房间快照失败 room=%s err=%v", roomID, err)
	}
}
