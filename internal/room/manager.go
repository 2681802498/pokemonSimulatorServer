package room

import (
	"context"
	crand "crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"go-server/configs"
	"go-server/internal/engine"
	"go-server/internal/model"
	"go-server/internal/protocol"
	"go-server/internal/rpc"
	"go-server/internal/store"
	"hash/crc32"
	"log"
	"sort"
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
	EngineConn   *engine.EngineInstance
	mu           sync.Mutex
}

type RoomManager struct {
	Rooms       map[string]*GameRoom
	AllSessions map[string]*Session
	Store       *store.RedisStore
	EngineConn  *engine.EngineInstance
	mu          sync.RWMutex
}

type RoomInfo struct {
	RoomID string `json:"room_id"`
	IsFull bool   `json:"is_full"`
}

// 新建一个房间管理器，负责管理所有房间和玩家会话，并从 Redis 恢复 C++ 侧写入的房间状态
func NewRoomManager(redisStore *store.RedisStore, engineConn *engine.EngineInstance) *RoomManager {
	return &RoomManager{
		Rooms:       make(map[string]*GameRoom),
		AllSessions: make(map[string]*Session),
		Store:       redisStore,
		EngineConn:  engineConn,
		mu:          sync.RWMutex{},
	}
}

func (rm *RoomManager) RestoreFromStore(ctx context.Context, nodeID int) error {
	if rm.Store == nil {
		return nil
	}

	snapshots, err := rm.Store.ListRoomSnapshots(ctx)
	if err != nil {
		return fmt.Errorf("list room snapshots failed: %w", err)
	}

	loaded := 0
	for _, snapshot := range snapshots {
		if snapshot.RoomID == "" {
			continue
		}
		if nodeID > 0 && snapshot.NodeID != nodeID {
			continue
		}

		rm.restoreRoomSnapshot(snapshot)
		loaded++
	}

	if moved, skipped := rm.ReassignRunningRoomsFromUnavailableNodes(); moved > 0 || skipped > 0 {
		log.Printf("[Redis] 启动恢复后完成房间重分配 moved=%d skipped=%d", moved, skipped)
	}

	log.Printf("[Redis] 已从 Redis 恢复房间快照 count=%d", loaded)
	return nil
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

	//新建房间
	newRoom := &GameRoom{
		ID:           roomID,
		Status:       RoomWaiting,
		Sessions:     make(map[string]*Session),
		NodeID:       0, // 先设置为0，后续会分配
		ReadyPlayers: make(map[string]bool),
		EngineConn:   rm.EngineConn,
	}
	rm.Rooms[roomID] = newRoom

	//加入玩家，并在加入前给房间加锁
	newRoom.mu.Lock()
	s.RoomID = roomID
	newRoom.Sessions[s.Player.ID] = s
	newRoom.ReadyPlayers[s.Player.ID] = false
	newRoom.mu.Unlock()

	log.Printf("房间 %s 创建成功，创建者: %s，等待开始游戏后分配节点", roomID, s.Player.ID)
	s.SendResponse("create_room_res", protocol.CodeSuccess, "房间创建成功", map[string]string{"room_id": roomID})

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

	if remainingCount == 0 {
		rm.DeleteRoom(roomID)
	} else {
		msg := "Player Left! (playerID: " + s.Player.ID + ")"
		r.BroadcastToRoom(msg, protocol.CodeSuccess, fmt.Sprintf("玩家%s已离开", s.Player.ID), nil)

		r.mu.Lock()
		if r.Status == RoomPlaying {
			r.Status = RoomFinished
			r.mu.Unlock()
			r.BroadcastToRoom("room_status_change", protocol.CodeSuccess, "游戏因玩家离开而停止", nil)
		} else {
			r.mu.Unlock()
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

	if status != RoomWaiting {
		// 向目标节点发送销毁房间的请求
		rm.sendDestroyRoomRequest(roomID, targetNodeID, "房间销毁")
	} else {
		log.Printf("[Room] 房间 %s 在等待中销毁，无需通知 C++", roomID)
	}
}

// sendDestroyRoomRequest 向指定节点发送销毁房间的请求，异步执行。
// 用于常规房间删除或房间迁移后销毁原节点的房间副本。
func (rm *RoomManager) sendDestroyRoomRequest(roomID string, nodeID int, reason string) {
	if rm.EngineConn == nil {
		log.Printf("[Room] 销毁房间 %s 失败: EngineConn 未初始化", roomID)
		return
	}

	node, err := rm.EngineConn.GetNodeByPodIndex(nodeID)
	if err != nil {
		log.Printf("[Room] 销毁房间 %s 失败: 节点 %d 不可用, 原因=%s, Error=%v", roomID, nodeID, reason, err)
		return
	}

	go func() {
		defer func() {
			if r := recover(); r != nil {
				log.Printf("[Room] panic recovered during DestroyRoom for %s: %v", roomID, r)
			}
		}()

		ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
		defer cancel()

		resp, err := rpc.CallDestroyRoom(ctx, node.GetClient(), roomID)
		if err != nil {
			log.Printf("[Room] 销毁房间 %s 失败 (节点=%d, 原因=%s): %v", roomID, nodeID, reason, err)
			return
		}

		if resp == nil {
			log.Printf("[Room] 销毁房间 %s 失败 (节点=%d, 原因=%s): resp == nil", roomID, nodeID, reason)
			return
		}

		if resp.Code != 0 {
			log.Printf("[Room] C++ 销毁房间 %s 失败 (节点=%d, 原因=%s): %s (Code: %d)", roomID, nodeID, reason, resp.Message, resp.Code)
		} else {
			log.Printf("[Room] ✅ C++ 成功清理房间: %s (节点=%d, 原因=%s)", roomID, nodeID, reason)
		}
	}()
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

// 生成房间ID：使用加密随机数，避免多 Pod 使用相同 math/rand 序列导致房号重复。
func generateRoomID() string {
	buf := make([]byte, 8)
	if _, err := crand.Read(buf); err != nil {
		// 极端情况下退回到时间戳，避免创建失败
		return fmt.Sprintf("room-%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(buf)
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
func (rm *RoomManager) CleanUpIfStillOffline(uid string) {
	rm.mu.Lock()

	s, exists := rm.AllSessions[uid]
	if !exists || s.Conn != nil {
		rm.mu.Unlock()
		return
	}

	roomID := s.RoomID

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

// 从房间快照里恢复玩家会话，主要是为了在玩家断线重连时能够恢复到之前的状态，包含房间ID、房间状态和当前房间内玩家列表等信息
func (rm *RoomManager) restoreRoomSnapshot(snapshot store.RoomSnapshot) {
	r := &GameRoom{
		ID:           snapshot.RoomID,
		Status:       RoomStatus(snapshot.Status),
		NodeID:       snapshot.NodeID,
		Sessions:     make(map[string]*Session),
		ReadyPlayers: make(map[string]bool),
		EngineConn:   rm.EngineConn,
	}

	playerIDs := make(map[string]struct{}, len(snapshot.Players))
	for _, playerID := range snapshot.Players {
		if playerID == "" {
			continue
		}
		playerIDs[playerID] = struct{}{}
	}
	for playerID := range snapshot.ReadyPlayers {
		if playerID == "" {
			continue
		}
		playerIDs[playerID] = struct{}{}
	}
	for playerID := range snapshot.SelectedPokemon {
		if playerID == "" {
			continue
		}
		playerIDs[playerID] = struct{}{}
	}

	for playerID := range playerIDs {
		session := &Session{
			Player: &model.Player{ID: playerID},
			RoomID: snapshot.RoomID,
			Send:   make(chan []byte, 256),
		}
		if team, ok := snapshot.SelectedPokemon[playerID]; ok {
			session.SelectedPokemon = copySelectedPokemon(team)
		}
		r.Sessions[playerID] = session
		r.ReadyPlayers[playerID] = snapshot.ReadyPlayers[playerID]
		rm.AllSessions[playerID] = session
	}

	rm.mu.Lock()
	rm.Rooms[r.ID] = r
	rm.mu.Unlock()

	log.Printf("[Redis] 已恢复房间快照 room=%s status=%d node=%d players=%d", r.ID, r.Status, r.NodeID, len(r.Sessions))
}

func copySelectedPokemon(team []map[string]interface{}) []map[string]interface{} {
	if len(team) == 0 {
		return nil
	}

	cloned := make([]map[string]interface{}, 0, len(team))
	for _, pokemon := range team {
		copiedPokemon := make(map[string]interface{}, len(pokemon))
		for key, value := range pokemon {
			copiedPokemon[key] = value
		}
		cloned = append(cloned, copiedPokemon)
	}

	return cloned
}

func (rm *RoomManager) ReassignRunningRoomsFromUnavailableNodes() (moved int, skipped int) {
	if rm.EngineConn == nil {
		return 0, 0
	}

	// 获取当前所有节点状态，找出健康的节点，并记录每个 pod 的 server_id
	status := rm.EngineConn.GetStatus()
	healthy := make(map[int]struct{}, len(status.Nodes))
	podServerIDs := make(map[int]string, len(status.Nodes))
	for _, node := range status.Nodes {
		podServerIDs[node.PodIndex] = node.ServerID
		if node.Healthy {
			healthy[node.PodIndex] = struct{}{}
		}
	}
	log.Printf("[Failover] 当前健康节点: %v", func() []int {
		ids := make([]int, 0, len(healthy))
		for id := range healthy {
			ids = append(ids, id)
		}
		sort.Ints(ids)
		return ids
	}())

	if len(healthy) == 0 {
		log.Printf("[Failover] 无可用节点，无法重分配房间")
		return 0, 0
	}

	rm.mu.RLock()
	targetRooms := make([]*GameRoom, 0)
	for _, r := range rm.Rooms {
		r.mu.Lock()
		nodeID := r.NodeID
		r.mu.Unlock()

		// 如果 node 不在健康集合里，加入待迁移列表
		if _, ok := healthy[nodeID]; !ok {
			targetRooms = append(targetRooms, r)
			continue
		}

		// 如果 Redis 可用，尝试读取 snapshot 中的 server_id 与当前 pod 的 server_id 比对
		if rm.Store != nil {
			ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
			snap, err := rm.Store.LoadRoomSnapshot(ctx, r.ID)
			cancel()
			if err == nil && snap != nil && snap.ServerID != "" {
				if podSID, ok := podServerIDs[nodeID]; ok {
					if podSID != snap.ServerID {
						// snapshot 中记录的 server_id 与当前 pod 的 server_id 不一致，触发迁移
						targetRooms = append(targetRooms, r)
						continue
					}
				}
			}
		}
	}
	rm.mu.RUnlock()

	log.Printf("[Failover] 重分配房间为: %v", func() []string {
		ids := make([]string, 0, len(targetRooms))
		for _, r := range targetRooms {
			ids = append(ids, r.ID)
		}
		return ids
	}())

	for _, r := range targetRooms {
		r.mu.Lock()
		currentNodeID := r.NodeID
		currentStatus := r.Status
		r.mu.Unlock()
		log.Printf("[Failover] 准备重分配房间 %s, 当前NodeID=%d, Status=%d", r.ID, currentNodeID, currentStatus)

		// 为每个受影响的房间重新选择一个健康节点，排除掉当前不可用的节点
		newNode, err := rm.pickHealthyNodeForRoom(r.ID, r.NodeID)
		if err != nil {
			log.Printf("[Failover] 房间 %s 重分配失败：找不到可用节点 err=%v", r.ID, err)
			skipped++
			continue
		}
		log.Printf("[Failover] 房间 %s 选中新的目标节点: PodIndex=%d PodName=%s", r.ID, newNode.PodIndex, newNode.PodName)

		r.mu.Lock()
		oldNodeID := r.NodeID
		roomStatus := r.Status
		r.mu.Unlock()

		// 对于进行中的房间，需要从 Redis 读取快照并向新 C++ 节点发送 CreateRoom 请求重新创建房间
		if roomStatus == RoomPlaying {
			if rm.Store == nil {
				log.Printf("[Failover] 房间 %s (RoomPlaying) 无法恢复: Redis Store 未初始化", r.ID)
				skipped++
				continue
			}

			ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
			snapshot, err := rm.Store.LoadRoomSnapshot(ctx, r.ID)
			cancel()
			if err != nil {
				log.Printf("[Failover] 房间 %s 从 Redis 读取快照失败: %v", r.ID, err)
				skipped++
				continue
			}

			// 将快照转换为 initJSON 并发送给新 C++ 节点
			snapshotJSON, err := json.Marshal(snapshot)
			if err != nil {
				log.Printf("[Failover] 房间 %s 序列化快照失败: %v", r.ID, err)
				skipped++
				continue
			}
			log.Printf("[Failover] 房间 %s 即将向新节点 %d 发送 CreateRoom, snapshotSize=%d", r.ID, newNode.PodIndex, len(snapshotJSON))

			ctx, cancel = context.WithTimeout(context.Background(), 5*time.Second)
			resp, err := rpc.CallCreateRoom(ctx, newNode.GetClient(), r.ID, string(snapshotJSON))
			cancel()
			if err != nil {
				log.Printf("[Failover] 房间 %s 向新节点 %d 重新创建失败: %v", r.ID, newNode.PodIndex, err)
				skipped++
				continue
			}

			if resp.Code != 0 {
				log.Printf("[Failover] 房间 %s 在新节点 %d 创建失败 (Code: %d, Message: %s)", r.ID, newNode.PodIndex, resp.Code, resp.Message)
				skipped++
				continue
			}

			r.mu.Lock()
			r.NodeID = newNode.PodIndex
			r.mu.Unlock()

			// 同步更新 Redis 中的 snapshot，使 NodeID 与内存中的值保持一致
			ctx, cancel = context.WithTimeout(context.Background(), 3*time.Second)
			updatedSnapshot := store.RoomSnapshot{
				RoomID:          r.ID,
				Status:          int(roomStatus),
				NodeID:          newNode.PodIndex,
				ServerID:        newNode.ServerID,
				Players:         snapshot.Players,
				ReadyPlayers:    snapshot.ReadyPlayers,
				SelectedPokemon: snapshot.SelectedPokemon,
				UpdatedAt:       time.Now().Unix(),
			}
			if saveErr := rm.Store.SaveRoomSnapshot(ctx, updatedSnapshot); saveErr != nil {
				log.Printf("[Failover] 房间 %s 更新 Redis 快照失败: %v", r.ID, saveErr)
			}
			cancel()

			log.Printf("[Failover] ✅ 房间 %s (RoomPlaying) 成功重分配: %d -> %d (已向新 C++ 节点重新创建)", r.ID, oldNodeID, newNode.PodIndex)

			// 销毁原节点中的房间副本，释放资源
			rm.sendDestroyRoomRequest(r.ID, oldNodeID, "房间迁移")

			moved++
		} else {
			// 对于等待中的房间，只需更新内存中的 NodeID，无需通知 C++
			log.Printf("[Failover] 房间 %s (RoomWaiting/RoomFinshed) 无需重新分配房间", r.ID)
			moved++
		}
	}

	return moved, skipped
}

// 从可用节点中为房间选择一个健康的节点，排除掉指定的 Pod 索引（通常是不可用的节点），如果没有可用节点则返回错误
func (rm *RoomManager) pickHealthyNodeForRoom(roomID string, excludePodIndex int) (*engine.EngineNode, error) {
	if rm.EngineConn == nil {
		return nil, fmt.Errorf("engine connection is nil")
	}

	// 建立一个健康节点白名单，排除掉不可用的节点和指定的排除节点
	status := rm.EngineConn.GetStatus()
	healthyIndices := make([]int, 0, len(status.Nodes))
	for _, node := range status.Nodes {
		if !node.Healthy || node.PodIndex == excludePodIndex { // 排除不可用节点和指定的排除节点
			continue
		}
		healthyIndices = append(healthyIndices, node.PodIndex)
	}

	if len(healthyIndices) == 0 {
		return nil, fmt.Errorf("no healthy nodes available")
	}

	// 对健康节点进行排序，确保一致性哈希算法的稳定性，减少同一房间频繁迁移的情况
	sort.Ints(healthyIndices)
	// 使用一致性哈希算法在健康节点中选择一个节点，确保同一房间尽可能分配到同一节点上，减少跨节点迁移的情况
	chosen := healthyIndices[int(crc32.ChecksumIEEE([]byte(roomID)))%len(healthyIndices)]
	return rm.EngineConn.GetNodeByPodIndex(chosen)
}
