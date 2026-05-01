package main

import (
	"encoding/json"
	"fmt"
	"log"
	"math/rand"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

const (
	serverAddr       = "ws://localhost:8080/ws?uid=%d"
	roomsPerRound    = 24
	playersPerRoom   = 2
	roundCount       = 3
	actionTimeout    = 5 * time.Second
	minActionPause   = 20 * time.Millisecond
	maxActionPause   = 80 * time.Millisecond
	connectTimeout   = 5 * time.Second
	maxInboxBuffer   = 512
	cleanupWaitDelay = 1200 * time.Millisecond
)

type gameResponse struct {
	Cmd  string          `json:"cmd"`
	Code int             `json:"code"`
	Msg  string          `json:"msg"`
	Data json.RawMessage `json:"data"`
}

type clientState struct {
	uid   int
	conn  *websocket.Conn
	inbox chan gameResponse
	mu    sync.Mutex
	room  string
	close sync.Once
}

type roomSession struct {
	roomID string
	owner  *clientState
	guest  *clientState
}

func main() {
	rand.Seed(time.Now().UnixNano())

	clients := make([]*clientState, 0, roomsPerRound*playersPerRoom)
	log.Printf("准备压测：每轮 %d 个房间、每房间 %d 人、共 %d 个客户端，目标总房间数 %d（应超过单个 C++ 服务器容量 8，触发多实例）",
		roomsPerRound, playersPerRoom, roomsPerRound*playersPerRoom, roomsPerRound)
	for i := 0; i < roomsPerRound*playersPerRoom; i++ {
		c, err := newClient(10000 + i)
		if err != nil {
			log.Fatalf("创建连接失败: %v", err)
		}
		clients = append(clients, c)
	}

	defer func() {
		for _, c := range clients {
			c.closeConn()
		}
	}()

	for round := 1; round <= roundCount; round++ {
		log.Printf("========== 第 %d 轮多房间开始测试 ==========", round)
		runMultiRoomStartScenario(round, clients)
		flushAll(clients)
		randomPause()
	}

	log.Println("多房间开始测试完成")
}

func newClient(uid int) (*clientState, error) {
	url := fmt.Sprintf(serverAddr, uid)
	dialer := websocket.Dialer{HandshakeTimeout: connectTimeout}
	conn, _, err := dialer.Dial(url, nil)
	if err != nil {
		return nil, err
	}

	c := &clientState{
		uid:   uid,
		conn:  conn,
		inbox: make(chan gameResponse, maxInboxBuffer),
	}

	go c.readLoop()
	log.Printf("用户 %d 已连接", uid)
	return c, nil
}

func (c *clientState) readLoop() {
	defer close(c.inbox)

	for {
		_, payload, err := c.conn.ReadMessage()
		if err != nil {
			log.Printf("用户 %d 读循环退出: %v", c.uid, err)
			return
		}

		var resp gameResponse
		if err := json.Unmarshal(payload, &resp); err != nil {
			log.Printf("用户 %d 响应解析失败: %v, payload=%s", c.uid, err, string(payload))
			continue
		}

		c.inbox <- resp
	}
}

func (c *clientState) closeConn() {
	c.close.Do(func() {
		_ = c.conn.Close()
	})
}

func (c *clientState) setRoom(roomID string) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.room = roomID
}

func (c *clientState) getRoom() string {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.room
}

func (c *clientState) clearRoom() {
	c.setRoom("")
}

func (c *clientState) send(cmd string, data interface{}) error {
	payload := map[string]interface{}{
		"cmd":  cmd,
		"data": data,
	}
	bytes, err := json.Marshal(payload)
	if err != nil {
		return err
	}
	return c.conn.WriteMessage(websocket.TextMessage, bytes)
}

func (c *clientState) waitFor(expected ...string) (gameResponse, bool) {
	allowed := make(map[string]struct{}, len(expected))
	for _, cmd := range expected {
		allowed[cmd] = struct{}{}
	}

	timer := time.NewTimer(actionTimeout)
	defer timer.Stop()

	for {
		select {
		case resp, ok := <-c.inbox:
			if !ok {
				return gameResponse{}, false
			}

			if _, ok := allowed[resp.Cmd]; ok {
				return resp, true
			}

			log.Printf("用户 %d 旁路消息: cmd=%s code=%d msg=%s", c.uid, resp.Cmd, resp.Code, resp.Msg)

		case <-timer.C:
			return gameResponse{}, false
		}
	}
}

func (c *clientState) createRoom() string {
	if err := c.send("create_room", map[string]interface{}{}); err != nil {
		log.Printf("用户 %d 创建房间发送失败: %v", c.uid, err)
		return ""
	}

	resp, ok := c.waitFor("create_room_res")
	if !ok {
		log.Printf("用户 %d 未收到创建房间响应", c.uid)
		return ""
	}

	log.Printf("用户 %d 创建房间响应: code=%d msg=%s", c.uid, resp.Code, resp.Msg)
	if resp.Code != 0 {
		return ""
	}

	var data struct {
		RoomID string `json:"room_id"`
	}
	if len(resp.Data) > 0 {
		if err := json.Unmarshal(resp.Data, &data); err != nil {
			log.Printf("用户 %d 解析 room_id 失败: %v, data=%s", c.uid, err, string(resp.Data))
			return ""
		}
	}
	if data.RoomID == "" {
		log.Printf("用户 %d 创建房间返回空 room_id", c.uid)
		return ""
	}

	c.setRoom(data.RoomID)
	return data.RoomID
}

