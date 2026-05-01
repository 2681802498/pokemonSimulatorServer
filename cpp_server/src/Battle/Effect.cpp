#include "Battle/Effect.h"
#include "Battle/Pokemon.h"
#include "Battle/Status.h"
#include <algorithm>

FieldEffect getFieldEffect(FieldEffectType type) {
    switch (type) {
        case FieldEffectType::ToxicSpikes:
            return {FieldEffectType::ToxicSpikes, -1, "Toxic Spikes",
                    [](Pokemon* target) { // onEntry: poison if not flying/levitating
                        if (target && target->getType1() != Type::Flying && target->getType2() != Type::Flying) {
                            // Placeholder: set status to Poison
                            target->addStatus(StatusType::Poison, -1); // Permanent until cured
                        }
                    },
                    nullptr // onTurnEnd
            };

        case FieldEffectType::FireSpin:
            return {FieldEffectType::FireSpin, 4, "Fire Spin", // Lasts 4-5 turns
                    nullptr, // onEntry
                    [](Pokemon* target) { // onTurnEnd: damage
                        if (target) {
                            int damage = std::max(1, target->getMaxHP() / 8);
                            target->setCurrentHP(target->getCurrentHP() - damage);
                        }
                    }
            };

        case FieldEffectType::LeechSeed:
            return {FieldEffectType::LeechSeed, -1, "Leech Seed",
                    nullptr, // onEntry
                    [](Pokemon* target) { // onTurnEnd: drain HP
                        if (target) {
                            int drain = std::max(1, target->getMaxHP() / 8);
                            target->setCurrentHP(target->getCurrentHP() - drain);
                            // Placeholder: heal the user, but need to know who planted it
                        }
                    }
            };

        case FieldEffectType::StealthRock:
            return {FieldEffectType::StealthRock, -1, "Stealth Rock",
                    [](Pokemon* target) { // onEntry: damage based on type
                        if (target) {
                            float multiplier = 1.0f;
                            // Simplified: damage based on weakness to Rock
                            if (target->getType1() == Type::Flying || target->getType1() == Type::Bug ||
                                target->getType1() == Type::Fire || target->getType1() == Type::Ice) {
                                multiplier = 2.0f;
                            }
                            int damage = static_cast<int>(target->getMaxHP() * 0.125f * multiplier);
                            target->setCurrentHP(target->getCurrentHP() - damage);
                        }
                    },
                    nullptr // onTurnEnd
            };

        default:
            return {FieldEffectType::None, 0, "None", nullptr, nullptr};
    }
}

std::string getFieldEffectName(FieldEffectType type) {
    switch (type) {
        case FieldEffectType::ToxicSpikes: return "Toxic Spikes";
        case FieldEffectType::FireSpin: return "Fire Spin";
        case FieldEffectType::LeechSeed: return "Leech Seed";
        case FieldEffectType::StealthRock: return "Stealth Rock";
        default: return "None";
    }
}
