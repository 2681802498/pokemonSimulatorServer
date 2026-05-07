module go-server

// 1. 正式声明使用 1.24
go 1.24.0

require (
	github.com/gorilla/websocket v1.5.3
	// 2. 既然用了 1.24，可以使用更新、性能更好的驱动版本
	github.com/redis/go-redis/v9 v9.18.0
	// 3. 核心：gRPC 也可以升级到最新，享受 1.24 的优化
	google.golang.org/grpc v1.80.0
	google.golang.org/protobuf v1.36.11
)

require (
	github.com/cespare/xxhash/v2 v2.3.0 // indirect
	github.com/dgryski/go-rendezvous v0.0.0-20200823014737-9f7001d12a5f // indirect
	go.uber.org/atomic v1.11.0 // indirect
	// 4. 这些官方库会自动适配 1.24
	golang.org/x/net v0.49.0 // indirect
	golang.org/x/sys v0.40.0 // indirect
	golang.org/x/text v0.33.0 // indirect
	// 5. genproto 会跟随 grpc 自动选择合适版本
	google.golang.org/genproto/googleapis/rpc v0.0.0-20260120221211-b8f7ae30c516 // indirect
)
