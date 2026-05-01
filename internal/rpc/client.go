package rpc

import (
	"fmt"
	"hash/crc32"
	"sort"
	"strconv"
	"sync"

	"go-server/api/calc"
)

type CppNode struct {
	ID     int
	Addr   string
	Client calc.CalculatorClient
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

func (m *NodeManager) RegisterNode(id int, addr string, client calc.CalculatorClient) {
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
