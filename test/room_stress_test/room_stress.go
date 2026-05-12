package main

import (
	"encoding/json"
	"fmt"
	"log"
	"math/rand"
	"os"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

const (
	serverAddr        = "ws://127.0.0.1:8080/ws?uid=%d"
	roomCountPerRound = 48 // 每轮固定 48 个房间
	playersPerRoom    = 2
	actionTimeout     = 15 * time.Second // 增加到15s，适应RPC重试机制
	minActionPause    = 350 * time.Millisecond
	maxActionPause    = 900 * time.Millisecond
	minStartGameDelay = 1 * time.Second // 同一组内：建房完成后到开局的等待
	maxStartGameDelay = 3 * time.Second
	minGroupInterval  = 2 * time.Second // 组间等待（不短）
	maxGroupInterval  = 5 * time.Second
	minCreateLeadWait = 2 * time.Second // 开始创建下一组房间前的随机等待
	maxCreateLeadWait = 4 * time.Second
	minRoundDuration  = 65 * time.Second
	connectTimeout    = 5 * time.Second
	maxInboxBuffer    = 512
	cleanupWaitDelay  = 1200 * time.Millisecond
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
	roomID      string
	owner       *clientState
	guest       *clientState
	startResult int32 // 0 = pending, 1 = success, -1 = failed
}

type RoundStats struct {
	TotalRooms   int
	SuccessCount int
	FailedCount  int
	Duration     time.Duration
}

func main() {
	rand.Seed(time.Now().UnixNano())

	maxClients := roomCountPerRound * playersPerRoom
	clients := make([]*clientState, 0, maxClients)
	log.Printf("准备一轮压测：%d 个房间、每房间 %d 人、共 %d 个客户端", roomCountPerRound, playersPerRoom, maxClients)
	for i := 0; i < maxClients; i++ {
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

	log.Printf("========== 第 1 轮压测开始 ==========")
	stats := runMultiRoomStartScenario(clients)

	log.Printf("\n========== 第 1 轮压测结果统计 ==========")
	log.Printf("总房间数: %d", stats.TotalRooms)
	log.Printf("成功启动: %d", stats.SuccessCount)
	log.Printf("启动失败: %d", stats.FailedCount)
	log.Printf("启动耗时: %.1f 秒", stats.Duration.Seconds())
	log.Printf("=======================================\n")

	os.Exit(0)
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

func runMultiRoomStartScenario(clients []*clientState) RoundStats {
	startTime := time.Now()
	roomCount := roomCountPerRound
	perm := rand.Perm(len(clients))
	log.Printf("按组串行执行 %d 个房间：每组=create+join+start，组间随机等待", roomCount)

	sessions := make([]*roomSession, 0, roomCount)
	successCount := 0
	failedCount := 0

	for i := 0; i < roomCount; i++ {
		owner := clients[perm[i*2]]
		guest := clients[perm[i*2+1]]

		// 等待随机时间后开始创建下一组房间
		randomPauseBetween(minCreateLeadWait, maxCreateLeadWait)

		roomID := owner.createRoom()
		if roomID == "" {
			log.Printf("第 %d 个房间创建失败", i+1)
			failedCount++
			continue
		}

		if !guest.joinRoom(roomID) {
			log.Printf("房间 %s 加人失败", roomID)
			owner.leaveRoom()
			failedCount++
			continue
		}

		rs := &roomSession{roomID: roomID, owner: owner, guest: guest, startResult: 0}
		sessions = append(sessions, rs)
		log.Printf("已准备房间 %d/%d，room_id=%s，owner=%d，guest=%d", i+1, roomCount, roomID, owner.uid, guest.uid)

		// 同组内：稍作随机等待后立即开始该房间
		randomPauseBetween(minStartGameDelay, maxStartGameDelay)
		if rs.owner.startGameExpectingRoom(rs.roomID) {
			rs.startResult = 1
			successCount++
		} else {
			rs.startResult = -1
			failedCount++
		}

		log.Printf("完成房间 %d/%d 的 create+join+start 组", i+1, roomCount)
		randomPauseBetween(minGroupInterval, maxGroupInterval)
	}

	if len(sessions) == 0 {
		log.Printf("没有成功准备任何房间")
		return RoundStats{TotalRooms: 0, SuccessCount: 0, FailedCount: 0, Duration: time.Since(startTime)}
	}

	if elapsed := time.Since(startTime); elapsed < minRoundDuration {
		extraWait := minRoundDuration - elapsed
		log.Printf("本轮已完成但总时长不足 %.0fs，补充等待 %.1fs", minRoundDuration.Seconds(), extraWait.Seconds())
		time.Sleep(extraWait)
	}

	time.Sleep(cleanupWaitDelay)

	// 清理房间
	log.Printf("清理房间...")
	cleanupOrder := rand.Perm(len(sessions))
	for _, idx := range cleanupOrder {
		rs := sessions[idx]
		randomPause()
		rs.guest.leaveRoom()
		randomPause()
		rs.owner.leaveRoom()
	}

	duration := time.Since(startTime)
	return RoundStats{
		TotalRooms:   len(sessions),
		SuccessCount: successCount,
		FailedCount:  failedCount,
		Duration:     duration,
	}
}

func randomPause() {
	randomPauseBetween(minActionPause, maxActionPause)
}

func randomPauseBetween(minDelay, maxDelay time.Duration) {
	if maxDelay <= minDelay {
		time.Sleep(minDelay)
		return
	}
	delta := time.Duration(rand.Int63n(int64(maxDelay - minDelay)))
	time.Sleep(minDelay + delta)
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
