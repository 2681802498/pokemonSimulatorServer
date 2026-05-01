#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>

struct Command
{
    std::string player_id;
    std::string action;
};

class Room
{
public:
    std::string room_id;
    int frame_count = 0;
    std::queue<Command> cmd_queue; // 待处理指令队列
    std::mutex mtx;                // 仅用于接收指令时的极短锁

    Room(std::string id) : room_id(id) {}

    // 每 50ms 被调用一次
    void Tick()
    {
        ProcessCommands();
        UpdatePhysics();
        frame_count++;
    }

private:
    void ProcessCommands()
    {
        std::lock_guard<std::mutex> lock(mtx);
        while (!cmd_queue.empty())
        {
            Command cmd = cmd_queue.front();
            // 处理逻辑... 比如移动、释放技能
            cmd_queue.pop();
        }
    }

    void UpdatePhysics()
    {
        // 这里的逻辑会根据 frame_count 持续增长
        // 比如：宝可梦位置更新、BUFF 结算
    }
};