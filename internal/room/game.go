package room

import (
	"context"
	"encoding/json"
	"fmt"
	"go-server/internal/protocol"
	"go-server/internal/rpc"
	"hash/crc32"
	"log"
	"sort"
	"time"
)

func (r *GameRoom) StartGame(s *Session) {
	r.mu.Lock()
	if r.Status != RoomWaiting {
		log.Printf("[Room] 房间 %s 当前状态 %v，无法开始游戏", r.ID, r.Status)
		r.mu.Unlock()
		return
	}

	sessions := make([]*Session, 0, len(r.Sessions))
	for _, session := range r.Sessions {
		sessions = append(sessions, session)
	}

	r.Status = RoomPlaying
	r.mu.Unlock()

	if len(sessions) < 2 {
		r.handleStartError(r.ID, "房间人数不足 2 人，无法初始化对战")
		return
	}

	initJSON, err := buildBattleInitJSON(r.ID, sessions)
	if err != nil {
		r.handleStartError(r.ID, fmt.Sprintf("构造 init_json 失败: %v", err))
		return
	}
	node, err := r.EngineConn.GetNodeForRoom(r.ID)
	if err != nil {
		r.handleStartError(r.ID, fmt.Sprintf("分配 C++ 引擎节点失败: %v", err))
		return
	}
	r.mu.Lock()
	r.NodeID = node.PodIndex
	targetNodeID := r.NodeID
	r.mu.Unlock()

	log.Printf("[Room] 房间 %s 准备同步至 C++ 节点 [%d]...", r.ID, targetNodeID)

	go func() {
		// 尝试使用目标节点，失败则故障转移
		node, err := r.EngineConn.GetNodeByPodIndex(targetNodeID)
		if err != nil {
			log.Printf("[Room] 房间 %s 的目标节点 [%d] 不可用，尝试故障转移: %v", r.ID, targetNodeID, err)
			// 故障转移：重新使用一致性哈希分配节点
			node, err = r.EngineConn.GetNodeForRoom(r.ID)
			if err != nil {
				r.handleStartError(r.ID, fmt.Sprintf("无可用 C++ 引擎节点: %v", err))
				return
			}
			log.Printf("[Room] 房间 %s 故障转移至新节点成功", r.ID)
		}

		// RPC 调用，带重试机制（最多3次，总超时15s）
		var resp *rpc.RPCResponse
		var rpcErr error
		maxRetries := 3

		for attempt := 1; attempt <= maxRetries; attempt++ {
			ctx, cancel := context.WithTimeout(context.Background(), time.Second*10)
			resp, rpcErr = rpc.CallCreateRoom(ctx, node.GetClient(), r.ID, initJSON)
			cancel()

			if rpcErr == nil && resp != nil && resp.Code == 0 {
				// 成功
				log.Printf("[Room] ✅ 房间 %s 已在 C++ 引擎中初始化成功 (第 %d 次尝试)", r.ID, attempt)
				r.BroadcastToRoom("game_started", protocol.CodeSuccess, "游戏正式开始", nil)
				return
			}

			if attempt < maxRetries {
				log.Printf("[Room] 房间 %s RPC 第 %d 次尝试失败，准备重试...", r.ID, attempt)
				time.Sleep(time.Second) // 重试前等待1秒
			}
		}

		// 所有重试都失败
		if rpcErr != nil {
			r.handleStartError(r.ID, fmt.Sprintf("C++ 引擎调用失败: %v", rpcErr))
		} else if resp != nil && resp.Code != 0 {
			r.handleStartError(r.ID, fmt.Sprintf("C++ 引擎响应失败: code=%d, msg=%s", resp.Code, resp.Message))
		} else {
			r.handleStartError(r.ID, "C++ 引擎调用返回异常")
		}
	}()
}

func (r *GameRoom) handleStartError(roomID string, reason string) {
	log.Printf("[Room] ❌ 房间 %s 启动失败: %s", roomID, reason)
	r.mu.Lock()
	r.Status = RoomWaiting
	r.mu.Unlock()

	r.BroadcastToRoom("game_start_error", protocol.CodeCppRPCError, reason, nil)
}

