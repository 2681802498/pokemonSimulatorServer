#pragma once

#include <string>
#include <functional>

class Pokemon;

enum class StatusType {
    None,
    Burn,
    Freeze,
    Paralysis,
    Poison,
    Sleep,
    Flinch,
    ToxicPoison,
    Confusion,
    Count
};

struct StatusEffect {
    StatusType type;
    int duration; // For sleep, etc.
    std::string name;
    std::function<void(Pokemon* self)> onTurnStart; // Effect applied at turn start
    std::function<void(Pokemon* self)> onTurnEnd;   // Effect applied at turn end
    std::function<bool(Pokemon* self)> canMove;     // Check if can move
};

StatusEffect getStatusEffect(StatusType type);
std::string getStatusName(StatusType type);
