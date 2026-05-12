package room

import (
	"encoding/json"
	"go-server/internal/model"
	"go-server/internal/protocol"
	"log"
	"log/slog"

	"github.com/gorilla/websocket"
)

type Session struct {
	Player          *model.Player            // 关联的玩家对象
	RoomID          string                   // 当前所在的房间ID
	Conn            *websocket.Conn          // 底层网络连接
	Send            chan []byte              // 待发送的消息队列（写缓冲）
	SelectedPokemon []map[string]interface{} // 玩家选择的6个宝可梦
}

// NewSession 创建一个新的会话
func NewSession(p *model.Player, conn *websocket.Conn) *Session {
	return &Session{
		Player: p,
		Conn:   conn,
		Send:   make(chan []byte, 256), // 缓冲区大小按需调整
	}
}

func (s *Session) WritePump() {
	currentConn := s.Conn

	log.Printf("Session [%s] 的 WritePump 启动", s.Player.ID)

	defer func() {
		log.Printf("Session [%s] 的 WritePump 退出", s.Player.ID)
		currentConn.Close()
	}()

	for {
		select {
		case message, ok := <-s.Send:
			if !ok {
				return
			}

			if s.Conn != currentConn {
				log.Printf("检测到连接已更新，旧 WritePump [%s] 自动退出", s.Player.ID)
				return
			}

			err := currentConn.WriteMessage(websocket.TextMessage, message)
			if err != nil {
				log.Printf("写入失败: %v", err)
				return
			}
		}
	}
}

func (s *Session) SendResponse(cmd string, code int, msg string, data interface{}) {
	resp := protocol.GameResponse{
		Cmd:  cmd,
		Code: code,
		Msg:  msg,
		Data: data,
	}

	bytes, err := json.Marshal(resp)
	if err != nil {
		log.Printf("JSON Marshal Error: %v", err)
		return
	}

	// 这里的 s.Send 是你在 WritePump 里监听的那个 channel
	s.Send <- bytes
}

func (r *GameRoom) BroadcastToRoom(cmd string, code int, msg string, data interface{}) {
	if r == nil {
		slog.Warn("广播目标房间为空，跳过发送", "cmd", cmd)
		return
	}

	resp := protocol.GameResponse{
		Cmd:  cmd,
		Code: code,
		Msg:  msg,
		Data: data,
	}

	bytes, err := json.Marshal(resp)
	if err != nil {
		slog.Error("广播消息序列化失败", "cmd", cmd, "error", err)
		return
	}

	r.mu.Lock() // 使用读锁，允许并发读取玩家列表
	defer r.mu.Unlock()

	for _, session := range r.Sessions {
		select {
		case session.Send <- bytes:
		default:
			// 某个玩家连接卡住了，缓冲区已满
			slog.Warn("玩家发送缓冲区已满，丢弃广播消息",
				"player_id", session.Player.ID,
				"cmd", cmd)
		}
	}
}
