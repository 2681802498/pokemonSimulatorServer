#pragma once

#include "BattleActions.h"
#include <cstddef>
#include <vector>

class BattleQueue {
public:
    void push(const BattleAction& action, bool trickRoom = false);
    BattleAction pop();
    bool empty() const;
    void clear();
    void setTrickRoom(bool enable);

private:
    struct QueuedAction {
        BattleAction action;
        std::size_t enqueueOrder = 0;
    };

    std::vector<QueuedAction> actions;
    std::size_t nextEnqueueOrder = 0;
    bool trickRoom = false;
};
