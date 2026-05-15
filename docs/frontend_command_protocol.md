# 前端指令集与后端返回约定

根据当前服务端路由与协议定义，前后端通过 WebSocket 交互。前端发起请求时使用统一结构，后端通过统一响应结构返回。

## 1. 通用消息格式

### 前端发送

```json
{
  "cmd": "指令名",
  "data": {}
}
```

- `cmd`：指令名
- `data`：业务数据，部分指令可为空对象 `{}`

### 后端返回

```json
{
  "cmd": "响应指令名",
  "code": 0,
  "msg": "说明",
  "data": {}
}
```

- `cmd`：响应指令名
- `code`：状态码，`0` 表示成功
- `msg`：返回说明
- `data`：返回数据

## 2. WebSocket 连接

### 连接地址

```
ws://<host>:8080/ws?uid=<player-id>
```

- `uid` 查询参数**必填**，缺失时返回 HTTP 400
- 全局无鉴权，`uid` 即为玩家唯一身份标识

### 断线与重连

1. WebSocket 连接断开后，Session 保留 **10 秒**（`ReconnectionTimeout`）
2. 10 秒内用相同 `uid` 重新连接，可恢复房间状态
3. 超过 10 秒未重连，Session 被清理，玩家自动离开房间
4. 如果旧连接存在且新连接接入（顶号），旧连接自动关闭

### 重连响应

重连时，后端根据玩家状态返回不同消息：

**已在房间中（恢复房间状态）**：

```json
{
  "cmd": "reconnect_res",
  "code": 0,
  "msg": "状态恢复成功",
  "data": {
    "room_id": "abc123...",
    "room_status": 0,
    "players": [{"id": "player1"}, {"id": "player2"}],
    "game_data": {"status": "waiting_cpp_integration"}
  }
}
```

同时向房间内其他玩家广播：

```json
{
  "cmd": "player_reconnect",
  "code": 0,
  "msg": "玩家 xxx 重新连接",
  "data": {"uid": "xxx"}
}
```

**在大厅（不在任何房间中）**：

```json
{
  "cmd": "reconnect_success",
  "code": 0,
  "msg": "已重连至大厅",
  "data": null
}
```

**房间已销毁**：

```json
{
  "cmd": "reconnect_error",
  "code": 404,
  "msg": "房间已销毁",
  "data": null
}
```

## 3. 错误码

| code | 含义 |
|---|---|
| 0 | 成功 |
| 10001 | 服务端错误 |
| 10002 | 参数错误 |
| 20001 | 非法发送指令（未知指令） |
| 20002 | 重复发送（如已在房间中再次加入） |
| 20003 | 数据非法（参数格式/内容不正确） |
| 30001 | C++ RPC 错误（引擎不可用或调用失败） |
| 40001 | 房间不存在 |
| 40002 | 房间已满（或房间已开始/已结束，用于 join_room） |
| 40003 | 房间已开始（无法选择宝可梦/无法提交动作） |
| 40004 | 房间已结束 |
| 40005 | 玩家不在房间中 |
| 50001 | 玩家连接失败（保留，当前未使用） |
| 50002 | 玩家重连失败（保留，当前未使用） |

## 4. 前端指令集

### 4.1 cluster_status

用途：获取集群状态（C++ 引擎节点健康信息）。

前端发送：

```json
{
  "cmd": "cluster_status",
  "data": {}
}
```

后端返回：

```json
{
  "cmd": "cluster_status_res",
  "code": 0,
  "msg": "获取集群状态成功",
  "data": {
    "total": 2,
    "healthy": 2,
    "unhealthy": 0,
    "nodes": [
      {
        "pod_index": 0,
        "pod_name": "engine-0",
        "addr": "engine-0.engine-headless.namespace.svc.cluster.local:50051",
        "healthy": true,
        "last_error": "",
        "last_heartbeat": "2025-01-01T00:00:00Z",
        "active_rooms": 0,
        "cpu_usage": 12.5,
        "memory_used": 1048576,
        "max_capacity": 10,
        "server_id": "abc123"
      }
    ],
    "replicas": 2,
    "namespace": "default",
    "service_name": "",
    "statefulset_name": "",
    "headless_service_name": ""
  }
}
```

---

### 4.2 get_redis

用途：获取 Redis 中的房间快照数据（调试/排障用）。

前端发送：

```json
{
  "cmd": "get_redis",
  "data": {}
}
```

后端返回：

