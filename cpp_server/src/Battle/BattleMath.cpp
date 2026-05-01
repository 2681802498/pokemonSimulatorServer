#include "Battle/BattleMath.h"

#include "Battle/Pokemon.h"

#include <algorithm>

int heavySlamPowerByWeightRatio(int attackerWeight, int defenderWeight) {
    const int safeDefenderWeight = std::max(1, defenderWeight);
    const float ratio = static_cast<float>(attackerWeight) / static_cast<float>(safeDefenderWeight);
    if (ratio >= 5.0f) return 120;
    if (ratio >= 4.0f) return 100;
    if (ratio >= 3.0f) return 80;
    if (ratio >= 2.0f) return 60;
    return 40;
}

float stageMultiplier(int stage) {
    if (stage >= 0) {
        return static_cast<float>(2 + stage) / 2.0f;
    }
    return 2.0f / static_cast<float>(2 - stage);
}

bool hasMajorStatus(const Pokemon* pokemon) {
    if (!pokemon) {
        return false;
    }
    return pokemon->hasStatus(StatusType::Burn)
        || pokemon->hasStatus(StatusType::Freeze)
        || pokemon->hasStatus(StatusType::Paralysis)
        || pokemon->hasStatus(StatusType::Poison)
        || pokemon->hasStatus(StatusType::Sleep)
        || pokemon->hasStatus(StatusType::ToxicPoison);
}

bool isSheerForceBoostedMove(const Move& move) {
    if (move.getCategory() == Category::Status || move.getEffectChance() <= 0) {
        return false;
    }

    switch (move.getEffect()) {
        case MoveEffect::Burn:
        case MoveEffect::Poison:
        case MoveEffect::Paralyze:
        case MoveEffect::Freeze:
        case MoveEffect::Flinch:
        case MoveEffect::Confuse:
            return true;
        case MoveEffect::StatChange:
            return move.getEffectParam1() > 0;
        default:
            return false;
    }
}

float criticalHitChanceForStage(int stage) {
    if (stage >= 3) {
        return 1.0f;
    }
    if (stage == 2) {
        return 0.5f;
    }
    if (stage == 1) {
        return 0.125f;
    }
    return 1.0f / 24.0f;
}