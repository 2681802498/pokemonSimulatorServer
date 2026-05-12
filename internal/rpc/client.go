package rpc

import (
	"context"
	"fmt"
	"hash/crc32"
	"sort"
	"strconv"
	"sync"

	"go-server/api/calc"

	"google.golang.org/grpc"
)

// EngineClient 抽象了房间服务可调用的 C++ gRPC 能力。
// calc.NewCalculatorClient 自动满足该接口。
type EngineClient interface {
	CreateRoom(ctx context.Context, in *calc.CreateRoomRequest, opts ...grpc.CallOption) (*calc.CommonResponse, error)
	SendCommand(ctx context.Context, in *calc.GameCommand, opts ...grpc.CallOption) (*calc.CommonResponse, error)
	DestroyRoom(ctx context.Context, in *calc.DestroyRoomRequest, opts ...grpc.CallOption) (*calc.DestroyRoomResponse, error)
	GetHeartbeat(ctx context.Context, in *calc.HeartbeatRequest, opts ...grpc.CallOption) (*calc.HeartbeatResponse, error)
}

// NewEngineClient 从连接创建统一的引擎客户端。
func NewEngineClient(cc grpc.ClientConnInterface) EngineClient {
	return calc.NewCalculatorClient(cc)
}

// RPCResponse 是对通用响应的轻量封装，避免上层业务直接依赖生成代码类型。
type RPCResponse struct {
	Code    int32
	Message string
}

// HeartbeatStatus 是对心跳响应的轻量封装。
type HeartbeatStatus struct {
	Code        int32
	ActiveRooms int32
	CPUUsage    float32
	MemoryUsed  uint64
	MaxCapacity int32
	ServerID    string // C++ 实例运行的唯一 ID
}

type CppNode struct {
	ID     int
	Addr   string
	Client EngineClient
}

type HashRing struct {
	nodes    []int
	resource map[int]*CppNode
}

type NodeManager struct {
	Nodes   map[int]*CppNode
	Ring    []uint32
	RingMap map[uint32]int
	mu      sync.RWMutex
}

// 全局变量，供外部调用
var Mgr = &NodeManager{
	Nodes:   make(map[int]*CppNode),
	RingMap: make(map[uint32]int)}

func (m *NodeManager) RegisterNode(id int, addr string, client EngineClient) {
	m.mu.Lock()
	defer m.mu.Unlock()

	// 1. 存入/更新物理节点
	m.Nodes[id] = &CppNode{ID: id, Addr: addr, Client: client}

	// 2. 重新构建整个哈希环（最稳妥的做法，防止位点重复）
	m.Ring = nil
	m.RingMap = make(map[uint32]int)

	for nodeID := range m.Nodes {
		for i := 0; i < 100; i++ {
			virtualKey := "node_" + strconv.Itoa(nodeID) + "_" + strconv.Itoa(i)
			h := crc32.ChecksumIEEE([]byte(virtualKey))
			m.Ring = append(m.Ring, h)
			m.RingMap[h] = nodeID
		}
	}

	// 3. 排序以便二分查找
	sort.Slice(m.Ring, func(i, j int) bool { return m.Ring[i] < m.Ring[j] })
}

func (m *NodeManager) GetNodeByRoomID(roomID string) (*CppNode, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	if len(m.Ring) == 0 {
		return nil, fmt.Errorf("no nodes")
	}

	h := crc32.ChecksumIEEE([]byte(roomID))
	idx := sort.Search(len(m.Ring), func(i int) bool { return m.Ring[i] >= h })
	if idx == len(m.Ring) {
		idx = 0
	}

	return m.Nodes[m.RingMap[m.Ring[idx]]], nil
}

func (m *NodeManager) RemoveNode(id int) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if _, exists := m.Nodes[id]; exists {
		delete(m.Nodes, id)
		// 重新构建环：这是关键，确保 GetNodeByRoomID 不再返回这个已死的 ID
		m.rebuildRingLocked()
		fmt.Printf("[RPC] 节点 %d 已从哈希环移除\n", id)
	}
}

func (m *NodeManager) rebuildRingLocked() {
	m.Ring = nil
	m.RingMap = make(map[uint32]int)
	for nodeID := range m.Nodes {
		for i := 0; i < 100; i++ {
			virtualKey := "node_" + strconv.Itoa(nodeID) + "_" + strconv.Itoa(i)
			h := crc32.ChecksumIEEE([]byte(virtualKey))
			m.Ring = append(m.Ring, h)
			m.RingMap[h] = nodeID
		}
	}
	sort.Slice(m.Ring, func(i, j int) bool { return m.Ring[i] < m.Ring[j] })
}

func (m *NodeManager) GetNodeByID(id int) (*CppNode, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	node, exists := m.Nodes[id]
	return node, exists
}

// CallCreateRoom 将创建房间请求统一封装在 rpc 层。
func CallCreateRoom(ctx context.Context, client EngineClient, roomID, initJSON string) (*RPCResponse, error) {
	resp, err := client.CreateRoom(ctx, &calc.CreateRoomRequest{RoomId: roomID, InitJson: initJSON})
	if err != nil {
		return nil, err
	}
	return &RPCResponse{Code: resp.Code, Message: resp.Message}, nil
}

// CallSendCommand 将对战指令统一封装在 rpc 层。
func CallSendCommand(ctx context.Context, client EngineClient, roomID, playerID, action string) (*RPCResponse, error) {
	resp, err := client.SendCommand(ctx, &calc.GameCommand{RoomId: roomID, PlayerId: playerID, Action: action})
	if err != nil {
		return nil, err
	}
	return &RPCResponse{Code: resp.Code, Message: resp.Message}, nil
}

// CallDestroyRoom 将销毁房间请求统一封装在 rpc 层。
func CallDestroyRoom(ctx context.Context, client EngineClient, roomID string) (*RPCResponse, error) {
	resp, err := client.DestroyRoom(ctx, &calc.DestroyRoomRequest{RoomId: roomID})
	if err != nil {
		return nil, err
	}
	return &RPCResponse{Code: resp.Code, Message: resp.Message}, nil
}

// CallGetHeartbeat 将心跳请求统一封装在 rpc 层。
func CallGetHeartbeat(ctx context.Context, client EngineClient) (*HeartbeatStatus, error) {
	resp, err := client.GetHeartbeat(ctx, &calc.HeartbeatRequest{})
	if err != nil {
		return nil, err
	}
	return &HeartbeatStatus{
		Code:        resp.Code,
		ActiveRooms: resp.ActiveRooms,
		CPUUsage:    resp.CpuUsage,
		MemoryUsed:  resp.MemoryUsed,
		MaxCapacity: resp.MaxCapacity,
		ServerID:    resp.ServerId,
	}, nil
}