```json
{
  "cmd": "get_redis_res",
  "code": 0,
  "msg": "获取 Redis 数据成功",
  "data": {
    "type": "get_redis_res",
    "count": 1,
    "timestamp": 1710000000,
    "rooms": [
      {
        "snapshot": {
          "room_id": "abc123",
          "status": 0,
          "node_id": 0,
          "players": ["player1", "player2"],
          "ready_players": {"player1": true},
          "selected_pokemon": {},
          "updated_at": 1710000000
        },
        "remaining_ttl_sec": 7200.0,
        "is_persistent": true
      }
    ],
    "engine_status": { "... 同上 cluster_status data ..." }
  }
}
```

失败时可能返回：

```json
{
  "cmd": "get_redis_res",
  "code": 10001,
  "msg": "Redis 未初始化",
  "data": null
}
```

```json
{
  "cmd": "get_redis_res",
  "code": 10001,
  "msg": "无法读取快照",
  "data": null
}
```

---

### 4.3 create_room

用途：创建房间。创建者自动加入房间。

前端发送：

```json
{
  "cmd": "create_room",
  "data": {}
}
```

后端返回：

```json
{
  "cmd": "create_room_res",
  "code": 0,
  "msg": "房间创建成功",
  "data": {
    "room_id": "a1b2c3d4e5f6a7b8"
  }
}
```

说明：`room_id` 为 16 字符十六进制字符串，由 `crypto/rand` 生成。

---

### 4.4 join_room

用途：加入指定房间。

前端发送：

```json
{
  "cmd": "join_room",
  "data": {
    "room_id": "a1b2c3d4e5f6a7b8"
  }
}
```

后端返回成功：

```json
{
  "cmd": "join_room_res",
  "code": 0,
  "msg": "成功加入房间",
  "data": null
}
```

加入成功后，向房间内所有玩家**广播**：

```json
{
  "cmd": "join_room",
  "code": 0,
  "msg": "玩家xxx加入了房间",
  "data": null
}
```

常见失败返回：

```json
{
  "cmd": "join_room_res",
  "code": 40001,
  "msg": "房间不存在,请检查房间编号",
  "data": null
}
```

```json
{
  "cmd": "join_room_res",
  "code": 20002,
  "msg": "你已经在房间里了",
  "data": null
}
```

```json
{
  "cmd": "join_room_res",
  "code": 40002,
  "msg": "房间已开始或已结束",
  "data": null
}
```

```json
{
  "cmd": "join_room_res",
  "code": 40002,
  "msg": "房间已满",
  "data": null
}
```

---

### 4.5 leave_room

用途：离开当前房间。

前端发送：

```json
{
  "cmd": "leave_room",
  "data": {}
}
```

后端返回：

```json
{
  "cmd": "leave_room_res",
  "code": 0,
  "msg": "已退出房间",
  "data": null
}
```

离开后，后端行为：

1. 房间内还有其他玩家时，广播玩家离开消息（cmd 携带离开玩家信息）：

```json
{
  "cmd": "Player Left! (playerID: xxx)",
  "code": 0,
  "msg": "玩家xxx已离开",
  "data": null
}
```

2. 如果游戏正在进行中（`RoomPlaying`），房间状态变更为 `RoomFinished` 并广播：

```json
{
  "cmd": "room_status_change",
  "code": 0,
  "msg": "游戏因玩家离开而停止",
  "data": null
}
```

3. 房间内无剩余玩家时，房间被删除。

如果房间不存在：

```json
{
  "cmd": "leave_room_res",
  "code": 40001,
  "msg": "房间不存在",
  "data": null
}
```

---

### 4.6 list_rooms

用途：获取当前等待中的房间列表。

前端发送：

```json
{
  "cmd": "list_rooms",
  "data": {}
}
```

后端返回：

```json
{
  "cmd": "list_rooms_res",
  "code": 0,
  "msg": "获取列表成功",
  "data": [
    {"room_id": "a1b2c3d4e5f6a7b8", "is_full": false},
    {"room_id": "b2c3d4e5f6a7b8c9", "is_full": true}
  ]
}
```

说明：`is_full` 为 `true` 表示房间已有 2 人（满员）。

---

### 4.7 match

用途：进入匹配队列。匹配采用简单 FIFO 模式，队列中每凑满 2 人自动创建房间并加入。

前端发送：

```json
{
  "cmd": "match",
  "data": {}
}
```

后端返回：

```json
{
  "cmd": "match_res",
  "code": 0,
  "msg": "已进入匹配队列",
  "data": null
}
```

说明：
- 匹配队列缓冲区大小 50
- 匹配成功时，系统自动为第一位玩家创建房间，第二位玩家自动加入该房间
- 匹配成功后，两位玩家分别收到 `create_room_res` / `join_room_res` 以及相关广播

---

### 4.8 start_game

用途：开始对战。

前端发送：

```json
{
  "cmd": "start_game",
  "data": {}
}
```

