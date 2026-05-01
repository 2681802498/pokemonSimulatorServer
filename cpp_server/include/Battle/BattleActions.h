#pragma once

#include "Pokemon.h"
#include "Moves.h"
#include "Items.h"

enum class ActionType {
    Attack,
    Switch,
    UseItem,
    Pass
};

struct BattleAction {
    ActionType type = ActionType::Pass;
    Pokemon* actor = nullptr;
    Pokemon* target = nullptr;
    Move move;
    ItemType item = ItemType::None;
    int switchIndex = -1;
    int priority = 0;
    int movePriority = 0;

    BattleAction() = default;
    static BattleAction makeAttack(Pokemon* actor, Pokemon* target, const Move& move);
    static BattleAction makeSwitch(Pokemon* actor, int switchIndex);
    static BattleAction makeUseItem(Pokemon* actor, Pokemon* target, ItemType item);
    static BattleAction makePass(Pokemon* actor);
};
