#include "Battle/BattleActions.h"

BattleAction BattleAction::makeAttack(Pokemon* actor, Pokemon* target, const Move& move) {
    BattleAction action;
    action.type = ActionType::Attack;
    action.actor = actor;
    action.target = target;
    action.move = move;
    action.movePriority = move.getPriority();
    action.priority = actor ? actor->getSpeed() : 0;
    return action;
}

BattleAction BattleAction::makeSwitch(Pokemon* actor, int switchIndex) {
    BattleAction action;
    action.type = ActionType::Switch;
    action.actor = actor;
    action.switchIndex = switchIndex;
    action.movePriority = 0;
    action.priority = actor ? actor->getSpeed() : 0;
    return action;
}

BattleAction BattleAction::makeUseItem(Pokemon* actor, Pokemon* target, ItemType item) {
    BattleAction action;
    action.type = ActionType::UseItem;
    action.actor = actor;
    action.target = target;
    action.item = item;
    action.movePriority = 0;
    action.priority = actor ? actor->getSpeed() : 0;
    return action;
}

BattleAction BattleAction::makePass(Pokemon* actor) {
    BattleAction action;
    action.type = ActionType::Pass;
    action.actor = actor;
    action.movePriority = 0;
    action.priority = actor ? actor->getSpeed() : 0;
    return action;
}
