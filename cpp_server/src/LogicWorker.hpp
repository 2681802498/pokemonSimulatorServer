#pragma once
#include <thread>
#include <unordered_map>
#include <chrono>
#include "Room.hpp"
#include <iostream>

class LogicWorker
{
public:
    std::unordered_map<std::string, std::shared_ptr<Room>> rooms;
    std::mutex rooms_mtx;
    std::unique_ptr<std::thread> worker_thread;
    bool running = true;

    void Start()
    {
        worker_thread = std::make_unique<std::thread>([this]()
                                                      {
            while (running) {
                auto start_time = std::chrono::steady_clock::now();

                // 遍历本线程管理的所有房间，执行 Tick
                for (auto& [id, room] : rooms) {
                    room->Tick();
                }

                // 维持 20FPS (50ms)
                auto end_time = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                if (elapsed.count() < 50) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50 - elapsed.count()));
                }
            } });
    }

    void AddRoom(std::string id)
    {
        std::lock_guard<std::mutex> lock(rooms_mtx);
        rooms[id] = std::make_shared<Room>(id);
        std::cout << "[Worker] Room " << id << " added to thread " << std::this_thread::get_id() << std::endl;
    }

    ~LogicWorker()
    {
        running = false;
        if (worker_thread && worker_thread->joinable())
        {
            worker_thread->join();
        }
    }
};