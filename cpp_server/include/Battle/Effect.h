#pragma once

#include <string>
#include <functional>

class Pokemon;

enum class FieldEffectType {
    None,
    ToxicSpikes,
    FireSpin,
    LeechSeed,
    StealthRock,
    Count
};

struct FieldEffect {
    FieldEffectType type;
    int duration; // -1 for permanent until removed
    std::string name;
    std::function<void(Pokemon* target)> onEntry; // Effect when entering the field
    std::function<void(Pokemon* target)> onTurnEnd; // Effect at end of turn
};

FieldEffect getFieldEffect(FieldEffectType type);
std::string getFieldEffectName(FieldEffectType type);