**错误响应**（直接返回给发送者）：

```json
{
  "cmd": "start_game_res",
  "code": 40001,
  "msg": "当前不在房间中",
  "data": null
}
```

```json
{
  "cmd": "start_game_res",
  "code": 40001,
  "msg": "房间不存在",
  "data": null
}
```

**成功时**（无直接 `start_game_res` 响应），服务端异步初始化 C++ 引擎（最多重试 3 次），成功后**广播**：

```json
{
  "cmd": "game_started",
  "code": 0,
  "msg": "游戏正式开始",
  "data": null
}
```

**失败时广播**：

```json
{
  "cmd": "game_start_error",
  "code": 30001,
  "msg": "失败原因（如：房间人数不足 2 人，无法初始化对战）",
  "data": null
}
```

说明：如果双方通过 `select_pokemon` 都已准备好，系统会自动触发 `start_game`，无需前端手动调用。

---

### 4.9 select_pokemon

用途：选择最多 6 个宝可梦。

前端发送的数据结构：

```json
{
  "cmd": "select_pokemon",
  "data": {
    "pokemon": [
      {
        "species_id": 25,
        "level": 50,
        "nature": 10,
        "ability": 66,
        "item": 99,
        "ivs": {"hp": 31, "attack": 31, "defense": 31, "specialAttack": 31, "specialDefense": 31, "speed": 31},
        "evs": {"hp": 0, "attack": 0, "defense": 4, "specialAttack": 252, "specialDefense": 0, "speed": 252},
        "moves": [85, 98, 86, 87]
      }
    ]
  }
}
```

字段说明：
- `species_id`：宝可梦种类 ID（整数）
- `level`：等级（整数）
- `nature`：性格（整数）
- `ability`：特性 ID（**必须是数字**，JSON 反序列化后校验为 float64）
- `item`：携带道具 ID（整数）
- `ivs` / `evs`：个体值 / 努力值，6 项均为整数
- `moves`：招式 ID 列表（整数数组）

后端返回成功：

```json
{
  "cmd": "select_pokemon_res",
  "code": 0,
  "msg": "宝可梦选择成功",
  "data": null
}
```

随后广播：

```json
{
  "cmd": "player_ready",
  "code": 0,
  "msg": "玩家 xxx 已准备好",
  "data": null
}
```

如果双方（2 人）都准备好，会广播宝可梦选择汇总并自动开始游戏：

```json
{
  "cmd": "pokemon_selection_summary",
  "code": 0,
  "msg": "双方宝可梦选择已汇总",
  "data": { "... 包含 side_a/side_b 宝可梦数据 + seed ..." }
}
```

常见失败返回：

```json
{
  "cmd": "select_pokemon_res",
  "code": 40005,
  "msg": "当前不在房间中",
  "data": null
}
```

```json
{
  "cmd": "select_pokemon_res",
  "code": 40005,
  "msg": "房间不存在",
  "data": null
}
```

```json
{
  "cmd": "select_pokemon_res",
  "code": 40005,
  "msg": "你不在该房间中",
  "data": null
}
```

```json
{
  "cmd": "select_pokemon_res",
  "code": 40003,
  "msg": "房间已开始游戏，无法选择宝可梦",
  "data": null
}
```

```json
{
  "cmd": "select_pokemon_res",
  "code": 20003,
  "msg": "pokemon 数据不能为空",
  "data": null
}
```

```json
{
  "cmd": "select_pokemon_res",
  "code": 20003,
  "msg": "JSON 解析失败: <具体错误>",
  "data": null
}
```

```json
{
  "cmd": "select_pokemon_res",
  "code": 20003,
  "msg": "最多只能选择 6 个宝可梦",
  "data": null
}
```

```json
{
  "cmd": "select_pokemon_res",
  "code": 20003,
  "msg": "ability 必须是数字",
  "data": null
}
```

---

### 4.10 battle_action

用途：提交对战动作。

前端发送：

```json
{
  "cmd": "battle_action",
  "data": {
    "action": "attack",
    "target": "Ember"
  }
}
```

说明：
- `data` 只要是合法 JSON 即可，具体字段由对战业务决定
- 服务端自动注入 `side` 字段（`"a"` 或 `"b"`），前端不需要自己传
- side 分配规则：房间内玩家按 ID 字母序排序，第一位为 a，第二位为 b

**直接返回**（动作提交后立即返回）：

当对手尚未提交动作时（`waiting == true`）：

```json
{
  "cmd": "battle_action_res",
  "code": 0,
  "msg": "动作已提交，等待对手",
  "data": {
    "waiting": true,
    "completed_side": "a",
    "completed_player_id": "player1"
  }
}
```

同时广播：

