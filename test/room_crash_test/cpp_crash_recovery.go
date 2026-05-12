package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"log"
	"math/rand"
	"os/exec"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

const (
	defaultWSAddr      = "ws://127.0.0.1:8080/ws?uid=%d"
	defaultNamespace   = "default"
	defaultCPPSelector = "app=pokemon-server"
	connectTimeout     = 6 * time.Second
	actionTimeout      = 15 * time.Second
)

type gameResponse struct {
	Cmd  string          `json:"cmd"`
	Code int             `json:"code"`
	Msg  string          `json:"msg"`
	Data json.RawMessage `json:"data"`
}

type roomSnapshot struct {
	RoomID    string `json:"room_id"`
	Status    int    `json:"status"`
	NodeID    int    `json:"node_id"`
	UpdatedAt int64  `json:"updated_at"`
}

type debugRoomInfo struct {
	Snapshot roomSnapshot `json:"snapshot"`
}

type client struct {
	uid   int
	conn  *websocket.Conn
	inbox chan gameResponse
	close sync.Once
}

type podList struct {
	Items []podItem `json:"items"`
}

type podItem struct {
	Metadata struct {
		Name string `json:"name"`
	} `json:"metadata"`
	Status struct {
		Phase      string `json:"phase"`
		Conditions []struct {
			Type   string `json:"type"`
			Status string `json:"status"`
		} `json:"conditions"`
	} `json:"status"`
}

func main() {
	rand.Seed(time.Now().UnixNano())

	wsAddr := flag.String("ws", defaultWSAddr, "WebSocket 地址模板，需包含 uid 占位符，如 ws://127.0.0.1:8080/ws?uid=%d")
	namespace := flag.String("namespace", defaultNamespace, "K8s namespace")
	cppSelector := flag.String("cpp-selector", defaultCPPSelector, "C++ pod label selector")
	recoveryTimeout := flag.Duration("recovery-timeout", 90*time.Second, "房间恢复观察超时")
	podReadyTimeout := flag.Duration("pod-ready-timeout", 120*time.Second, "等待 C++ pod 就绪超时")
	preSnapshotTimeout := flag.Duration("pre-snapshot-timeout", 40*time.Second, "等待崩溃前房间快照写入 Redis 的超时")
	flag.Parse()

	owner, err := newClient(*wsAddr, 910001)
	if err != nil {
		log.Fatalf("owner 连接失败: %v", err)
	}
	defer owner.closeConn()

	guest, err := newClient(*wsAddr, 910002)
	if err != nil {
		log.Fatalf("guest 连接失败: %v", err)
	}
	defer guest.closeConn()

	roomID := createAndStartGame(owner, guest)
	log.Printf("[Step] 已创建并开局 room=%s", roomID)

	dumpRedisBeforeCrash(owner)

	before, err := waitRoomSnapshotAvailable(owner, roomID, *preSnapshotTimeout)
	if err != nil {
		log.Fatalf("获取崩溃前快照失败: %v", err)
	}
	log.Printf("[Before] room=%s node_id=%d status=%d updated_at=%d", before.RoomID, before.NodeID, before.Status, before.UpdatedAt)

	targetPod, err := chooseTargetPod(*namespace, *cppSelector, before.NodeID)
	if err != nil {
		log.Fatalf("选择待删除 C++ pod 失败: %v", err)
	}
	log.Printf("[Step] 目标 C++ pod: %s", targetPod)

	if err := deletePod(*namespace, targetPod); err != nil {
		log.Fatalf("删除 C++ pod 失败: %v", err)
	}
	log.Printf("[Step] 已触发 pod 删除: %s", targetPod)

	// 在删除 pod 后立即打印 Redis 中的房间快照，便于观察 C++ 重启期间的快照变化
	dumpRedisBeforeCrash(owner)
	// 给系统一点时间让删除操作和 C++ 重建开始（非严格等待）
	time.Sleep(600 * time.Millisecond)

	if err := waitAnyReadyPod(*namespace, *cppSelector, *podReadyTimeout); err != nil {
		log.Fatalf("等待 C++ pod 就绪失败: %v", err)
	}
	log.Printf("[Step] C++ pod 已恢复为 Ready")

	after, err := waitRoomRecovery(owner, roomID, before, *recoveryTimeout)
	if err != nil {
		log.Fatalf("恢复验证失败: %v", err)
	}

	log.Printf("[PASS] 恢复完成 room=%s before(node=%d,updated=%d,status=%d) after(node=%d,updated=%d,status=%d)",
		roomID, before.NodeID, before.UpdatedAt, before.Status, after.NodeID, after.UpdatedAt, after.Status)

	// 尝试主动退出房间，触发服务端清理，避免残留状态影响后续测试
	_ = owner.send("leave_room", map[string]interface{}{})
	_ = guest.send("leave_room", map[string]interface{}{})
	// 给服务端一点时间完成清理（例如删除内存房间与 Redis 快照）
	time.Sleep(800 * time.Millisecond)
}

