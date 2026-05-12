package main

import (
	"context"
	"encoding/json"
	"go-server/configs"
	"go-server/internal/engine"
	"go-server/internal/handler"
	"go-server/internal/room"
	"go-server/internal/store"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func initLogger() {
	var handler slog.Handler
	level := slog.LevelDebug

	if os.Getenv("APP_ENV") == "production" {
		// 生产环境：全量信息 JSON，包含完整日期
		handler = slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo})
	} else {
		// 开发环境：精简时间 Text
		handler = slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
			Level: level,
			ReplaceAttr: func(groups []string, a slog.Attr) slog.Attr {
				if a.Key == slog.TimeKey {
					return slog.String(a.Key, a.Value.Time().Format("15:04:05"))
				}
				return a
			},
		})
	}

	slog.SetDefault(slog.New(handler))
}

func initRedis() *store.RedisStore {
	redisStore, err := store.NewRedisStore(
		configs.RedisAddr,
		configs.RedisPassword,
		configs.RedisDB,
		configs.RedisTTL,
	)
	if err != nil {
		slog.Error("Redis 初始化失败", "error", err)
		os.Exit(1)
	}

	slog.Info("Redis 初始化成功", "addr", configs.RedisAddr, "db", configs.RedisDB)
	return redisStore
}

func initEngineConn() *engine.EngineInstance {
	return engine.NewEngineInstance()
}

func initGameRouter(rm *room.RoomManager, engineConn *engine.EngineInstance) *handler.Router {
	gameRouter := handler.NewRouter()
	handler.InitGameRouters(gameRouter, rm, engineConn)
	return gameRouter
}

func initRoomManager(redisStore *store.RedisStore, engineConn *engine.EngineInstance) *room.RoomManager {
	rm := room.NewRoomManager(redisStore, engineConn)
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := rm.RestoreFromStore(ctx, 0); err != nil {
		slog.Error("Redis 房间恢复失败", "error", err)
	}
	rm.StartMatchWorker()
	return rm
}

func initHTTPServer(gameRouter *handler.Router, rm *room.RoomManager, engineConn *engine.EngineInstance, redisStore *store.RedisStore) *http.Server {
	server := &http.Server{Addr: ":8080", Handler: nil}

	http.HandleFunc("/ws", func(w http.ResponseWriter, r *http.Request) {
		handler.HandleWS(w, r, gameRouter, rm)
	})
	http.HandleFunc("/debug/engine", func(w http.ResponseWriter, r *http.Request) {
		status := engineConn.GetStatus()
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(status)
	})
	http.HandleFunc("/debug/room_snapshot", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}

		roomID := r.URL.Query().Get("room_id")
		if roomID == "" {
			http.Error(w, "missing room_id", http.StatusBadRequest)
			return
		}

		ctx, cancel := context.WithTimeout(r.Context(), 2*time.Second)
		defer cancel()

		snapshot, err := redisStore.LoadRoomSnapshot(ctx, roomID)
		if err != nil {
			if err == store.ErrNotFound {
				http.Error(w, "room snapshot not found", http.StatusNotFound)
				return
			}
			http.Error(w, "failed to load room snapshot", http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(snapshot)
	})

	return server
}

func startHTTPServer(server *http.Server, engineConn *engine.EngineInstance) {
	go func() {
		slog.Info("游戏后端已启动", "port", 8080, "cpp_instances", engineConn.GetStatus().Total)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("服务异常退出", "error", err)
		}
	}()
}

func main() {
	initLogger()
	redisStore := initRedis()
	defer func() {
		if err := redisStore.Close(); err != nil {
			slog.Error("Redis 关闭失败", "error", err)
		}
	}()

	engineConn := initEngineConn()

	rm := initRoomManager(redisStore, engineConn)

	// 注册房间重新分配回调
	engineConn.SetReassignRoomsFunc(func() {
		if moved, skipped := rm.ReassignRunningRoomsFromUnavailableNodes(); moved > 0 || skipped > 0 {
			slog.Info("房间重新分配完成", "moved", moved, "skipped", skipped)
		}
	})

	gameRouter := initGameRouter(rm, engineConn)

	// 1. 准备信号监听
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)

	server := initHTTPServer(gameRouter, rm, engineConn, redisStore)
	startHTTPServer(server, engineConn)

	// 3. 阻塞在这里，等待信号
	// 程序运行到这里会“停住”，直到你按 Ctrl+C
	sig := <-sigChan
	slog.Info("接收到关闭信号", "signal", sig.String())

	// 4. 开始执行优雅关闭
	slog.Info("正在执行 Shutdown 流程...")

	// 先关闭 HTTP 服务，不再接收新连接
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := server.Shutdown(ctx); err != nil {
		slog.Error("HTTP Server 关闭失败", "error", err)
	}

	// 调用你 engine 里的逻辑，关闭所有 C++ 进程
	engineConn.Shutdown()

	slog.Info("服务已完全退出")
}