```json
{
  "cmd": "battle_state_push",
  "code": 0,
  "msg": "有玩家已提交动作，等待另一方",
  "data": { "... 同上 waiting 数据 ..." }
}
```

当双方都已提交（`waiting == false`，C++ 返回回合结果）：

```json
{
  "cmd": "battle_action_res",
  "code": 0,
  "msg": "动作已提交",
  "data": { "... C++ 引擎返回的回合结果 ..." }
}
```

同时广播：

```json
{
  "cmd": "battle_state_push",
  "code": 0,
  "msg": "对战状态更新",
  "data": { "... C++ 引擎返回的回合结果 ..." }
}
```

常见失败返回：

```json
{
  "cmd": "battle_action_res",
  "code": 40001,
  "msg": "当前不在房间中",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 40001,
  "msg": "房间不存在",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 40005,
  "msg": "你不在该房间中",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 40003,
  "msg": "房间当前不在对战中",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 20003,
  "msg": "action 不能为空",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 20003,
  "msg": "action 必须是合法 JSON",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 20003,
  "msg": "action 处理失败: <具体错误>",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 30001,
  "msg": "目标 C++ 节点不可用",
  "data": null
}
```

```json
{
  "cmd": "battle_action_res",
  "code": 30001,
  "msg": "C++ 调用失败: <具体错误>",
  "data": null
}
```

## 5. 服务端主动推送消息

这些消息不是前端主动请求后立刻得到的普通响应，而是服务端在业务变化时主动发送：

| 推送 cmd | 含义 | 触发场景 |
|---|---|---|
| `join_room` | 有玩家加入房间 | 其他玩家 join_room 成功后广播 |
| `player_ready` | 某个玩家已准备好 | 玩家 select_pokemon 成功后广播 |
| `pokemon_selection_summary` | 双方宝可梦选择汇总 | 双方都 ready 后广播，附带 side_a/side_b + seed |
| `game_started` | 游戏正式开始 | C++ 引擎初始化成功后广播 |
| `game_start_error` | 游戏开始失败 | 引擎初始化失败后广播（code 30001） |
| `battle_state_push` | 对战状态变化 | 玩家提交 battle_action 后广播 |
| `room_status_change` | 房间状态变化 | 游戏中玩家离开导致房间结束 |
| `player_reconnect` | 玩家重连 | 离线玩家重新连接并恢复房间状态后广播 |

重连相关推送（仅发送给重连玩家本人）：

| 推送 cmd | 含义 | 触发场景 |
|---|---|---|
| `reconnect_res` | 房间状态恢复成功 | 重连时玩家在某个房间中 |
| `reconnect_success` | 重连至大厅 | 重连时玩家不在任何房间中 |
| `reconnect_error` | 重连失败 | 重连时原房间已销毁（code 404） |

## 6. 未知指令

如果前端发送了未注册指令，后端会返回：

```json
{
  "cmd": "<原cmd>_res",
  "code": 20001,
  "msg": "未知指令",
  "data": null
}
```

## 7. 前端接入建议

1. `*_res` 视为请求响应（直接对应前端请求）
2. `*_push`、`game_started`、`game_start_error`、`player_ready`、`room_status_change`、`pokemon_selection_summary`、`player_reconnect` 视为服务端主动推送
3. `reconnect_res`、`reconnect_success`、`reconnect_error` 为重连流程专用消息
4. 所有请求都走统一的 `cmd + data` 结构
5. 收到响应后根据 `code` 判断成功或失败（`0` = 成功）
6. WebSocket 连接必须携带 `?uid=<player-id>` 参数
7. 断线后 10 秒内使用相同 `uid` 重连可恢复状态，超时后数据丢失
8. 注意处理 `battle_action_res` 中的 `waiting` 字段，区分"等待对手"和"回合结果"

## 8. 指令汇总

| 指令 | 响应 cmd | 说明 |
|---|---|---|
| `cluster_status` | `cluster_status_res` | 获取引擎集群状态 |
| `get_redis` | `get_redis_res` | 获取 Redis 房间快照（调试用） |
| `create_room` | `create_room_res` | 创建房间（16 字符 hex ID） |
| `join_room` | `join_room_res` | 加入房间（最大 2 人） |
| `leave_room` | `leave_room_res` | 离开房间 |
| `list_rooms` | `list_rooms_res` | 获取等待中房间列表 |
| `match` | `match_res` | 进入匹配队列（FIFO，2 人成局） |
| `start_game` | `start_game_res`（仅错误）/ `game_started`（广播） | 开始对战 |
| `select_pokemon` | `select_pokemon_res` | 选择宝可梦（最多 6 只） |
| `battle_action` | `battle_action_res` | 提交对战动作 |
