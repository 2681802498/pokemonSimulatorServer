package store

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

type RedisStore struct {
	client *redis.Client
	ttl    time.Duration
}

type RoomSnapshot struct {
	RoomID          string                              `json:"room_id"`
	Status          int                                 `json:"status"`
	NodeID          int                                 `json:"node_id"`
	Players         []string                            `json:"players"`
	ReadyPlayers    map[string]bool                     `json:"ready_players"`
	SelectedPokemon map[string][]map[string]interface{} `json:"selected_pokemon"`
	UpdatedAt       int64                               `json:"updated_at"`
}

var ErrNotFound = errors.New("not found")

// 新建redis存储，连接失败会返回错误
func NewRedisStore(addr string, password string, db int, ttl time.Duration) (*RedisStore, error) {
	client := redis.NewClient(&redis.Options{
		Addr:     addr,
		Password: password,
		DB:       db,
	})

	store := &RedisStore{client: client, ttl: ttl}
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	if err := store.Ping(ctx); err != nil {
		_ = client.Close()
		return nil, err
	}

	return store, nil
}

// Ping用于检查Redis连接是否正常
func (s *RedisStore) Ping(ctx context.Context) error {
	if err := s.client.Ping(ctx).Err(); err != nil {
		return fmt.Errorf("redis ping failed: %w", err)
	}
	return nil
}

// Close用于关闭Redis连接，释放资源
func (s *RedisStore) Close() error {
	if s.client == nil {
		return nil
	}
	return s.client.Close()
}

// SaveRoomSnapshot将房间快照保存到Redis中，使用JSON格式存储，并设置过期时间
func (s *RedisStore) SaveRoomSnapshot(ctx context.Context, snapshot RoomSnapshot) error {
	if snapshot.RoomID == "" {
		return errors.New("room_id is required")
	}
	if snapshot.UpdatedAt == 0 {
		snapshot.UpdatedAt = time.Now().Unix()
	}

	payload, err := json.Marshal(snapshot)
	if err != nil {
		return fmt.Errorf("marshal room snapshot failed: %w", err)
	}

	if err := s.client.Set(ctx, s.roomKey(snapshot.RoomID), payload, s.ttl).Err(); err != nil {
		return fmt.Errorf("save room snapshot failed: %w", err)
	}

	return nil
}

// LoadRoomSnapshot从Redis中加载房间快照，返回RoomSnapshot结构体，如果未找到则返回ErrNotFound
func (s *RedisStore) LoadRoomSnapshot(ctx context.Context, roomID string) (*RoomSnapshot, error) {
	if roomID == "" {
		return nil, errors.New("room_id is required")
	}

	raw, err := s.client.Get(ctx, s.roomKey(roomID)).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return nil, ErrNotFound
		}
		return nil, fmt.Errorf("load room snapshot failed: %w", err)
	}

	var snapshot RoomSnapshot
	if err := json.Unmarshal([]byte(raw), &snapshot); err != nil {
		return nil, fmt.Errorf("unmarshal room snapshot failed: %w", err)
	}

	return &snapshot, nil
}

// ListRoomSnapshots使用SCAN命令遍历所有以"room:"开头的键，加载并返回所有房间快照的列表
func (s *RedisStore) ListRoomSnapshots(ctx context.Context) ([]RoomSnapshot, error) {
	const scanCount int64 = 100

	var (
		cursor    uint64
		snapshots []RoomSnapshot
	)

	for {
		keys, nextCursor, err := s.client.Scan(ctx, cursor, "room:*", scanCount).Result()
		if err != nil {
			return nil, fmt.Errorf("scan room snapshots failed: %w", err)
		}

		for _, key := range keys {
			raw, getErr := s.client.Get(ctx, key).Result()
			if getErr != nil {
				if errors.Is(getErr, redis.Nil) {
					continue
				}
				return nil, fmt.Errorf("load room snapshot key=%s failed: %w", key, getErr)
			}

			var snapshot RoomSnapshot
			if unmarshalErr := json.Unmarshal([]byte(raw), &snapshot); unmarshalErr != nil {
				return nil, fmt.Errorf("unmarshal room snapshot key=%s failed: %w", key, unmarshalErr)
			}

			if snapshot.RoomID == "" {
				continue
			}

			snapshots = append(snapshots, snapshot)
		}

		//cursor是 Redis 提供的迭代器，每次仅扫描一部分，nextCursor为0表示扫描完成
		cursor = nextCursor
		if cursor == 0 {
			break
		}
	}

	return snapshots, nil
}

// DeleteRoomSnapshot从Redis中删除指定房间ID的快照
func (s *RedisStore) DeleteRoomSnapshot(ctx context.Context, roomID string) error {
	if roomID == "" {
		return errors.New("room_id is required")
	}

	if err := s.client.Del(ctx, s.roomKey(roomID)).Err(); err != nil {
		return fmt.Errorf("delete room snapshot failed: %w", err)
	}

	return nil
}

// SaveSessionBinding将用户ID和房间ID的绑定关系保存到Redis中，设置过期时间，以便在用户断线后自动清理绑定信息
func (s *RedisStore) SaveSessionBinding(ctx context.Context, userID string, roomID string) error {
	if userID == "" {
		return errors.New("user_id is required")
	}
	if roomID == "" {
		return errors.New("room_id is required")
	}

	if err := s.client.Set(ctx, s.sessionKey(userID), roomID, s.ttl).Err(); err != nil {
		return fmt.Errorf("save session binding failed: %w", err)
	}

	return nil
}

// LoadSessionBinding从Redis中加载指定用户ID的房间绑定关系，返回房间ID，如果未找到则返回ErrNotFound
func (s *RedisStore) LoadSessionBinding(ctx context.Context, userID string) (string, error) {
	if userID == "" {
		return "", errors.New("user_id is required")
	}

	roomID, err := s.client.Get(ctx, s.sessionKey(userID)).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return "", ErrNotFound
		}
		return "", fmt.Errorf("load session binding failed: %w", err)
	}

	return roomID, nil
}

// DeleteSessionBinding从Redis中删除指定用户ID的房间绑定关系，通常在用户断线或离开房间时调用
func (s *RedisStore) DeleteSessionBinding(ctx context.Context, userID string) error {
	if userID == "" {
		return errors.New("user_id is required")
	}

	if err := s.client.Del(ctx, s.sessionKey(userID)).Err(); err != nil {
		return fmt.Errorf("delete session binding failed: %w", err)
	}

	return nil
}

// roomKey和sessionKey是Redis键的生成函数，分别用于生成房间快照和会话绑定的键，确保键的命名规范和唯一性
func (s *RedisStore) roomKey(roomID string) string {
	return "room:" + roomID
}

// sessionKey生成会话绑定的Redis键，格式为"session:{userID}"，用于存储用户ID和房间ID的绑定关系
func (s *RedisStore) sessionKey(userID string) string {
	return "session:" + userID
}