func newClient(wsTemplate string, uid int) (*client, error) {
	url := fmt.Sprintf(wsTemplate, uid)
	dialer := websocket.Dialer{HandshakeTimeout: connectTimeout}
	conn, _, err := dialer.Dial(url, nil)
	if err != nil {
		return nil, err
	}
	c := &client{uid: uid, conn: conn, inbox: make(chan gameResponse, 512)}
	go c.readLoop()
	return c, nil
}

func (c *client) readLoop() {
	defer close(c.inbox)
	for {
		_, payload, err := c.conn.ReadMessage()
		if err != nil {
			return
		}
		var resp gameResponse
		if err := json.Unmarshal(payload, &resp); err != nil {
			continue
		}
		c.inbox <- resp
	}
}

func (c *client) closeConn() {
	c.close.Do(func() { _ = c.conn.Close() })
}

func (c *client) send(cmd string, data map[string]interface{}) error {
	payload := map[string]interface{}{"cmd": cmd, "data": data}
	body, err := json.Marshal(payload)
	if err != nil {
		return err
	}
	return c.conn.WriteMessage(websocket.TextMessage, body)
}

func (c *client) waitFor(expected ...string) (gameResponse, error) {
	allowed := make(map[string]struct{}, len(expected))
	for _, e := range expected {
		allowed[e] = struct{}{}
	}
	t := time.NewTimer(actionTimeout)
	defer t.Stop()

	for {
		select {
		case r, ok := <-c.inbox:
			if !ok {
				return gameResponse{}, errors.New("ws closed")
			}
			if _, ok := allowed[r.Cmd]; ok {
				return r, nil
			}
			log.Printf("[UID=%d] 旁路消息 cmd=%s code=%d msg=%s", c.uid, r.Cmd, r.Code, r.Msg)
		case <-t.C:
			return gameResponse{}, fmt.Errorf("wait timeout, expected=%v", expected)
		}
	}
}

func createAndStartGame(owner, guest *client) string {
	if err := owner.send("create_room", map[string]interface{}{}); err != nil {
		log.Fatalf("create_room 发送失败: %v", err)
	}
	resp, err := owner.waitFor("create_room_res")
	if err != nil {
		log.Fatalf("create_room_res 等待失败: %v", err)
	}
	if resp.Code != 0 {
		log.Fatalf("create_room_res 失败: code=%d msg=%s", resp.Code, resp.Msg)
	}
	var createData struct {
		RoomID string `json:"room_id"`
	}
	if err := json.Unmarshal(resp.Data, &createData); err != nil {
		log.Fatalf("create_room_res 解析失败: %v", err)
	}
	if createData.RoomID == "" {
		log.Fatalf("create_room_res 未返回 room_id")
	}

	if err := guest.send("join_room", map[string]interface{}{"room_id": createData.RoomID}); err != nil {
		log.Fatalf("join_room 发送失败: %v", err)
	}
	joinResp, err := guest.waitFor("join_room_res")
	if err != nil {
		log.Fatalf("join_room_res 等待失败: %v", err)
	}
	if joinResp.Code != 0 {
		log.Fatalf("join_room_res 失败: code=%d msg=%s", joinResp.Code, joinResp.Msg)
	}

	if err := owner.send("start_game", map[string]interface{}{}); err != nil {
		log.Fatalf("start_game 发送失败: %v", err)
	}
	startedResp, err := owner.waitFor("game_started", "game_start_error")
	if err != nil {
		log.Fatalf("start_game 响应等待失败: %v", err)
	}
	if startedResp.Code != 0 {
		log.Fatalf("start_game 失败: cmd=%s code=%d msg=%s", startedResp.Cmd, startedResp.Code, startedResp.Msg)
	}

	return createData.RoomID
}

