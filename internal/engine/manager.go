package engine

import (
	"context"
	"fmt"
	"go-server/configs"
	"go-server/internal/rpc"
	"hash/crc32"
	"log/slog"
	"os"
	"sort"
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
	ServerID      string    `json:"server_id,omitempty"` // C++ 实例运行的唯一 ID，用于检测重启

	mu                   sync.RWMutex
	conn                 *grpc.ClientConn
	client               rpc.EngineClient
	reconnectMu          sync.Mutex
	reconnecting         bool
	reconnectFailures    int       // 连续重连失败次数
	lastReconnectFailure time.Time // 最后一次重连失败的时间
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
	ServerID      string    `json:"server_id,omitempty"` // C++ 实例运行的唯一 ID，用于检测重启
}

// EngineStatus 是引擎连接对外暴露的状态快照。
type EngineStatus struct {
	Total               int                `json:"total"`
	Healthy             int                `json:"healthy"`
	Unhealthy           int                `json:"unhealthy"`
	Nodes               []EngineNodeStatus `json:"nodes"`
	Replicas            int                `json:"replicas"`
	Namespace           string             `json:"namespace"`
	ServiceName         string             `json:"service_name"`
	StatefulSetName     string             `json:"stateful_set_name"`
	HeadlessServiceName string             `json:"headless_service_name"`
}

// EngineInstance 负责维护 StatefulSet 中多个 C++ Pod 的长连接与健康检查。
//
// 说明：
// 1. 一个长连接对应一个 StatefulSet Pod（pod-0, pod-1, 等）；
// 2. 房间根据房间 ID 使用一致性哈希选择落地的 Pod；
// 3. 每个 Pod 有稳定的 DNS 地址：<statefulset>-{index>.<headless-service>.<namespace>.svc.cluster.local；
// 4. 健康检查确保所有 Pod 连接可用，断线自动重连；
// 5. 定期探测新 Pod 生成，自动发现并建立连接（动态扩容）。
type EngineInstance struct {
	mu                sync.RWMutex
	ctx               context.Context
	cancel            context.CancelFunc
	wg                sync.WaitGroup
	healthCheckEvery  time.Duration
	heartbeatTimeout  time.Duration
	dialTimeout       time.Duration
	podDiscoveryEvery time.Duration

	replicas            int
	namespace           string
	statefulSetName     string
	headlessServiceName string
	maxPodIndex         int // 跟踪已发现的最大Pod索引
	nodes               map[int]*EngineNode
	reassignFunc        func() // 房间重新分配回调
}

