# Go 客户端连接指南

## 服务器信息
- gRPC 地址：`127.0.0.1:50051`
- 协议：`plaintext`（未启用 TLS）
- Proto 包名：`calc`
- 服务名：`Calculator`
- Proto 文件：`api/proto/calc.proto`
- Go 输出包：`api/calc`

## 当前 RPC

### `CreateRoom`
请求：
- `room_id: string`
- `init_json: string`

响应：
- `code: int32`
- `message: string`

### `SendCommand`
请求：
- `room_id: string`
- `player_id: string`
- `action: string`

响应：
- `code: int32`
- `message: string`

### `DestroyRoom`
请求：
- `room_id: string`

响应：
- `code: int32`
- `message: string`

### `GetHeartbeat`
请求：
- 空请求

响应：
- `code: int32`
- `active_rooms: int32`
- `cpu_usage: float`
- `memory_used: uint64`
- `max_capacity: int32`
- `server_id: string`（用于识别 C++ 实例是否重启）

## Go 侧生成代码

建议在 Go 项目中执行：

```bash
protoc --go_out=. --go-grpc_out=. api/proto/calc.proto
```

生成后可通过 `calc` 包引用 `CalculatorClient`。

## 连接示例

```go
ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
defer cancel()

conn, err := grpc.DialContext(
    ctx,
    "127.0.0.1:50051",
    grpc.WithTransportCredentials(insecure.NewCredentials()),
    grpc.WithBlock(),
)
if err != nil {
    panic(err)
}
defer conn.Close()

client := calc.NewCalculatorClient(conn)

hb, err := client.GetHeartbeat(context.Background(), &calc.HeartbeatRequest{})
if err != nil {
    panic(err)
}

fmt.Printf("code=%d active_rooms=%d server_id=%s\n", hb.Code, hb.ActiveRooms, hb.ServerId)
```

示例依赖：`context`、`time`、`fmt`、`google.golang.org/grpc`、`google.golang.org/grpc/credentials/insecure`。

## K8s 调试连接（可选）

如果在 Kubernetes 中调试 C++ 引擎 gRPC，可先端口转发到引擎 Pod（示例）：

```bash
kubectl port-forward pod/pokemon-server-0 50051:50051
```

然后继续用 `127.0.0.1:50051` 连接。

## 测试说明

本机已验证：
- `GetHeartbeat` 可正常返回
- Redis 依赖需要可用，当前服务器连接 `127.0.0.1:6379`

## 备注

当前服务器还会：
- 启动时创建 2 个测试房间
- 保持房间活动更新线程运行
- `server_id` 变化可用于判断 C++ 实例重启并触发房间重新分配逻辑