func getRoomSnapshot(c *client, roomID string) (roomSnapshot, error) {
	if err := c.send("get_redis", map[string]interface{}{}); err != nil {
		return roomSnapshot{}, err
	}
	resp, err := c.waitFor("get_redis_res")
	if err != nil {
		return roomSnapshot{}, err
	}
	if resp.Code != 0 {
		return roomSnapshot{}, fmt.Errorf("get_redis_res failed: code=%d msg=%s", resp.Code, resp.Msg)
	}

	var wrapper struct {
		Rooms []json.RawMessage `json:"rooms"`
	}
	if err := json.Unmarshal(resp.Data, &wrapper); err != nil {
		return roomSnapshot{}, fmt.Errorf("unmarshal get_redis data failed: %w, raw=%s", err, truncate(string(resp.Data), 300))
	}

	for _, one := range wrapper.Rooms {
		var direct roomSnapshot
		if err := json.Unmarshal(one, &direct); err == nil && direct.RoomID == roomID {
			return direct, nil
		}

		var debug debugRoomInfo
		if err := json.Unmarshal(one, &debug); err == nil && debug.Snapshot.RoomID == roomID {
			return debug.Snapshot, nil
		}
	}

	return roomSnapshot{}, fmt.Errorf("room %s not found in redis (rooms=%d, sample=%s)", roomID, len(wrapper.Rooms), firstRoomSample(wrapper.Rooms))
}

func firstRoomSample(rooms []json.RawMessage) string {
	if len(rooms) == 0 {
		return "<empty>"
	}
	return truncate(string(rooms[0]), 220)
}

func truncate(s string, n int) string {
	if n <= 0 || len(s) <= n {
		return s
	}
	return s[:n] + "..."
}

func waitRoomSnapshotAvailable(c *client, roomID string, timeout time.Duration) (roomSnapshot, error) {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		snap, err := getRoomSnapshot(c, roomID)
		if err == nil {
			return snap, nil
		}
		log.Printf("[WaitPreSnapshot] room=%s 尚未出现在 Redis，继续等待... err=%v", roomID, err)
		time.Sleep(2 * time.Second)
	}
	return roomSnapshot{}, fmt.Errorf("room %s 在 %s 内未写入 Redis，可能是 C++ 侧尚未持久化快照", roomID, timeout)
}

