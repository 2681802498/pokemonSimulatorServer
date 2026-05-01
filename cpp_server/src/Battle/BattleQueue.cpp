#include "Battle/BattleQueue.h"
#include <algorithm>

void BattleQueue::push(const BattleAction& action, bool trickRoomEnabled) {
    trickRoom = trickRoomEnabled;
    actions.push_back(QueuedAction{action, nextEnqueueOrder++});
}

BattleAction BattleQueue::pop() {
    if (actions.empty()) {
        return BattleAction::makePass(nullptr);
    }
    std::sort(actions.begin(), actions.end(), [this](const QueuedAction& a, const QueuedAction& b) {
        if (a.action.movePriority != b.action.movePriority) {
            return a.action.movePriority > b.action.movePriority;
        }
        if (a.action.priority != b.action.priority) {
            if (trickRoom) {
                return a.action.priority < b.action.priority;
            }
            return a.action.priority > b.action.priority;
        }
        return a.enqueueOrder < b.enqueueOrder;
    });
    BattleAction front = actions.front().action;
    actions.erase(actions.begin());
    return front;
}

bool BattleQueue::empty() const {
    return actions.empty();
}

void BattleQueue::clear() {
    actions.clear();
    nextEnqueueOrder = 0;
}

void BattleQueue::setTrickRoom(bool enable) {
    trickRoom = enable;
}
