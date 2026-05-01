#include "Battle/MoveEffectHandler.h"

#include "Battle/Battle.h"
#include "Battle/PRNG.h"

#include <cmath>

namespace {
bool isMajorStatus(StatusType status) {
    switch (status) {
        case StatusType::Burn:
        case StatusType::Freeze:
        case StatusType::Paralysis:
        case StatusType::Poison:
        case StatusType::Sleep:
        case StatusType::ToxicPoison:
            return true;
        default:
            return false;
    }
}
}

namespace MoveEffectHandlers {

void applyStandardMoveEffect(Battle& battle, Pokemon* attacker, Pokemon* defender, const Move& move) {
    if (!attacker || !defender) {
        return;
    }

    MoveEffect effect = move.getEffect();
    if (effect == MoveEffect::None) {
        return;
    }

    int chance = move.getEffectChance();
    if (chance < 100) {
        if (PRNG::nextFloat(0.0f, 100.0f) > static_cast<float>(chance)) {
            return;
        }
    }

    if (move.getName() == "Dragon Dance") {
        attacker->changeStatStage(StatIndex::Attack, 1);
        attacker->changeStatStage(StatIndex::Speed, 1);
        battle.triggerItemEffect(attacker, ItemTrigger::OnStatChange, defender);
        return;
    }

    switch (effect) {
        case MoveEffect::Paralyze:
            if (resolveStatusImmunity(defender->getAbility(), StatusType::Paralysis)) {
                break;
            }
            defender->addStatus(StatusType::Paralysis);
            break;
        case MoveEffect::Sleep:
            if (resolveStatusImmunity(defender->getAbility(), StatusType::Sleep)) {
                break;
            }
            defender->addStatus(StatusType::Sleep);
            break;
        case MoveEffect::Freeze:
            defender->addStatus(StatusType::Freeze);
            break;
        case MoveEffect::Burn:
            defender->addStatus(StatusType::Burn);
            break;
        case MoveEffect::Poison:
            if (move.getEffectParam1() == 2) {
                if (resolveStatusImmunity(defender->getAbility(), StatusType::ToxicPoison)) {
                    break;
                }
                defender->addStatus(StatusType::ToxicPoison);
            } else {
                if (resolveStatusImmunity(defender->getAbility(), StatusType::Poison)) {
                    break;
                }
                defender->addStatus(StatusType::Poison);
            }
            break;
        case MoveEffect::Confuse:
            defender->addStatus(StatusType::Confusion);
            break;
        case MoveEffect::Flinch:
            if (resolveStatusImmunity(defender->getAbility(), StatusType::Flinch)) {
                break;
            }
            defender->addStatus(StatusType::Flinch, 1);
            break;
        case MoveEffect::Recoil: {
            int recoilDamage = move.getEffectParam1();
            if (recoilDamage > 0) {
                int actualRecoil = attacker->getMaxHP() * recoilDamage / 100;
                if (actualRecoil < 1) {
                    actualRecoil = 1;
                }
                attacker->setCurrentHP(attacker->getCurrentHP() - actualRecoil);
            }
            break;
        }
        case MoveEffect::Drain: {
            int drainPercentage = move.getEffectParam1();
            if (drainPercentage > 0) {
                int damageDealt = battle.calculateDamage(attacker, defender, move);
                int drainAmount = damageDealt * drainPercentage / 100;
                if (drainAmount < 1) {
                    drainAmount = 1;
                }
                attacker->setCurrentHP(attacker->getCurrentHP() + drainAmount);
            }
            break;
        }
        case MoveEffect::StatChange: {
            const int encodedIndex = move.getEffectParam1();
            const int changeAmount = move.getEffectParam2();
            const bool affectSelf = encodedIndex < 0;
            const int absIndex = std::abs(encodedIndex);
            if (changeAmount == 0 || absIndex <= 0 || absIndex >= static_cast<int>(StatIndex::Count)) {
                break;
            }

            Pokemon* target = affectSelf ? attacker : defender;
            Pokemon* itemOpponent = affectSelf ? defender : attacker;

            if (!affectSelf && changeAmount < 0) {
                Side* targetSide = Battle::findSideForPokemon(battle, target);
                if (targetSide && targetSide->hasMist()) {
                    break;
                }
            }

            if (!affectSelf && changeAmount < 0) {
                const AbilityType targetAbility = target->getAbility();
                if (abilityBlocksGenericStatDrops(targetAbility)) {
                    break;
                }
                if (absIndex == static_cast<int>(StatIndex::Attack) && abilityBlocksAttackDrops(targetAbility)) {
                    break;
                }
                if (abilityReflectsStatDrops(targetAbility)) {
                    attacker->changeStatStage(static_cast<StatIndex>(absIndex), changeAmount);
                    break;
                }
            }

            const int beforeStage = target->getStatStage(static_cast<StatIndex>(absIndex));
            target->changeStatStage(static_cast<StatIndex>(absIndex), changeAmount);
            const int afterStage = target->getStatStage(static_cast<StatIndex>(absIndex));

            if (!affectSelf && changeAmount < 0 && afterStage < beforeStage) {
                applyStatLoweredReaction(target->getAbility(), target);
            }

            if (afterStage != beforeStage) {
                battle.triggerItemEffect(target, ItemTrigger::OnStatChange, itemOpponent);
            }
            break;
        }
        case MoveEffect::Safeguard: {
            Side* side = Battle::findSideForPokemon(battle, attacker);
            if (!side) {
                break;
            }

            int protectCount = side->getProtectCount();
            float successRate = 1.0f;
            if (protectCount > 0) {
                for (int i = 0; i < protectCount; i++) {
                    successRate *= (1.0f / 3.0f);
                }
            }

            bool success = PRNG::nextFloat(0.0f, 1.0f) <= successRate;
            if (success) {
                attacker->setIsProtected(true);
                side->setProtectCount(protectCount + 1);
            } else {
                side->resetProtectCount();
            }
            break;
        }
        default:
            break;
    }
}

}  // namespace MoveEffectHandlers
