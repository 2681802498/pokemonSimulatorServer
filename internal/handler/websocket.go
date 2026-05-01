package handler

import (

	// 这里的路径取决于你 go.mod 里的名字

	"go-server/configs"
	"go-server/internal/model"
	"go-server/internal/protocol"
	"go-server/internal/room"
	"log"
	"net/http"
	"time"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

func HandleWS(w http.ResponseWriter, r *http.Request, router *Router, rm *room.RoomManager) {
	uid := r.URL.Query().Get("uid")
	if uid == "" {
		log.Println("连接拒绝：缺少 uid")
		http.Error(w, "Missing uid", 400)
		return
	}

	// 升级 HTTP 连接为 WebSocket
	currentConn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("升级失败: %v", err)
		return
	}

	defer currentConn.Close()

	var s *room.Session
	oldSession, exists := rm.FindSession(uid)

	if exists {
		if oldSession.Conn != nil {
			if oldSession.Conn != nil {
				log.Printf("玩家 [%s] 触发顶号：尝试断开旧连接", uid)
				oldSession.Conn.Close()
			}
		} else {
			log.Printf("玩家 [%s] 尝试重连", uid)
		}
		s = oldSession
		s.Conn = currentConn
	} else {
		p := &model.Player{ID: uid}
		s = room.NewSession(p, currentConn)
		rm.RegisterSession(uid, s)
	}

	go s.WritePump()

	if exists {
		if s.RoomID != "" && rm.Rooms[s.RoomID] != nil {
			log.Printf("玩家 [%s] 重连后尝试恢复房间 [%s]", uid, s.RoomID)
			go rm.SyncRoomState(s)
		} else {
			log.Printf("玩家 [%s] 重连成功，但不在任何房间", uid)
			s.SendResponse("reconnect_success", protocol.CodeSuccess, "已重连至大厅", nil)
		}
	}

	defer func() {
		log.Printf("玩家 [%s] 的一个链路协程退出", s.Player.ID)

		if s.Conn == currentConn {
			s.Conn = nil
			log.Printf("玩家 [%s] 真正离线，进入保留期", s.Player.ID)
			go func(pID string, rID string) {
				time.Sleep(configs.ReconnectionTimeout)
				rm.CleanUpIfStillOffline(pID, rID)
			}(s.Player.ID, s.RoomID)
		} else {
			log.Printf("玩家 [%s] 链路已由新连接接管，旧协程退出", s.Player.ID)
		}
	}()

	log.Printf("玩家 [%s] 连接成功", s.Player.ID)

	//读循环
	for {
		_, message, err := currentConn.ReadMessage()
		if err != nil {
			log.Printf("读取失败: %v", err)
			break
		}

		router.Dispatch(s, message)
	}
}
