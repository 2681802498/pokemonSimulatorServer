package configs

import "time"

const (
	// C++ 引擎相关配置
	CppBinPath             = "./cpp_server/build/cpp_compute_server"
	CppDataDir             = "./cpp_server/data"
	CppStartPort           = 50051
	CppInstanceCount       = 2
	CppMaxInstance         = 10
	CppHealthCheckInterval = 5 * time.Second
	CppShutdownTimeout     = 30 * time.Second
	CppLoadThreshold       = 50.0 // 分配正在等待关闭的负载阈值

	CmdReadTimeout = 5 * time.Second

	MatchQueueSize = 50 //匹配池大小

	ReconnectionTimeout = 10 * time.Second // 玩家断开后保留 Session 的时间

	RedisAddr     = "127.0.0.1:6379"
	RedisPassword = ""
	RedisDB       = 0
	RedisTTL      = 2 * time.Hour

	MaxPlayersPerRoom       = 2 //房间最大人数
	RoomStatusCheckInterval = 5 * time.Second
)
