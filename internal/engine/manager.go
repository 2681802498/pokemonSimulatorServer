package engine

import (
	"context"
	"fmt"
	"go-server/api/calc"
	"go-server/configs"
	"go-server/internal/protocol"
	"go-server/internal/room"
	"go-server/internal/rpc"
	"log"
	"os"
	"os/exec"
	"strconv"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

type EngineInstance struct {
	ID          int
	Port        int
	Addr        string
	Cmd         *exec.Cmd
	Conn        *grpc.ClientConn
	Client      calc.CalculatorClient
	ActiveRooms int32      // 当前房间数
	MaxCapacity int32      // 最大承载能力
	IsHealthy   bool       // 节点是否健康
	LastUpdate  time.Time  // 上次更新时间
	mu          sync.Mutex // 保护实例内部状态切换
	EmptySince  *time.Time // 记录变为空的时间（nil表示非空或刚变为空）
	ShouldStop  bool       // 主动关闭标记：true 表示不要重启
}

type EnginePool struct {
	Instances map[int]*EngineInstance
	isClosing bool
	mu        sync.RWMutex
	usedPorts map[int]bool // 端口占用表
	nextID    int          // 自增实例ID
}

type NodeStatus struct {
	ID          int     `json:"id"`
	Address     string  `json:"address"`
	IsHealthy   bool    `json:"is_healthy"`
	ActiveRooms int32   `json:"active_rooms"`
	MaxCapacity int32   `json:"max_capacity"`
	LoadPercent float64 `json:"load_percent"`
}

type ClusterStatusResponse struct {
	Nodes      []NodeStatus `json:"nodes"`
	TotalRooms int          `json:"total_rooms"`
}

// 为新实例分配端口
func (ep *EnginePool) allocPort() int {
	base := configs.CppStartPort
	max := configs.CppStartPort + configs.CppMaxInstance*10 // 预留足够空间
	for p := base; p < max; p++ {
		if !ep.usedPorts[p] {
			ep.usedPorts[p] = true
			return p
		}
	}
	panic("No available port for new instance")
}

// 释放端口
func (ep *EnginePool) freePort(port int) {
	delete(ep.usedPorts, port)
}

//新建引擎池
func NewEnginePool() *EnginePool {
	pool := &EnginePool{
		Instances: make(map[int]*EngineInstance),
		usedPorts: make(map[int]bool),
		nextID:    1,
	}

	for i := 0; i < configs.CppInstanceCount; i++ {
		id := pool.nextID
		pool.nextID++
		port := pool.allocPort()
		pool.startInstance(id, port)
	}
	// 分配未被占用的端口

	go pool.monitorNodes()

	// 定时动态扩缩容
	go func() {
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for range ticker.C {
			pool.CheckAndScale()
		}
	}()

	return pool
}

// 绑定房间管理器，供自动扩缩容时调用
func (ep *EnginePool) CheckAndScale() {
	ep.mu.Lock()
	defer ep.mu.Unlock()

	totalActive := 0
	totalMax := 0
	emptyInstanceCount := 0
	var emptyInstances []*EngineInstance

	for _, inst := range ep.Instances {
		if inst.IsHealthy {
			totalActive += int(inst.ActiveRooms)
			totalMax += int(inst.MaxCapacity)

			if inst.ActiveRooms == 0 {
				emptyInstanceCount++
				emptyInstances = append(emptyInstances, inst)
				// 记录变为空的时间
				if inst.EmptySince == nil {
					now := time.Now()
					inst.EmptySince = &now
				}
			} else {
				// 只要有房间，清空EmptySince
				inst.EmptySince = nil
			}
		}
	}

	timeToWait := configs.CppShutdownTimeout
	globalLoad := 0.0
	if totalMax > 0 {
		globalLoad = (float64(totalActive) / float64(totalMax)) * 100
	}

	// 扩容策略：当负载过高或空闲实例不足时，拉起新实例
	needMoreInstances := false
	if globalLoad > 75 && len(ep.Instances) < configs.CppMaxInstance {
		needMoreInstances = true
		log.Printf("[AutoScaler] 压力过大 (%.1f%%)，正在拉起新实例", globalLoad)
	} else if emptyInstanceCount == 0 && len(ep.Instances) < configs.CppMaxInstance {
		// 没有空闲实例，拉起一个作为备用（用于数据转移）
		needMoreInstances = true
		log.Printf("[AutoScaler] 无空闲实例，拉起备用实例用于数据转移")
	}

	if needMoreInstances {
		newID := ep.nextID
		ep.nextID++
		newPort := ep.allocPort()
		go ep.startInstance(newID, newPort)
	}

	// 最小保留实例数，和启动时一致
	minInstanceCount := configs.CppInstanceCount

	if len(ep.Instances) > minInstanceCount && emptyInstanceCount > minInstanceCount {
		// 只保留minInstanceCount个空闲实例，关闭其他的
		for i := minInstanceCount; i < len(emptyInstances); i++ {
			instToShutdown := emptyInstances[i]
			if instToShutdown.EmptySince != nil && time.Since(*instToShutdown.EmptySince) > timeToWait {

				log.Printf("[AutoScaler] 关闭冗余空实例 ID: %d (已空闲%.0fs)", instToShutdown.ID, time.Since(*instToShutdown.EmptySince).Seconds())
				go ep.shutdownInstance(instToShutdown.ID)
			} else {
				log.Printf("[AutoScaler] 空实例 ID: %d 空闲不足等待阈值，暂不关闭", instToShutdown.ID)
			}
		}
	}
}

//开启引擎实例
func (ep *EnginePool) startInstance(id int, port int) {
	addr := fmt.Sprintf("127.0.0.1:%d", port)
	log.Printf("[Engine] 启动参数: bin=%s, port=%d, data_dir=%s", configs.CppBinPath, port, configs.CppDataDir)
	cmd := exec.Command(
		configs.CppBinPath,
		"--port", strconv.Itoa(port),
		"--data-dir", configs.CppDataDir,
	)

	_ = os.MkdirAll("logs", 0755)
	logPath := fmt.Sprintf("logs/engine_%d_stdout.log", id)

	outfile, err := os.OpenFile(logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	if err != nil {
		log.Printf("[Engine] 无法创建日志文件 %s: %v", logPath, err)
	} else {
		cmd.Stdout = outfile
		cmd.Stderr = outfile
	}

	if err := cmd.Start(); err != nil {
		log.Printf("[Engine] 关键错误：无法启动 ID %d (端口 %d): %v", id, port, err)
		return
	}

	log.Printf("[Engine] 实例已拉起: ID=%d, Port=%d, PID=%d", id, port, cmd.Process.Pid)

	// 建立 gRPC 连接
	conn, err := grpc.NewClient(addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Printf("[Engine] 警告：无法连接到 %s: %v", addr, err)
	}

	client := calc.NewCalculatorClient(conn)
	inst := &EngineInstance{
		ID:          id,
		Port:        port,
		Addr:        addr,
		Cmd:         cmd,
		Conn:        conn,
		Client:      client,
		IsHealthy:   true,
		ActiveRooms: 0,
	}

	ep.mu.Lock()
	ep.Instances[id] = inst
	ep.mu.Unlock()

	rpc.Mgr.RegisterNode(id, addr, client)

	go func(i *EngineInstance, f *os.File) {
		ep.watchInstance(i)
		if f != nil {
			f.Close()
			log.Printf("[Engine] 实例 %d 日志文件已关闭", i.ID)
		}
	}(inst, outfile)
}

//监视引擎实例，自动重启
func (ep *EnginePool) watchInstance(inst *EngineInstance) {
	// 等待进程结束
	_ = inst.Cmd.Wait()

	inst.mu.Lock()
	shouldStop := inst.ShouldStop
	inst.mu.Unlock()

	ep.mu.RLock()
	if ep.isClosing {
		log.Printf("server close")
		ep.mu.RUnlock()
		return
	}
	ep.mu.RUnlock()

	if shouldStop {
		log.Printf("[Engine] 实例 %s (ID:%d) 为主动关闭，不执行重启", inst.Addr, inst.ID)
		return
	}

	log.Printf("[Warning] 引擎实例 %s (ID:%d) 已退出, 1秒后尝试重启...", inst.Addr, inst.ID)

	if inst.Conn != nil {
		inst.Conn.Close()
	}

	time.Sleep(time.Second * 1)

	// 直接调用 startInstance 重新拉起
	ep.startInstance(inst.ID, inst.Port)
}

//选择出负载最低的实例（最适合填入的实例/c++服务器）
func (ep *EnginePool) PickBestInstance() (*EngineInstance, error) {
	ep.mu.RLock()
	defer ep.mu.RUnlock()

	var (
		best           *EngineInstance //最好的实例
		minLoad        = 101.0
		candidates     []*EngineInstance
		candidateLoads []float64 //记录负载以便后续比较
		waitingEmpty   []*EngineInstance
		waitingLoads   []float64
	)

	for _, inst := range ep.Instances {
		inst.mu.Lock()
		if !inst.IsHealthy {
			inst.mu.Unlock()
			continue
		}
		load := 0.0
		if inst.MaxCapacity > 0 {
			load = (float64(inst.ActiveRooms) / float64(inst.MaxCapacity)) * 100
		}
		// 非等待关闭的节点
		if !(inst.ActiveRooms == 0 && inst.EmptySince != nil) {
			candidates = append(candidates, inst)
			candidateLoads = append(candidateLoads, load)
		} else {
			waitingEmpty = append(waitingEmpty, inst)
			waitingLoads = append(waitingLoads, load)
		}
		inst.mu.Unlock()
	}

	// 优先选非等待关闭节点
	for i, inst := range candidates {
		if candidateLoads[i] < minLoad {
			minLoad = candidateLoads[i]
			best = inst
		}
	}

	// 如果所有非等待关闭节点负载都高于阈值，再考虑等待关闭的空闲节点
	if best == nil || minLoad > configs.CppLoadThreshold {
		for i, inst := range waitingEmpty {
			if waitingLoads[i] < minLoad {
				minLoad = waitingLoads[i]
				best = inst
			}
		}
	}

	if best == nil {
		return nil, fmt.Errorf("所有节点已满或不健康")
	}
	return best, nil
}

//从引擎池中关闭实例
func (ep *EnginePool) shutdownInstance(id int) {
	ep.mu.Lock()
	inst, ok := ep.Instances[id]
	if !ok {
		ep.mu.Unlock()
		return
	}

	inst.mu.Lock()
	inst.ShouldStop = true
	inst.mu.Unlock()

	// 检查实例是否为空，只有空实例才能被关闭
	if inst.ActiveRooms > 0 {
		ep.mu.Unlock()
		log.Printf("[Engine] 实例 %d 仍有活跃房间 (%d)，无法关闭", id, inst.ActiveRooms)
		return
	}

	// 从池中移除
	delete(ep.Instances, id)
	ep.freePort(inst.Port)
	ep.mu.Unlock()

	// 关闭进程和连接
	if inst.Cmd != nil && inst.Cmd.Process != nil {
		log.Printf("[Engine] 关闭空实例 %d (PID: %d)", id, inst.Cmd.Process.Pid)
		_ = inst.Cmd.Process.Kill()
	}

	if inst.Conn != nil {
		inst.Conn.Close()
	}

	// 注销节点
	rpc.Mgr.RemoveNode(id)
}

//关闭整个引擎池
func (ep *EnginePool) Shutdown() {
	ep.mu.Lock()
	if ep.isClosing {
		ep.mu.Unlock()
		return
	}
	ep.isClosing = true
	ep.mu.Unlock()

	for id, inst := range ep.Instances {
		inst.mu.Lock()
		inst.ShouldStop = true
		inst.mu.Unlock()

		if inst.Cmd.Process != nil {
			log.Printf("[Engine] 正在关闭实例: %d", id)
			_ = inst.Cmd.Process.Kill()
		}
		if inst.Conn != nil {
			inst.Conn.Close()
		}
		rpc.Mgr.RemoveNode(id)
	}
}

//每五秒与引擎节点同步一次状态，更新负载信息和健康状态
func (ep *EnginePool) monitorNodes() {
	ticker := time.NewTicker(5 * time.Second) // 每 5 秒同步一次负载
	for range ticker.C {
		ep.mu.RLock()
		// 检查是否正在关闭
		if ep.isClosing {
			ep.mu.RUnlock()
			return
		}

		// 复制一份实例指针，避免在 RPC 阻塞时长期持有大锁
		instances := make([]*EngineInstance, 0, len(ep.Instances))
		for _, inst := range ep.Instances {
			instances = append(instances, inst)
		}
		ep.mu.RUnlock()

		for _, inst := range instances {
			go func(node *EngineInstance) {
				// 设置较短的超时，防止某个 C++ 节点卡死拖累监控协程
				ctx, cancel := context.WithTimeout(context.Background(), time.Second*2)
				defer cancel()

				resp, err := node.Client.GetHeartbeat(ctx, &calc.HeartbeatRequest{})

				node.mu.Lock()
				defer node.mu.Unlock()

				if err != nil {
					if node.IsHealthy {
						log.Printf("[Monitor] 警告：节点 %d 连接中断: %v", node.ID, err)
					}
					node.IsHealthy = false
				} else {
					node.IsHealthy = true
					node.ActiveRooms = resp.ActiveRooms
					node.MaxCapacity = resp.MaxCapacity
					node.LastUpdate = time.Now()
				}
			}(inst)
		}
	}
}

//返回c++服务器集群状态
func (ep *EnginePool) GetClusterStatus() ClusterStatusResponse {
	ep.mu.RLock()
	defer ep.mu.RUnlock()

	res := ClusterStatusResponse{
		Nodes:      make([]NodeStatus, 0, len(ep.Instances)),
		TotalRooms: 0,
	}

	for _, inst := range ep.Instances {
		inst.mu.Lock()

		loadPct := 0.0
		if inst.MaxCapacity > 0 {
			loadPct = (float64(inst.ActiveRooms) / float64(inst.MaxCapacity)) * 100
		}

		node := NodeStatus{
			ID:          inst.ID,
			Address:     inst.Addr,
			IsHealthy:   inst.IsHealthy,
			ActiveRooms: inst.ActiveRooms,
			MaxCapacity: inst.MaxCapacity,
			LoadPercent: loadPct,
		}

		res.TotalRooms += int(inst.ActiveRooms)
		res.Nodes = append(res.Nodes, node)

		inst.mu.Unlock()
	}

	return res
}

//返回c++服务器集群状态的响应
func (ep *EnginePool) StatusResponse(s *room.Session) {
	statusData := ep.GetClusterStatus()
	s.SendResponse("cluster_status_res", protocol.CodeSuccess, "获取集群状态成功", statusData)
}