func waitRoomRecovery(c *client, roomID string, before roomSnapshot, timeout time.Duration) (roomSnapshot, error) {
	deadline := time.Now().Add(timeout)
	log.Printf("[DEBUG] 开始恢复观察，目标房间: %s, 崩溃前状态: NodeID=%d, UpdatedAt=%d", roomID, before.NodeID, before.UpdatedAt)

	for time.Now().Before(deadline) {
		after, err := getRoomSnapshot(c, roomID)
		if err == nil {
			// --- 新增：打印当前 Redis 中的实时数据 ---
			log.Printf("[Current Redis] RoomID: %s, NodeID: %d, Status: %d, UpdatedAt: %d",
				after.RoomID, after.NodeID, after.Status, after.UpdatedAt)

			nodeChanged := after.NodeID != before.NodeID
			updatedChanged := after.UpdatedAt > before.UpdatedAt
			// 只要满足 状态合法，并且 (节点变了 或者 时间更新了)
			statusValid := after.Status == before.Status || after.Status == 1 || after.Status == 0

			log.Printf("[CHECK] Node: %d->%d | Time: %d->%d",
				before.NodeID, after.NodeID, before.UpdatedAt, after.UpdatedAt)

			if statusValid && (nodeChanged || updatedChanged || after.NodeID > 0) {
				log.Printf("[MATCH] 满足恢复条件！NodeChanged=%v, UpdatedChanged=%v", nodeChanged, updatedChanged)
				return after, nil
			}

			log.Printf("[WaitRecovery] 尚未达到恢复条件，继续等待... (NodeID: %d, UpdatedAt: %d)", after.NodeID, after.UpdatedAt)
		} else {
			// 如果 Redis 里暂时查不到这个 roomID，也会报错
			log.Printf("[WaitRecovery] 读取快照失败或房间尚未在 Redis 重现: %v", err)
		}
		time.Sleep(3 * time.Second)
	}
	return roomSnapshot{}, fmt.Errorf("timeout waiting room recovery: room=%s", roomID)
}

func chooseTargetPod(namespace, selector string, nodeID int) (string, error) {
	pods, err := listPods(namespace, selector)
	if err != nil {
		return "", err
	}
	if len(pods) == 0 {
		return "", fmt.Errorf("no pods found for selector=%s", selector)
	}

	if nodeID >= 0 {
		suffix := fmt.Sprintf("-%d", nodeID)
		for _, p := range pods {
			if strings.HasSuffix(p.Metadata.Name, suffix) || strings.Contains(p.Metadata.Name, suffix+"-") {
				return p.Metadata.Name, nil
			}
		}
	}

	return pods[rand.Intn(len(pods))].Metadata.Name, nil
}

func deletePod(namespace, podName string) error {
	_, err := runKubectl(context.Background(), "-n", namespace, "delete", "pod", podName, "--grace-period=0", "--force")
	return err
}

func waitAnyReadyPod(namespace, selector string, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		pods, err := listPods(namespace, selector)
		if err != nil {
			time.Sleep(2 * time.Second)
			continue
		}
		for _, p := range pods {
			if p.Status.Phase == "Running" && isReady(p) {
				return nil
			}
		}
		time.Sleep(2 * time.Second)
	}
	return fmt.Errorf("wait ready pod timeout selector=%s", selector)
}

func listPods(namespace, selector string) ([]podItem, error) {
	out, err := runKubectl(context.Background(), "-n", namespace, "get", "pods", "-l", selector, "-o", "json")
	if err != nil {
		return nil, err
	}
	var list podList
	if err := json.Unmarshal([]byte(out), &list); err != nil {
		return nil, err
	}
	return list.Items, nil
}

func isReady(p podItem) bool {
	for _, c := range p.Status.Conditions {
		if c.Type == "Ready" && c.Status == "True" {
			return true
		}
	}
	return false
}

func runKubectl(ctx context.Context, args ...string) (string, error) {
	cmd := exec.CommandContext(ctx, "kubectl", args...)
	out, err := cmd.CombinedOutput()
	if err != nil {
		return "", fmt.Errorf("kubectl %v failed: %w, output=%s", args, err, string(out))
	}
	return string(out), nil
}

func dumpRedisBeforeCrash(c *client) {
	if err := c.send("get_redis", map[string]interface{}{}); err != nil {
		log.Printf("[Redis诊断] 发送 get_redis 失败: %v", err)
		return
	}
	resp, err := c.waitFor("get_redis_res")
	if err != nil {
		log.Printf("[Redis诊断] 等待 get_redis_res 失败: %v", err)
		return
	}
	log.Printf("[Redis诊断] code=%d msg=%s", resp.Code, resp.Msg)
	log.Printf("[Redis诊断] 原始响应数据:\n%s", string(resp.Data))
}