// NewEngineInstance 创建并初始化引擎连接管理器。
//
// 流程：读取 StatefulSet 配置 -> 为每个 Pod 建立持久连接 -> 启动健康检查协程 -> 启动 Pod 动态发现协程。
func NewEngineInstance() *EngineInstance {
	ctx, cancel := context.WithCancel(context.Background())

	replicas := getEnvInt("K8S_ENGINE_REPLICAS", 1)
	namespace := getEnvString("K8S_ENGINE_NAMESPACE", "default")
	statefulSetName := getEnvString("K8S_ENGINE_STATEFULSET", "pokemon-server")
	headlessServiceName := getEnvString("K8S_ENGINE_HEADLESS_SERVICE", statefulSetName+"-headless")

	inst := &EngineInstance{
		ctx:                 ctx,
		cancel:              cancel,
		healthCheckEvery:    configs.CppHealthCheckInterval,
		heartbeatTimeout:    2 * time.Second,
		dialTimeout:         5 * time.Second,
		podDiscoveryEvery:   5 * time.Second, // 每5秒探测一次新Pod
		replicas:            replicas,
		namespace:           namespace,
		statefulSetName:     statefulSetName,
		headlessServiceName: headlessServiceName,
		maxPodIndex:         replicas - 1,
		nodes:               make(map[int]*EngineNode),
	}

	slog.Info("Engine 初始化配置", "replicas", replicas, "namespace", namespace, "statefulset", statefulSetName, "headless_service", headlessServiceName)

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

// SetReassignRoomsFunc 注册房间重新分配的回调函数。
func (e *EngineInstance) SetReassignRoomsFunc(fn func()) {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.reassignFunc = fn
}

// buildPodDNSAddr 构造 StatefulSet Pod 的 DNS 地址。
func (e *EngineInstance) buildPodDNSAddr(podIndex int) string {
	podName := fmt.Sprintf("%s-%d", e.statefulSetName, podIndex)
	port := getEnvString("K8S_ENGINE_PORT", strconv.Itoa(configs.CppStartPort))
	return fmt.Sprintf("%s.%s.%s.svc.cluster.local:%s", podName, e.headlessServiceName, e.namespace, port)
}

// GetNodeForRoom 根据房间 ID 使用一致性哈希返回对应的 EngineNode。
func (e *EngineInstance) GetNodeForRoom(roomID string) (*EngineNode, error) {
	if node, ok := e.pickPreferredNodeForRoom(roomID); ok {
		return node, nil
	}

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

// pickPreferredNodeForRoom 在健康节点中优先选择“未满容量且 CPU<70%”的节点；
// 若无低负载节点，则退化为“未满容量”的健康节点。
func (e *EngineInstance) pickPreferredNodeForRoom(roomID string) (*EngineNode, bool) {
	indices := e.snapshotPodIndices()
	if len(indices) == 0 {
		return nil, false
	}

	underLoadIndices := make([]int, 0, len(indices))
	notFullIndices := make([]int, 0, len(indices))

	for _, idx := range indices {
		node := e.getNode(idx)
		if node == nil {
			continue
		}

		status := node.snapshot()
		if !status.Healthy {
			continue
		}

		// 处理 MaxCapacity 未上报 (<=0) 的情况：使用 configs.CppMaxInstance 作为回退容量。
		effectiveMax := status.MaxCapacity
		if effectiveMax <= 0 {
			effectiveMax = int32(configs.CppMaxInstance)
		}
		// 若节点已达到或超过容量阈值，则视为已满并跳过
		if effectiveMax > 0 && status.ActiveRooms >= effectiveMax {
			continue
		}

		notFullIndices = append(notFullIndices, idx)
		if status.CPUUsage < 70.0 {
			underLoadIndices = append(underLoadIndices, idx)
		}
	}

	candidates := underLoadIndices
	if len(candidates) == 0 {
		candidates = notFullIndices
	}
	if len(candidates) == 0 {
		return nil, false
	}

	sort.Ints(candidates)
	chosen := candidates[int(crc32.ChecksumIEEE([]byte(roomID)))%len(candidates)]
	node, err := e.GetNodeByPodIndex(chosen)
	if err != nil {
		return nil, false
	}

	return node, true
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
	e.mu.RLock()
	replicas := e.replicas
	e.mu.RUnlock()

	if replicas <= 0 {
		replicas = 1
	}

	h := crc32.ChecksumIEEE([]byte(roomID))
	return int(h) % replicas
}

// GetStatus 返回当前引擎连接状态快照。
func (e *EngineInstance) GetStatus() EngineStatus {
	e.mu.RLock()
	replicas := e.replicas
	namespace := e.namespace
	statefulSetName := e.statefulSetName
	headlessServiceName := e.headlessServiceName
	e.mu.RUnlock()

	indices := e.snapshotPodIndices()
	statuses := make([]EngineNodeStatus, 0, len(indices))
	healthy := 0

	for _, idx := range indices {
		node := e.getNode(idx)
		if node == nil {
			continue
		}
		s := node.snapshot()
		// 只统计仍可能影响业务的节点：
		// 1) 健康节点；
		// 2) 或者仍持有房间的非健康节点。
		// 这样可以避免历史探测到但已不存在、且没有房间的节点长期把 unhealthy 数量撑高。
		if !s.Healthy && s.ActiveRooms == 0 {
			continue
		}
		if s.Healthy {
			healthy++
		}
		statuses = append(statuses, s)
	}

	return EngineStatus{
		Total:               len(statuses),
		Healthy:             healthy,
		Unhealthy:           len(statuses) - healthy,
		Nodes:               statuses,
		Replicas:            replicas,
		Namespace:           namespace,
		ServiceName:         headlessServiceName,
		StatefulSetName:     statefulSetName,
		HeadlessServiceName: headlessServiceName,
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

// healthCheckLoop 按固定周期巡检所有引擎连接的心跳状态，并定期探测新 Pod。
func (e *EngineInstance) healthCheckLoop() {
	defer e.wg.Done()

	ticker := time.NewTicker(e.healthCheckEvery)
	defer ticker.Stop()

	// 定期探测新 Pod 的时间计数器
	podDiscoveryTicker := time.NewTicker(e.podDiscoveryEvery)
	defer podDiscoveryTicker.Stop()

	for {
		select {
		case <-e.ctx.Done(): // 如果收到关闭信号，退出循环
			return
		case <-ticker.C: // 如果断开了，尝试重连；如果连接了，检查心跳
			e.ensureConnected()
			e.checkHeartbeat()
			e.shrinkReplicaCountIfNeeded()
			// 检查心跳后，如果有不健康的节点，触发房间重新分配
			e.checkAndTriggerReassign()
		case <-podDiscoveryTicker.C: // 服务运行过程中定期探测新 Pod 是否生成
			e.discoverNewPods()
		}
	}
}

// checkAndTriggerReassign 检查是否存在不健康节点，如果有则触发房间重新分配。
func (e *EngineInstance) checkAndTriggerReassign() {
	status := e.GetStatus()
	if status.Unhealthy > 0 {
		slog.Warn("检测到 unhealthy Pod，触发房间重新分配", "unhealthy_count", status.Unhealthy)
		e.triggerReassignRooms()
	}
}

// triggerReassignRooms 同步触发房间重新分配，确保迁移先于节点清理执行。
func (e *EngineInstance) triggerReassignRooms() {
	e.mu.RLock()
	fn := e.reassignFunc
	e.mu.RUnlock()
	if fn != nil {
		fn()
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
	for _, i := range e.snapshotPodIndices() {
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
				// 记录重连失败
				e.recordReconnectFailure(i)
			}
		}
	}
}

// discoverNewPods 定期探测新 Pod 是否生成。若发现新 Pod，则自动建立连接。
func (e *EngineInstance) discoverNewPods() {
	maxAttempts := e.getMaxPodIndex() + 10 // 从已知最大索引开始，向后探测最多10个Pod

	for i := 0; i <= maxAttempts; i++ {
		if node := e.getNode(i); node != nil {
			// 已存在的节点交给健康检查/重连逻辑处理，避免重复探测。
			continue
		}

		addr := e.buildPodDNSAddr(i)

		// 快速尝试连接新Pod，使用较短的超时
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		conn, err := grpc.DialContext(
			ctx,
			addr,
			grpc.WithTransportCredentials(insecure.NewCredentials()),
			grpc.WithBlock(),
		)
		cancel()

		if err != nil {
			// 此Pod不存在，继续尝试下一个
			slog.Debug("新Pod探测失败，跳过", "pod_index", i, "addr", addr)
			continue
		}

		// 发现了新的Pod，关闭临时连接，使用正式连接建立流程
		_ = conn.Close()

		slog.Info("发现新Pod！", "pod_index", i, "addr", addr)

		// 建立正式连接
		if err := e.connectAndRegisterPod(i, addr); err != nil {
			slog.Error("新Pod连接失败", "pod_index", i, "addr", addr, "error", err)
			continue
		}

		// 更新最大已知Pod索引和副本数
		e.updateMaxPodIndex(i)
		slog.Info("新Pod已连接并注册", "pod_index", i, "addr", addr, "new_max_pod_index", i)
	}
}

// getMaxPodIndex 获取当前已发现的最大Pod索引。
func (e *EngineInstance) getMaxPodIndex() int {
	e.mu.RLock()
	defer e.mu.RUnlock()
	return e.maxPodIndex
}

// updateMaxPodIndex 更新最大已发现Pod索引。
func (e *EngineInstance) updateMaxPodIndex(podIndex int) {
	e.mu.Lock()
	defer e.mu.Unlock()

	if podIndex > e.maxPodIndex {
		e.maxPodIndex = podIndex
	}
}

// shrinkReplicaCountIfNeeded 在检测到高索引 Pod 已全部失活时，将 replicas 向下回退。
// 仅依据当前仍健康的节点计算，避免历史探测到的节点长期占用巡检位。
func (e *EngineInstance) shrinkReplicaCountIfNeeded() {
	indices := e.snapshotPodIndices()
	maxHealthyIndex := -1

	for _, idx := range indices {
		node := e.getNode(idx)
		if node == nil {
			continue
		}

		status := node.snapshot()
		if status.Healthy && idx > maxHealthyIndex {
			maxHealthyIndex = idx
		}
	}

	if maxHealthyIndex < 0 {
		return
	}

	newReplicas := maxHealthyIndex + 1

	e.mu.Lock()
	defer e.mu.Unlock()

	if newReplicas >= e.replicas {
		return
	}

	oldReplicas := e.replicas
	e.replicas = newReplicas
	if e.maxPodIndex >= newReplicas {
		e.maxPodIndex = newReplicas - 1
	}
	slog.Info("副本数已向下回退", "old_replicas", oldReplicas, "new_replicas", e.replicas)
}

// recordReconnectFailure 记录重连失败，失败超过1次时触发房间重新分配，失败2次则删除节点。
func (e *EngineInstance) recordReconnectFailure(podIndex int) {
	node := e.getNode(podIndex)
	if node == nil {
		return
	}

	node.incrementReconnectFailures()
	failures := node.getReconnectFailures()

	slog.Warn("记录 Pod 重连失败", "pod_index", podIndex, "failures", failures)

	// 失败超过1次时触发房间重新分配
	if failures > 1 {
		slog.Warn("Pod 重连失败超过1次，触发房间重新分配", "pod_index", podIndex, "failures", failures)
		e.triggerReassignRooms()
	}

	// 失败2次时删除节点
	if failures >= 2 {
		e.deleteNode(podIndex)
	}
}

// resetReconnectFailures 重置节点的重连失败计数。
func (e *EngineInstance) resetReconnectFailures(podIndex int) {
	node := e.getNode(podIndex)
	if node == nil {
		return
	}

	failures := node.getReconnectFailures()
	node.resetReconnectFailures()
	if failures > 0 {
		slog.Info("重连成功，重置失败计数", "pod_index", podIndex, "previous_failures", failures)
	}
}

// deleteNode 从管理器中删除指定的节点。
func (e *EngineInstance) deleteNode(podIndex int) {
	e.mu.Lock()
	node, ok := e.nodes[podIndex]
	if !ok {
		e.mu.Unlock()
		return
	}
	delete(e.nodes, podIndex)
	e.mu.Unlock()

	slog.Warn("删除 Pod 节点（重连失败超过2次）", "pod_index", podIndex, "pod_name", node.PodName)

	// 关闭连接
	if conn := node.closeConn(); conn != nil {
		rpc.Mgr.RemoveNode(podIndex)
		_ = conn.Close()
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

	// RPC 调用获取心跳状态，如果失败则标记节点为不健康
	hb, err := rpc.CallGetHeartbeat(ctx, client)
	if err != nil {
		node.markUnhealthy(err)
		return err
	}

	// markHealthy 返回 true 表示 ServerID 发生了变化（C++ 实例重启）
	if serverIDChanged := node.markHealthy(hb); serverIDChanged {
		slog.Warn("检测到 C++ 实例重启，触发房间重新分配", "pod_index", podIndex, "new_server_id", hb.ServerID)
		e.triggerReassignRooms()
	}
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
			// 重连成功，重置失败计数
			e.resetReconnectFailures(podIndex)
			return
		}

		// 重连失败，增加计数并检查是否需要删除节点
		e.recordReconnectFailure(podIndex)

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
		podName := fmt.Sprintf("%s-%d", e.statefulSetName, podIndex)
		node = &EngineNode{PodIndex: podIndex, PodName: podName}
		e.nodes[podIndex] = node
	}
	e.mu.Unlock()

	oldConn := node.replaceConnection(addr, conn, client)
	rpc.Mgr.RegisterNode(podIndex, addr, client)

	if oldConn != nil {
		_ = oldConn.Close()
	}

	// 连接成功，重置失败计数
	e.resetReconnectFailures(podIndex)

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

func (n *EngineNode) markHealthy(hb *rpc.HeartbeatStatus) bool {
	n.mu.Lock()
	defer n.mu.Unlock()

	// 检查 ServerID 是否发生变化（说明 C++ 实例重启）
	serverIDChanged := n.ServerID != "" && n.ServerID != hb.ServerID

	n.Healthy = true
	n.LastError = ""
	n.LastHeartbeat = time.Now()
	n.ActiveRooms = hb.ActiveRooms
	n.CPUUsage = hb.CPUUsage
	n.MemoryUsed = hb.MemoryUsed
	n.MaxCapacity = hb.MaxCapacity
	n.ServerID = hb.ServerID

	// 如果检测到重启，重置重连失败计数（防止删除新启动的节点）
	if serverIDChanged {
		n.reconnectFailures = 0
	}

	return serverIDChanged
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
		ServerID:      n.ServerID,
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

func (n *EngineNode) getReconnectFailures() int {
	n.mu.RLock()
	defer n.mu.RUnlock()
	return n.reconnectFailures
}

func (n *EngineNode) incrementReconnectFailures() {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.reconnectFailures++
	n.lastReconnectFailure = time.Now()
}

func (n *EngineNode) resetReconnectFailures() {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.reconnectFailures = 0
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
