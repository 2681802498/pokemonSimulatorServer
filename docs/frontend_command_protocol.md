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

### 后端返回

```json
{
  "cmd": "响应指令名",
  "code": 0,
  "msg": "说明",
  "data": {}
}
```

其中：

- `cmd`：指令名
- `data`：业务数据，部分指令可为空对象
- `code`：状态码，`0` 表示成功
- `msg`：返回说明
- `data`：返回数据

## 2. 错误码

| code | 含义 |
|---|---|
| 0 | 成功 |
| 10001 | 服务端错误 |
| 10002 | 参数错误 |
| 20001 | 非法发送指令 |
| 20002 | 重复发送 |
| 20003 | 数据非法 |
| 30001 | C++ RPC 错误 |
| 40001 | 房间不存在 |
| 40002 | 房间已满 |
| 40003 | 房间已开始 |
| 40004 | 房间已结束 |
| 40005 | 玩家不在房间中 |

## 3. 前端指令集

### 3.1 cluster_status

用途：获取集群状态。

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
  "data": {}
}
```

---

### 3.2 get_redis

用途：获取 Redis 中的房间快照数据。

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
    "rooms": []
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

---

### 3.3 create_room

用途：创建房间。

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
    "room_id": "ABC123"
  }
}
```

失败时可能返回：

```json
{
  "cmd": "create_room_res",
  "code": 30001,
  "msg": "算力集群暂不可用",
  "data": null
}
```

---

### 3.4 join_room

用途：加入指定房间。

前端发送：

```json
{
  "cmd": "join_room",
  "data": {
    "room_id": "ABC123"
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
  "msg": "房间已满",
  "data": null
}
```

---

### 3.5 leave_room

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

### 3.6 list_rooms

用途：获取当前房间列表。

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
  "data": []
}
```

---

### 3.7 match

用途：进入匹配队列。

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

---

### 3.8 start_game

用途：开始对战。

前端发送：

```json
{
  "cmd": "start_game",
  "data": {}
}
```

这个指令的成功结果不是直接返回在当前请求里，而是由服务端广播：

```json
{
  "cmd": "game_started",
  "code": 0,
  "msg": "游戏正式开始",
  "data": null
}
```

失败时会广播：

```json
{
  "cmd": "game_start_error",
  "code": 30001,
  "msg": "失败原因",
  "data": null
}
```

---

### 3.9 select_pokemon

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
      },
      {
        "species_id": 6,
        "level": 50,
        "nature": 10,
        "ability": 66,
        "item": 92,
        "ivs": {"hp": 31, "attack": 31, "defense": 31, "specialAttack": 31, "specialDefense": 31, "speed": 31},
        "evs": {"hp": 0, "attack": 0, "defense": 4, "specialAttack": 252, "specialDefense": 0, "speed": 252},
        "moves": [53, 9, 108, 241]
      },
      {
        "species_id": 9,
        "level": 50,
        "nature": 15,
        "ability": 67,
        "item": 86,
        "ivs": {"hp": 31, "attack": 31, "defense": 31, "specialAttack": 31, "specialDefense": 31, "speed": 31},
        "evs": {"hp": 252, "attack": 0, "defense": 4, "specialAttack": 252, "specialDefense": 0, "speed": 0},
        "moves": [56, 57, 58, 59]
      },
      {
        "species_id": 3,
        "level": 50,
        "nature": 15,
        "ability": 65,
        "item": 87,
        "ivs": {"hp": 31, "attack": 31, "defense": 31, "specialAttack": 31, "specialDefense": 31, "speed": 31},
        "evs": {"hp": 4, "attack": 0, "defense": 0, "specialAttack": 252, "specialDefense": 0, "speed": 252},
        "moves": [71, 72, 73, 74]
      },
      {
        "species_id": 1,
        "level": 50,
        "nature": 3,
        "ability": 47,
        "item": 86,
        "ivs": {"hp": 31, "attack": 31, "defense": 31, "specialAttack": 31, "specialDefense": 31, "speed": 31},
        "evs": {"hp": 252, "attack": 252, "defense": 0, "specialAttack": 0, "specialDefense": 4, "speed": 0},
        "moves": [75, 76, 77, 78]
      },
      {
        "species_id": 131,
        "level": 50,
        "nature": 3,
        "ability": 47,
        "item": 86,
        "ivs": {"hp": 31, "attack": 31, "defense": 31, "specialAttack": 31, "specialDefense": 31, "speed": 31},
        "evs": {"hp": 252, "attack": 252, "defense": 0, "specialAttack": 0, "specialDefense": 4, "speed": 0},
        "moves": [79, 80, 81, 82]
      }
    ]
  }
}
```

后端返回成功：

```json
{
  "cmd": "select_pokemon_res",
  "code": 0,
  "msg": "宝可梦选择成功",
  "data": null
}
```

随后可能广播：

```json
{
  "cmd": "player_ready",
  "code": 0,
  "msg": "玩家 XXX 已准备好",
  "data": null
}
```

如果双方都准备好，后端会继续广播：

```json
{
  "cmd": "game_started",
  "code": 0,
  "msg": "游戏正式开始",
  "data": null
}
```

常见失败返回：

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
  "msg": "必须选择 6 个宝可梦",
  "data": null
}
```

---

### 3.10 battle_action

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

说明：`data` 只要是合法 JSON 即可，具体字段由对战业务决定。

后端返回成功：

```json
{
  "cmd": "battle_action_res",
  "code": 0,
  "msg": "动作已提交",
  "data": {}
}
```

随后广播：

```json
{
  "cmd": "battle_state_push",
  "code": 0,
  "msg": "对战状态更新",
  "data": {}
}
```

常见失败返回：

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
  "code": 20003,
  "msg": "action 必须是合法 JSON",
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

## 4. 服务端主动推送消息

这些消息不是前端主动请求后立刻得到的普通响应，而是服务端在业务变化时主动发送：

| 推送 cmd | 含义 |
|---|---|
| `game_started` | 游戏开始 |
| `game_start_error` | 游戏开始失败 |
| `player_ready` | 某个玩家已准备好 |
| `battle_state_push` | 对战状态变化 |
| `room_status_change` | 房间状态变化 |
| `join_room` | 有玩家加入房间时广播 |

## 5. 未知指令

如果前端发送了未注册指令，后端会返回：

```json
{
  "cmd": "原cmd_res",
  "code": 20001,
  "msg": "未知指令",
  "data": null
}
```

## 6. 前端接入建议

前端可以按下面的规则处理消息：

1. `*_res` 视为请求响应
2. `*_push`、`game_started`、`player_ready`、`room_status_change` 视为服务端推送
3. 所有请求都走统一的 `cmd + data` 结构
4. 收到响应后根据 `code` 判断成功或失败

## 7. 结论

当前前端可直接使用的核心指令就是：

- `cluster_status`
- `get_redis`
- `create_room`
- `join_room`
- `leave_room`
- `list_rooms`
- `match`
- `start_game`
- `select_pokemon`
- `battle_action`

对应返回以 `_res` 结尾，另外还有若干服务端主动推送消息。
