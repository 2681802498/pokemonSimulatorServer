#pragma once

#include "BattleActions.h"
#include <functional>
#include <string>
#include <vector>

struct ActionLog {
    int turnNumber = 0;
    std::string actorName;
    std::string targetName;
    std::string actionName;
    std::string itemName;
};

struct BattleEvent {
    int turnNumber = 0;
    ActionLog sideAAction;
    ActionLog sideBAction;
};

enum class RuntimeEventPhase {
    BeforeActionResolve,
    AfterActionResolve,
};

struct RuntimeActionEvent {
    RuntimeEventPhase phase = RuntimeEventPhase::BeforeActionResolve;
    int turnNumber = 0;
    BattleAction action;
};

class EventSystem {
public:
    using ActionEventCallback = std::function<void(const RuntimeActionEvent&)>;

    void recordEvent(const BattleEvent& event);
    const std::vector<BattleEvent>& getEvents() const;
    int registerActionEventCallback(ActionEventCallback callback);
    bool unregisterActionEventCallback(int callbackId);
    void emitActionEvent(const RuntimeActionEvent& event) const;
    void clear();

private:
    struct ActionEventListener {
        int id = 0;
        ActionEventCallback callback;
    };

    std::vector<BattleEvent> events;
    std::vector<ActionEventListener> actionEventListeners;
    int nextActionEventListenerId = 1;
};
