package engine

import (
	"fmt"
	"strings"
)

type PoolStatus struct {
	TotalInstances int              `json:"total_instances"`
	Instances      []InstanceStatus `json:"instances"`
}

type InstanceStatus struct {
	ID    int    `json:"id"`
	Addr  string `json:"addr"`
	State string `json:"state"` // gRPC 连接状态: READY, CONNECTING, etc.
}

func (ep *EnginePool) GetStatus() PoolStatus {
	ep.mu.RLock()
	defer ep.mu.RUnlock()

	status := PoolStatus{
		TotalInstances: len(ep.Instances),
		Instances:      make([]InstanceStatus, 0, len(ep.Instances)),
	}

	for id, inst := range ep.Instances {
		state := inst.Conn.GetState()

		status.Instances = append(status.Instances, InstanceStatus{
			ID:    id,
			Addr:  inst.Addr,
			State: state.String(),
		})
	}
	return status
}

func (ep *EnginePool) PrintStatus() {
	ep.mu.RLock()
	defer ep.mu.RUnlock()

	fmt.Println("\n" + strings.Repeat("=", 60))
	fmt.Printf("%-5s | %-15s | %-8s | %-12s | %-10s\n", "ID", "Address", "Status", "Rooms/Max", "Load")
	fmt.Println(strings.Repeat("-", 60))

	for _, inst := range ep.Instances {
		inst.mu.Lock()

		status := "HEALTHY"
		if !inst.IsHealthy {
			status = "UNHEALTHY"
		}

		// 计算负载百分比
		loadPct := 0.0
		if inst.MaxCapacity > 0 {
			loadPct = (float64(inst.ActiveRooms) / float64(inst.MaxCapacity)) * 100
		}

		fmt.Printf("%-5d | %-15s | %-8s | %-12s | %-6.1f%%\n",
			inst.ID,
			inst.Addr,
			status,
			fmt.Sprintf("%d/%d", inst.ActiveRooms, inst.MaxCapacity),
			loadPct,
		)

		inst.mu.Unlock()
	}
	fmt.Println(strings.Repeat("=", 60) + "\n")
}
