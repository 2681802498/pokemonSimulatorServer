package handler

import (
	"encoding/json"
	"go-server/internal/engine"
	"go-server/internal/protocol"
	"go-server/internal/room"
	"log"
	"log/slog"
	"strconv"
	"time"
)

func InitGameRouters(router *Router, rm *room.RoomManager, ep *engine.EnginePool) {

	router.Register("cluster_status", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 cluster_status 指令")
		ep.StatusResponse(s)
	})

	router.Register("create_room", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 CreateRoom 指令")
		rm.CreateRoom(s) // 内部生成随机房号并 SendResponse
	})

	router.Register("join_room", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 JoinRoom 指令")
		var joinData struct {
			RoomID string `json:"room_id"`
		}
		json.Unmarshal(req.Data, &joinData)
		rm.JoinRoom(joinData.RoomID, s)
	})

	router.Register("leave_room", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {

		roomID := s.RoomID // 记录一下
		l.Info("处理 LeaveRoom 指令", "room_id", roomID)

		rm.LeaveRoom(roomID, s)

		// 给离开的人回一个确认
		s.SendResponse("leave_room_res", protocol.CodeSuccess, "已退出房间", nil)
	})

	router.Register("list_rooms", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 ListRooms 指令")
		rooms := rm.GetRooms()
		s.SendResponse("list_rooms_res", protocol.CodeSuccess, "获取列表成功", rooms)
	})

	router.Register("match", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 Match 指令")
		room.MatchQueue <- s
		s.SendResponse("match_res", protocol.CodeSuccess, "已进入匹配队列", nil)
	})

	router.Register("start_game", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 StartGame 指令")
		rm.StartGameForSession(s)
	})

	router.Register("select_pokemon", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 SelectPokemon 指令", "room_id", s.RoomID)
		rm.HandleSelectPokemonAndPersist(s, req.Data)
	})

	router.Register("battle_action", func(s *room.Session, req *protocol.GameRequest, l *slog.Logger) {
		l.Info("处理 BattleAction 指令", "room_id", s.RoomID)
		rm.HandleBattleActionAndPersist(s, req.Data)
	})
}

type HandlerFunc func(s *room.Session, req *protocol.GameRequest, l *slog.Logger)

type Router struct {
	handlers map[string]HandlerFunc
}

func NewRouter() *Router {
	return &Router{handlers: make(map[string]HandlerFunc)}
}

// Register 注册指令处理器
func (r *Router) Register(cmd string, handler HandlerFunc) {
	r.handlers[cmd] = handler
}

// Dispatch 解析并分发消息
func (r *Router) Dispatch(s *room.Session, payload []byte) {
	reqID := "req-" + strconv.FormatInt(time.Now().UnixNano(), 36)
	l := slog.With("req_id", reqID, "player_id", s.Player.ID)

	var req protocol.GameRequest
	if err := json.Unmarshal(payload, &req); err != nil {
		log.Printf("JSON 解析失败: %v, 用户: %s", err, s.Player.ID)
		return
	}

	l.Info("收到指令", "cmd", req.Cmd)
	// 根据指令找到对应的处理函数
	if handler, ok := r.handlers[req.Cmd]; ok {
		handler(s, &req, l)
	} else {
		l.Warn("收到未知指令", "cmd", req.Cmd)
		s.SendResponse(req.Cmd+"_res", protocol.CodeSendInvalid, "未知指令", nil)
	}
}