func (c *clientState) joinRoom(roomID string) bool {
	if err := c.send("join_room", map[string]interface{}{"room_id": roomID}); err != nil {
		log.Printf("用户 %d 加入房间发送失败: %v", c.uid, err)
		return false
	}

	resp, ok := c.waitFor("join_room_res")
	if !ok {
		log.Printf("用户 %d 未收到加入房间响应", c.uid)
		return false
	}

	log.Printf("用户 %d 加入房间响应: code=%d msg=%s", c.uid, resp.Code, resp.Msg)
	if resp.Code == 0 {
		c.setRoom(roomID)
		return true
	}
	return false
}

func (c *clientState) startGameExpectingRoom(roomID string) bool {
	if roomID == "" {
		return false
	}

	if err := c.send("start_game", map[string]interface{}{}); err != nil {
		log.Printf("用户 %d 开始游戏发送失败: %v", c.uid, err)
		return false
	}

	resp, ok := c.waitFor("game_started", "game_start_error")
	if !ok {
		log.Printf("用户 %d 未收到开始游戏广播(房间 %s)", c.uid, roomID)
		return false
	}

	log.Printf("房间 %s 用户 %d 开始游戏结果: cmd=%s code=%d msg=%s", roomID, c.uid, resp.Cmd, resp.Code, resp.Msg)
	return resp.Code == 0
}

func (c *clientState) leaveRoom() {
	roomID := c.getRoom()
	if err := c.send("leave_room", map[string]interface{}{}); err != nil {
		log.Printf("用户 %d 离开房间发送失败: %v", c.uid, err)
		return
	}

	resp, ok := c.waitFor("leave_room_res")
	if !ok {
		log.Printf("用户 %d 未收到离开房间响应(当前 room=%s)", c.uid, roomID)
		c.clearRoom()
		return
	}

	log.Printf("用户 %d 离开房间响应: code=%d msg=%s", c.uid, resp.Code, resp.Msg)
	c.clearRoom()
}

func runMultiRoomStartScenario(round int, clients []*clientState) {
	perm := rand.Perm(len(clients))
	sessions := make([]*roomSession, 0, roomsPerRound)
	log.Printf("第 %d 轮：开始创建 %d 个房间，预期总房间数将达到 %d，超过单个 C++ 服务器上限 8", round, roomsPerRound, roomsPerRound)

	for i := 0; i < roomsPerRound; i++ {
		owner := clients[perm[i*2]]
		guest := clients[perm[i*2+1]]

		roomID := owner.createRoom()
		if roomID == "" {
			log.Printf("第 %d 轮：第 %d 个房间创建失败", round, i+1)
			continue
		}

		randomPause()
		if !guest.joinRoom(roomID) {
			log.Printf("第 %d 轮：房间 %s 加人失败", round, roomID)
			continue
		}

		log.Printf("第 %d 轮：已准备房间 %d/%d，room_id=%s，owner=%d，guest=%d", round, i+1, roomsPerRound, roomID, owner.uid, guest.uid)
		sessions = append(sessions, &roomSession{roomID: roomID, owner: owner, guest: guest})
		randomPause()
	}

	if len(sessions) == 0 {
		log.Printf("第 %d 轮：没有成功准备任何房间", round)
		return
	}

	log.Printf("第 %d 轮：开始并发触发 %d 个房间启动", round, len(sessions))
	if len(sessions) < roomsPerRound {
		log.Printf("第 %d 轮：实际准备房间数 %d，小于目标 %d，可能有个别创建/加入失败", round, len(sessions), roomsPerRound)
	}
	startOrder := rand.Perm(len(sessions))
	var wg sync.WaitGroup
	wg.Add(len(startOrder))
	for _, idx := range startOrder {
		rs := sessions[idx]
		go func(s *roomSession) {
			defer wg.Done()
			s.owner.startGameExpectingRoom(s.roomID)
		}(rs)
	}
	wg.Wait()

	time.Sleep(cleanupWaitDelay)
	cleanupOrder := rand.Perm(len(sessions))
	for _, idx := range cleanupOrder {
		rs := sessions[idx]
		randomPause()
		rs.guest.leaveRoom()
		randomPause()
		rs.owner.leaveRoom()
	}
}

func randomPause() {
	delta := time.Duration(rand.Int63n(int64(maxActionPause - minActionPause)))
	time.Sleep(minActionPause + delta)
}

func flushAll(clients []*clientState) {
	for _, c := range clients {
		for {
			select {
			case resp, ok := <-c.inbox:
				if !ok {
					goto nextClient
				}
				log.Printf("用户 %d 清理残留消息: cmd=%s code=%d msg=%s", c.uid, resp.Cmd, resp.Code, resp.Msg)
			default:
				goto nextClient
			}
		}
	nextClient:
	}
}

