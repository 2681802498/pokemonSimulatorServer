#include "Battle/EventSystem.h"

#include <algorithm>

void EventSystem::recordEvent(const BattleEvent& event) {
    events.push_back(event);
}

const std::vector<BattleEvent>& EventSystem::getEvents() const {
    return events;
}

int EventSystem::registerActionEventCallback(ActionEventCallback callback) {
    if (!callback) {
        return 0;
    }

    const int callbackId = nextActionEventListenerId++;
    actionEventListeners.push_back(ActionEventListener{callbackId, std::move(callback)});
    return callbackId;
}

bool EventSystem::unregisterActionEventCallback(int callbackId) {
    const auto it = std::remove_if(actionEventListeners.begin(), actionEventListeners.end(), [callbackId](const ActionEventListener& listener) {
        return listener.id == callbackId;
    });

    if (it == actionEventListeners.end()) {
        return false;
    }

    actionEventListeners.erase(it, actionEventListeners.end());
    return true;
}

void EventSystem::emitActionEvent(const RuntimeActionEvent& event) const {
    for (const ActionEventListener& listener : actionEventListeners) {
        if (listener.callback) {
            listener.callback(event);
        }
    }
}

void EventSystem::clear() {
    events.clear();
}