func buildBattleInitJSON(roomID string, sessions []*Session) (string, error) {
	if len(sessions) < 2 {
		return "", fmt.Errorf("sessions must be at least 2")
	}

	sorted := append([]*Session(nil), sessions...)
	sort.Slice(sorted, func(i, j int) bool {
		return sorted[i].Player.ID < sorted[j].Player.ID
	})

	seed := int(crc32.ChecksumIEEE([]byte(roomID)))

	// 转换玩家选择的宝可梦为 interface{} 数组
	pokemonA := make([]interface{}, 0, len(sorted[0].SelectedPokemon))
	for _, poke := range sorted[0].SelectedPokemon {
		pokemonA = append(pokemonA, poke)
	}

	pokemonB := make([]interface{}, 0, len(sorted[1].SelectedPokemon))
	for _, poke := range sorted[1].SelectedPokemon {
		pokemonB = append(pokemonB, poke)
	}

	payload := map[string]interface{}{
		"seed": seed,
		"side_a": map[string]interface{}{
			"name":    "Player-" + sorted[0].Player.ID,
			"pokemon": pokemonA,
		},
		"side_b": map[string]interface{}{
			"name":    "Player-" + sorted[1].Player.ID,
			"pokemon": pokemonB,
		},
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return "", err
	}

	return string(data), nil
}

func (rm *RoomManager) HandleBattleAction(s *Session, rawAction json.RawMessage) {
	if s.RoomID == "" {
		s.SendResponse("battle_action_res", protocol.CodeRoomNotExist, "当前不在房间中", nil)
		return
	}

	rm.mu.RLock()
	r, exists := rm.Rooms[s.RoomID]
	rm.mu.RUnlock()
	if !exists || r == nil {
		s.SendResponse("battle_action_res", protocol.CodeRoomNotExist, "房间不存在", nil)
		return
	}

	r.forwardBattleAction(s, rawAction)
}

func (rm *RoomManager) HandleBattleActionAndPersist(s *Session, rawAction json.RawMessage) {
	rm.HandleBattleAction(s, rawAction)

	if s.RoomID == "" {
		return
	}

	rm.mu.RLock()
	r, exists := rm.Rooms[s.RoomID]
	rm.mu.RUnlock()
	if !exists || r == nil {
		return
	}
}

func (rm *RoomManager) StartGameForSession(s *Session) {
	if s.RoomID == "" {
		s.SendResponse("start_game_res", protocol.CodeRoomNotExist, "当前不在房间中", nil)
		return
	}

	rm.mu.RLock()
	r, exists := rm.Rooms[s.RoomID]
	rm.mu.RUnlock()
	if !exists || r == nil {
		s.SendResponse("start_game_res", protocol.CodeRoomNotExist, "房间不存在", nil)
		return
	}

	r.StartGame(s)
}

func (rm *RoomManager) HandleSelectPokemonAndPersist(s *Session, rawData json.RawMessage) {
	if s.RoomID == "" {
		s.SendResponse("select_pokemon_res", protocol.CodeRoomNotExist, "当前不在房间中", nil)
		return
	}

	rm.mu.RLock()
	r, exists := rm.Rooms[s.RoomID]
	rm.mu.RUnlock()
	if !exists || r == nil {
		s.SendResponse("select_pokemon_res", protocol.CodeRoomNotExist, "房间不存在", nil)
		return
	}

	r.HandleSelectPokemon(s, rawData)
}

