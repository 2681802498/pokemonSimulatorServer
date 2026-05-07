package engine

import (
	"context"
	"fmt"
	"go-server/configs"
	"go-server/internal/rpc"
	"hash/crc32"
	"log/slog"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// EngineNode 表示一个具体的 StatefulSet Pod 节点及其长连接状态。
type EngineNode struct {
	PodIndex      int       `json:"pod_index"`
	PodName       string    `json:"pod_name"`
	Addr          string    `json:"addr"`
	Healthy       bool      `json:"healthy"`
	LastError     string    `json:"last_error,omitempty"`
	LastHeartbeat time.Time `json:"last_heartbeat,omitempty"`
	ActiveRooms   int32     `json:"active_rooms"`
	CPUUsage      float32   `json:"cpu_usage"`
	MemoryUsed    uint64    `json:"memory_used"`
	MaxCapacity   int32     `json:"max_capacity"`

	mu           sync.RWMutex
	conn         *grpc.ClientConn
	client       rpc.EngineClient
	reconnectMu  sync.Mutex
	reconnecting bool
}

// EngineNodeStatus 是对外暴露的节点状态快照。
type EngineNodeStatus struct {
	PodIndex      int       `json:"pod_index"`
	PodName       string    `json:"pod_name"`
	Addr          string    `json:"addr"`
	Healthy       bool      `json:"healthy"`
	LastError     string    `json:"last_error,omitempty"`
	LastHeartbeat time.Time `json:"last_heartbeat,omitempty"`
	ActiveRooms   int32     `json:"active_rooms"`
	CPUUsage      float32   `json:"cpu_usage"`
	MemoryUsed    uint64    `json:"memory_used"`
	MaxCapacity   int32     `json:"max_capacity"`
}

// EngineStatus 是引擎连接对外暴露的状态快照。
type EngineStatus struct {
	Total       int                `json:"total"`
	Healthy     int                `json:"healthy"`
	Unhealthy   int                `json:"unhealthy"`
	Nodes       []EngineNodeStatus `json:"nodes"`
	Replicas    int                `json:"replicas"`
	Namespace   string             `json:"namespace"`
	ServiceName string             `json:"service_name"`
}

// EngineInstance 负责维护 StatefulSet 中多个 C++ Pod 的长连接与健康检查。
//
// 说明：
// 1. 一个长连接对应一个 StatefulSet Pod（pod-0, pod-1, 等）；
// 2. 房间根据房间 ID 使用一致性哈希选择落地的 Pod；
// 3. 每个 Pod 有稳定的 DNS 地址：pokemon-server-{index}.{serviceName}.{namespace}.svc.cluster.local；
// 4. 健康检查确保所有 Pod 连接可用，断线自动重连。
type EngineInstance struct {
	mu               sync.RWMutex
	ctx              context.Context
	cancel           context.CancelFunc
	wg               sync.WaitGroup
	healthCheckEvery time.Duration
	heartbeatTimeout time.Duration
	dialTimeout      time.Duration

	replicas    int
	namespace   string
	serviceName string
	nodes       map[int]*EngineNode
}

// NewEngineInstance 创建并初始化引擎连接管理器。
//
// 流程：读取 StatefulSet 配置 -> 为每个 Pod 建立持久连接 -> 启动健康检查协程。
func NewEngineInstance() *EngineInstance {
	ctx, cancel := context.WithCancel(context.Background())

	replicas := getEnvInt("K8S_ENGINE_REPLICAS", 1)
	namespace := getEnvString("K8S_ENGINE_NAMESPACE", "default")
	serviceName := getEnvString("K8S_ENGINE_SERVICE", "pokemon-server")

	inst := &EngineInstance{
		ctx:              ctx,
		cancel:           cancel,
		healthCheckEvery: configs.CppHealthCheckInterval,
		heartbeatTimeout: 2 * time.Second,
		dialTimeout:      5 * time.Second,
		replicas:         replicas,
		namespace:        namespace,
		serviceName:      serviceName,
		nodes:            make(map[int]*EngineNode),
	}

	slog.Info("Engine 初始化配置", "replicas", replicas, "namespace", namespace, "service", serviceName)

	for i := 0; i < replicas; i++ {
		addr := inst.buildPodDNSAddr(i)
		if err := inst.connectAndRegisterPod(i, addr); err != nil {
			slog.Error("Pod 连接失败", "pod_index", i, "addr", addr, "error", err)
			continue
		}
	}

	inst.wg.Add(1)
	go inst.healthCheckLoop()

	slog.Info("Engine 已初始化", "replicas", replicas, "connected", len(inst.snapshotPodIndices()))
	return inst
}

// buildPodDNSAddr 构造 StatefulSet Pod 的 DNS 地址。
func (e *EngineInstance) buildPodDNSAddr(podIndex int) string {
	podName := fmt.Sprintf("%s-%d", e.serviceName, podIndex)
	port := getEnvString("K8S_ENGINE_PORT", strconv.Itoa(configs.CppStartPort))
	return fmt.Sprintf("%s.%s-headless.%s.svc.cluster.local:%s", podName, e.serviceName, e.namespace, port)
}

// GetNodeForRoom 根据房间 ID 使用一致性哈希返回对应的 EngineNode。
func (e *EngineInstance) GetNodeForRoom(roomID string) (*EngineNode, error) {
	if e.replicas <= 0 {
		return nil, fmt.Errorf("no replicas configured")
	}

	podIndex := e.hashRoom(roomID)
	node := e.getNode(podIndex)
	if node == nil {
		return nil, fmt.Errorf("pod %d not found", podIndex)
	}

	return node, nil
}

// GetNodeByPodIndex 根据 Pod 索引返回对应的 EngineNode。
func (e *EngineInstance) GetNodeByPodIndex(podIndex int) (*EngineNode, error) {
	node := e.getNode(podIndex)
	if node == nil {
		return nil, fmt.Errorf("pod %d not found", podIndex)
	}
	return node, nil
}

// hashRoom 使用房间 ID 和副本数进行一致性哈希。
func (e *EngineInstance) hashRoom(roomID string) int {
	h := crc32.ChecksumIEEE([]byte(roomID))
	return int(h) % e.replicas
}

// GetStatus 返回当前引擎连接状态快照。
func (e *EngineInstance) GetStatus() EngineStatus {
	indices := e.snapshotPodIndices()
	statuses := make([]EngineNodeStatus, 0, len(indices))
	healthy := 0

	for _, idx := range indices {
		node := e.getNode(idx)
		if node == nil {
			continue
		}
		s := node.snapshot()
		if s.Healthy {
			healthy++
		}
		statuses = append(statuses, s)
	}

	return EngineStatus{
		Total:       len(statuses),
		Healthy:     healthy,
		Unhealthy:   len(statuses) - healthy,
		Nodes:       statuses,
		Replicas:    e.replicas,
		Namespace:   e.namespace,
		ServiceName: e.serviceName,
	}
}

// Shutdown 停止健康检查协程，并关闭所有引擎连接。
func (e *EngineInstance) Shutdown() {
	e.cancel()
	e.wg.Wait()

	indices := e.snapshotPodIndices()
	for _, idx := range indices {
		node := e.getNode(idx)
		if node == nil {
			continue
		}

		if conn := node.closeConn(); conn != nil {
			rpc.Mgr.RemoveNode(idx)
			_ = conn.Close()
		}
	}

	e.mu.Lock()
	e.nodes = make(map[int]*EngineNode)
	e.mu.Unlock()

	slog.Info("Engine 已关闭")
}

// healthCheckLoop 按固定周期巡检所有引擎连接的心跳状态。
func (e *EngineInstance) healthCheckLoop() {
	defer e.wg.Done()

	ticker := time.NewTicker(e.healthCheckEvery)
	defer ticker.Stop()

	for {
		select {
		case <-e.ctx.Done():
			return
		case <-ticker.C:
			e.ensureConnected()
			e.checkHeartbeat()
		}
	}
}

// ensureConnected 对断开的 Pod 发起重连。
func (e *EngineInstance) ensureConnected() {
	for i := 0; i < e.replicas; i++ {
		node := e.getNode(i)
		if node == nil {
			continue
		}

		if node.isConnected() || node.isReconnecting() {
			continue
		}

		addr := node.addrSnapshot()
		if addr == "" {
			continue
		}

		if err := e.reconnect(i, addr); err != nil {
			slog.Debug("Pod 断开，已进入重连流程", "pod_index", i, "addr", addr, "error", err)
		}
	}
}

// checkHeartbeat 检查所有 Pod 的心跳，并在失败时尝试重连。
func (e *EngineInstance) checkHeartbeat() {
	for i := 0; i < e.replicas; i++ {
		node := e.getNode(i)
		if node == nil {
			continue
		}

		if err := e.checkSingleNode(i); err != nil {
			addr := node.addrSnapshot()
			slog.Warn("Pod 心跳失败，尝试重连", "pod_index", i, "addr", addr, "error", err)
			if addr == "" {
				continue
			}

			if recErr := e.reconnect(i, addr); recErr != nil {
				slog.Error("Pod 重连失败", "pod_index", i, "addr", addr, "error", recErr)
			}
		}
	}
}

// checkSingleNode 向指定 Pod 发起心跳请求，并刷新其状态信息。
func (e *EngineInstance) checkSingleNode(podIndex int) error {
	node := e.getNode(podIndex)
	if node == nil {
		return fmt.Errorf("pod %d not found", podIndex)
	}

	client := node.clientSnapshot()
	if client == nil {
		node.markUnhealthy(fmt.Errorf("pod not connected"))
		return fmt.Errorf("pod not connected")
	}

	ctx, cancel := context.WithTimeout(context.Background(), e.heartbeatTimeout)
	defer cancel()

	hb, err := rpc.CallGetHeartbeat(ctx, client)
	if err != nil {
		node.markUnhealthy(err)
		return err
	}

	node.markHealthy(hb)
	return nil
}

// reconnect 在心跳失败后重新建立指定 Pod 的连接。
func (e *EngineInstance) reconnect(podIndex int, addr string) error {
	node := e.getNode(podIndex)
	if node == nil {
		return fmt.Errorf("pod %d not found", podIndex)
	}

	node.reconnectMu.Lock()
	if node.reconnecting {
		node.reconnectMu.Unlock()
		return fmt.Errorf("reconnect in progress")
	}
	node.reconnecting = true
	node.reconnectMu.Unlock()

	if err := e.connectAndRegisterPod(podIndex, addr); err == nil {
		node.reconnectMu.Lock()
		node.reconnecting = false
		node.reconnectMu.Unlock()
		return nil
	}

	go e.attemptReconnectLoop(podIndex, addr)
	return fmt.Errorf("reconnect scheduled")
}

func (e *EngineInstance) attemptReconnectLoop(podIndex int, addr string) {
	defer func() {
		node := e.getNode(podIndex)
		if node == nil {
			return
		}
		node.reconnectMu.Lock()
		node.reconnecting = false
		node.reconnectMu.Unlock()
	}()

	backoff := 1 * time.Second
	maxBackoff := 30 * time.Second

	for {
		select {
		case <-e.ctx.Done():
			return
		default:
		}

		if err := e.connectAndRegisterPod(podIndex, addr); err == nil {
			return
		}

		time.Sleep(backoff)
		backoff *= 2
		if backoff > maxBackoff {
			backoff = maxBackoff
		}
	}
}

// connectAndRegisterPod 建立一个到 Pod 的持久 gRPC 连接，并将其注册到 rpc.Mgr。
func (e *EngineInstance) connectAndRegisterPod(podIndex int, addr string) error {
	ctx, cancel := context.WithTimeout(context.Background(), e.dialTimeout)
	defer cancel()

	conn, err := grpc.DialContext(
		ctx,
		addr,
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithBlock(),
	)
	if err != nil {
		return err
	}

	client := rpc.NewEngineClient(conn)

	e.mu.Lock()
	node, ok := e.nodes[podIndex]
	if !ok {
		podName := fmt.Sprintf("%s-%d", e.serviceName, podIndex)
		node = &EngineNode{PodIndex: podIndex, PodName: podName}
		e.nodes[podIndex] = node
	}
	e.mu.Unlock()

	oldConn := node.replaceConnection(addr, conn, client)
	rpc.Mgr.RegisterNode(podIndex, addr, client)

	if oldConn != nil {
		_ = oldConn.Close()
	}

	return nil
}

func (e *EngineInstance) getNode(podIndex int) *EngineNode {
	e.mu.RLock()
	defer e.mu.RUnlock()

	return e.nodes[podIndex]
}

func (e *EngineInstance) snapshotPodIndices() []int {
	e.mu.RLock()
	defer e.mu.RUnlock()

	indices := make([]int, 0, len(e.nodes))
	for idx := range e.nodes {
		indices = append(indices, idx)
	}
	return indices
}

func (n *EngineNode) isConnected() bool {
	n.mu.RLock()
	defer n.mu.RUnlock()

	return n.conn != nil && n.client != nil
}

func (n *EngineNode) isReconnecting() bool {
	n.reconnectMu.Lock()
	defer n.reconnectMu.Unlock()

	return n.reconnecting
}

func (n *EngineNode) clientSnapshot() rpc.EngineClient {
	n.mu.RLock()
	defer n.mu.RUnlock()

	return n.client
}

// GetClient 返回节点的 gRPC 客户端。
func (n *EngineNode) GetClient() rpc.EngineClient {
	n.mu.RLock()
	defer n.mu.RUnlock()

	return n.client
}

func (n *EngineNode) addrSnapshot() string {
	n.mu.RLock()
	defer n.mu.RUnlock()

	return n.Addr
}

func (n *EngineNode) replaceConnection(addr string, conn *grpc.ClientConn, client rpc.EngineClient) *grpc.ClientConn {
	n.mu.Lock()
	defer n.mu.Unlock()

	oldConn := n.conn
	n.Addr = addr
	n.conn = conn
	n.client = client
	n.Healthy = true
	n.LastError = ""
	n.LastHeartbeat = time.Now()
	return oldConn
}

func (n *EngineNode) markHealthy(hb *rpc.HeartbeatStatus) {
	n.mu.Lock()
	defer n.mu.Unlock()

	n.Healthy = true
	n.LastError = ""
	n.LastHeartbeat = time.Now()
	n.ActiveRooms = hb.ActiveRooms
	n.CPUUsage = hb.CPUUsage
	n.MemoryUsed = hb.MemoryUsed
	n.MaxCapacity = hb.MaxCapacity
}

func (n *EngineNode) markUnhealthy(reason error) {
	n.mu.Lock()
	defer n.mu.Unlock()

	n.Healthy = false
	n.LastError = reason.Error()
}

func (n *EngineNode) snapshot() EngineNodeStatus {
	n.mu.RLock()
	defer n.mu.RUnlock()

	return EngineNodeStatus{
		PodIndex:      n.PodIndex,
		PodName:       n.PodName,
		Addr:          n.Addr,
		Healthy:       n.Healthy,
		LastError:     n.LastError,
		LastHeartbeat: n.LastHeartbeat,
		ActiveRooms:   n.ActiveRooms,
		CPUUsage:      n.CPUUsage,
		MemoryUsed:    n.MemoryUsed,
		MaxCapacity:   n.MaxCapacity,
	}
}

func (n *EngineNode) closeConn() *grpc.ClientConn {
	n.mu.Lock()
	defer n.mu.Unlock()

	conn := n.conn
	n.conn = nil
	n.client = nil
	return conn
}

// getEnvString 从环境变量读取字符串值，或使用默认值。
func getEnvString(key, defaultVal string) string {
	if val := strings.TrimSpace(os.Getenv(key)); val != "" {
		return val
	}
	return defaultVal
}

// getEnvInt 从环境变量读取整数值，或使用默认值。
func getEnvInt(key string, defaultVal int) int {
	if val := strings.TrimSpace(os.Getenv(key)); val != "" {
		if n, err := strconv.Atoi(val); err == nil {
			return n
		}
	}
	return defaultVal
}
