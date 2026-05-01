# 🎮 Game Server 协议文档（WebSocket）

## 一、基础通信结构


### 1. 客户端请求格式

```json
{
  "cmd": "string",   // 指令名称
  "data": {}         // 业务数据（JSON对象）
}
````

---

### 2. 服务端响应格式

```json
{
  "cmd": "string",   // 指令名称 + "_res"
  "code": 0,         // 状态码
  "msg": "string",   // 描述信息
  "data": {}         // 返回数据
}
```

---

## 二、错误码定义

### 1. 通用错误码

| Code  | 含义 |
|------|------|
| 0 | 成功 |
| 10001 | 服务器内部错误 |
| 10002 | 参数错误 |

---

### 2. 指令 / 协议错误码

| Code  | 含义 |
|------|------|
| 20001 | 指令无效 |
| 20002 | 指令重复 |
| 20003 | 数据格式错误 |

---

### 3. RPC 错误码

| Code  | 含义 |
|------|------|
| 30001 | C++ RPC 服务异常 |

---

### 4. 房间系统错误码

| Code  | 含义 |
|------|------|
| 40001 | 房间不存在 |
| 40002 | 房间已满 |
| 40003 | 房间已开始 |
| 40004 | 房间已结束 |
| 40005 | 玩家不在房间中 |

---


## 三、通用指令

### 1. Hello（测试 RPC 通信）

#### 请求

```json
{
  "cmd": "hello",
  "data": {}
}
```

#### 响应

```json
{
  "cmd": "hello_res",
  "code": 0,
  "msg": "ok",
  "data": null
}
```

---

## 四、房间系统

### 1. 创建房间

#### 请求

```json
{
  "cmd": "create_room",
  "data": {}
}
```

#### 响应

```json
{
  "cmd": "create_room_res",
  "code": 0,
  "msg": "创建成功",
  "data": {
    "room_id": "123456"
  }
}
```

---

### 2. 加入房间

#### 请求

```json
{
  "cmd": "join_room",
  "data": {
    "room_id": "123456"
  }
}
```

#### 响应

```json
{
  "cmd": "join_room_res",
  "code": 0,
  "msg": "加入成功",
  "data": null
}
```

---

### 3. 离开房间

#### 请求

```json
{
  "cmd": "leave_room",
  "data": {}
}
```

#### 响应

```json
{
  "cmd": "leave_room_res",
  "code": 0,
  "msg": "已退出房间",
  "data": null
}
```

---

### 4. 获取房间列表

#### 请求

```json
{
  "cmd": "list_rooms",
  "data": {}
}
```

#### 响应

```json
{
  "cmd": "list_rooms_res",
  "code": 0,
  "msg": "获取成功",
  "data": [
    {
      "room_id": "123456",
      "player_count": 1,
      "max_player": 2,
      "status": "waiting"
    }
  ]
}
```

---

## 五、匹配系统

### 1. 进入匹配队列

#### 请求

```json
{
  "cmd": "match",
  "data": {}
}
```

#### 响应

```json
{
  "cmd": "match_res",
  "code": 0,
  "msg": "已进入匹配队列",
  "data": null
}
```

---

### （建议扩展）

#### 匹配成功推送（服务端主动）

```json
{
  "cmd": "match_success_push",
  "code": 0,
  "msg": "匹配成功",
  "data": {
    "room_id": "123456"
  }
}
```

---

## 六、设计规范（强烈建议）

### 1. 命名规范

* 请求：`xxx`
* 响应：`xxx_res`
* 推送：`xxx_push`

---

### 2. data 字段规范

* 无数据：`null`
* 必须为 JSON 对象或数组
* 禁止直接传 string / number

---

### 3. 错误处理规范

* 所有错误必须返回：

```json
{
  "code": 非0,
  "msg": "错误原因"
}
```

---

### 4. 扩展建议（未来）

#### 战斗系统（预留）

* `start_battle`
* `battle_action`
* `battle_state_push`

#### 玩家系统（预留）

* `login`
* `logout`
* `player_info`

---

## 七、版本建议（可选）

```json
{
  "version": "1.0"
}
```

---

# ✅ 总结

该协议具备：

* 清晰结构（cmd + data）
* 统一返回格式
* 可扩展性强
* 适用于 WebSocket + RPC 架构

```
```