func (r *GameRoom) forwardBattleAction(s *Session, rawAction json.RawMessage) {
	r.mu.Lock()
	status := r.Status
	targetNodeID := r.NodeID
	_, inRoom := r.Sessions[s.Player.ID]
	r.mu.Unlock()

	if !inRoom {
		s.SendResponse("battle_action_res", protocol.CodePlayerNotInRoom, "你不在该房间中", nil)
		return
	}

	if status != RoomPlaying {
		s.SendResponse("battle_action_res", protocol.CodeRoomStarted, "房间当前不在对战中", nil)
		return
	}

	if len(rawAction) == 0 {
		s.SendResponse("battle_action_res", protocol.CodeDataInvalid, "action 不能为空", nil)
		return
	}

	if !json.Valid(rawAction) {
		s.SendResponse("battle_action_res", protocol.CodeDataInvalid, "action 必须是合法 JSON", nil)
		return
	}

	node, err := r.EngineConn.GetNodeByPodIndex(targetNodeID)
	if err != nil {
		s.SendResponse("battle_action_res", protocol.CodeCppRPCError, "目标 C++ 节点不可用", nil)
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	resp, err := rpc.CallSendCommand(ctx, node.GetClient(), r.ID, s.Player.ID, string(rawAction))
	if err != nil {
		s.SendResponse("battle_action_res", protocol.CodeCppRPCError, fmt.Sprintf("C++ 调用失败: %v", err), nil)
		return
	}

	if resp.Code != 0 {
		s.SendResponse("battle_action_res", int(resp.Code), resp.Message, nil)
		return
	}

	var turnResult interface{}
	if err := json.Unmarshal([]byte(resp.Message), &turnResult); err != nil {
		turnResult = map[string]interface{}{"raw": resp.Message}
	}

	s.SendResponse("battle_action_res", protocol.CodeSuccess, "动作已提交", turnResult)
	r.BroadcastToRoom("battle_state_push", protocol.CodeSuccess, "对战状态更新", turnResult)
}

// HandleSelectPokemon 处理玩家宝可梦选择
func (r *GameRoom) HandleSelectPokemon(s *Session, rawData json.RawMessage) {
	r.mu.Lock()
	status := r.Status
	_, inRoom := r.Sessions[s.Player.ID]
	r.mu.Unlock()

	if !inRoom {
		s.SendResponse("select_pokemon_res", protocol.CodePlayerNotInRoom, "你不在该房间中", nil)
		return
	}

	if status != RoomWaiting {
		s.SendResponse("select_pokemon_res", protocol.CodeRoomStarted, "房间已开始游戏，无法选择宝可梦", nil)
		return
	}

	if len(rawData) == 0 {
		s.SendResponse("select_pokemon_res", protocol.CodeDataInvalid, "pokemon 数据不能为空", nil)
		return
	}

	var req protocol.SelectPokemonRequest
	if err := json.Unmarshal(rawData, &req); err != nil {
		s.SendResponse("select_pokemon_res", protocol.CodeDataInvalid, fmt.Sprintf("JSON 解析失败: %v", err), nil)
		return
	}

	if len(req.Pokemon) != 6 {
		s.SendResponse("select_pokemon_res", protocol.CodeDataInvalid, "必须选择 6 个宝可梦", nil)
		return
	}

	// 将选择的宝可梦转换为 map 存储
	s.SelectedPokemon = make([]map[string]interface{}, 0, len(req.Pokemon))
	for _, poke := range req.Pokemon {
		pokeMap := map[string]interface{}{
			"speciesID": poke.SpeciesID,
			"level":     poke.Level,
			"ability":   poke.Ability,
			"moves":     poke.Moves,
		}
		s.SelectedPokemon = append(s.SelectedPokemon, pokeMap)
	}

	// 更新房间 ReadyPlayers 状态
	r.mu.Lock()
	r.ReadyPlayers[s.Player.ID] = true
	readyCount := 0
	for _, ready := range r.ReadyPlayers {
		if ready {
			readyCount++
		}
	}
	allPlayersReady := len(r.Sessions) == 2 && readyCount == 2
	r.mu.Unlock()

	s.SendResponse("select_pokemon_res", protocol.CodeSuccess, "宝可梦选择成功", nil)
	r.BroadcastToRoom("player_ready", protocol.CodeSuccess, fmt.Sprintf("玩家 %s 已准备好", s.Player.ID), nil)

	// 如果两个玩家都准备好，自动开始游戏
	if allPlayersReady {
		log.Printf("[Room] 房间 %s 两个玩家都已选择宝可梦，准备开始游戏", r.ID)
		r.StartGame(nil)
	}
}
