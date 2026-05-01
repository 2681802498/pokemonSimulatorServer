#include "Battle/Status.h"
#include "Battle/Pokemon.h"
#include <algorithm>

StatusEffect getStatusEffect(StatusType type) {
    switch (type) {
        case StatusType::Burn:
            return {StatusType::Burn, -1, "Burn",
                    [](Pokemon* self) { // onTurnStart: damage
                        if (self) {
                            int damage = std::max(1, self->getMaxHP() / 16);
                            self->setCurrentHP(self->getCurrentHP() - damage);
                        }
                    },
                    nullptr, // onTurnEnd
                    nullptr  // canMove
            };

        case StatusType::Freeze:
            return {StatusType::Freeze, -1, "Freeze",
                    nullptr, // onTurnStart
                    nullptr, // onTurnEnd
                    [](Pokemon* self) -> bool { // canMove: 20% chance to thaw
                        if (self) {
                            // Placeholder: always frozen for now, implement thaw logic later
                            return false;
                        }
                        return true;
                    }
            };

        case StatusType::Paralysis:
            return {StatusType::Paralysis, -1, "Paralysis",
                    nullptr, // onTurnStart
                    nullptr, // onTurnEnd
                    [](Pokemon* self) -> bool { // canMove: 25% chance to fail
                        if (self) {
                            // Placeholder: always can move for now
                            return true;
                        }
                        return true;
                    }
            };

        case StatusType::Poison:
            return {StatusType::Poison, -1, "Poison",
                    [](Pokemon* self) { // onTurnStart: damage
                        if (self) {
                            int damage = std::max(1, self->getMaxHP() / 8);
                            self->setCurrentHP(self->getCurrentHP() - damage);
                        }
                    },
                    nullptr, // onTurnEnd
                    nullptr  // canMove
            };

        case StatusType::Sleep:
            return {StatusType::Sleep, 1, "Sleep", // Duration will be set randomly 1-3 turns
                    nullptr, // onTurnStart
                    nullptr, // onTurnEnd
                    [](Pokemon* self) -> bool { // canMove: false while asleep
                        return false;
                    }
            };

        case StatusType::Flinch:
            return {StatusType::Flinch, 1, "Flinch",
                    nullptr,
                    nullptr,
                    [](Pokemon* self) -> bool {
                        return false;
                    }
            };

        case StatusType::ToxicPoison:
            return {StatusType::ToxicPoison, -1, "Toxic Poison",
                    [](Pokemon* self) { // onTurnStart: increasing damage
                        if (self) {
                            static int toxicCounter = 1;
                            int damage = std::max(1, self->getMaxHP() * toxicCounter / 16);
                            self->setCurrentHP(self->getCurrentHP() - damage);
                            toxicCounter++;
                        }
                    },
                    nullptr, // onTurnEnd
                    nullptr  // canMove
            };

        default:
            return {StatusType::None, 0, "None", nullptr, nullptr, nullptr};
    }
}

std::string getStatusName(StatusType type) {
    switch (type) {
        case StatusType::Burn: return "Burn";
        case StatusType::Freeze: return "Freeze";
        case StatusType::Paralysis: return "Paralysis";
        case StatusType::Poison: return "Poison";
        case StatusType::Sleep: return "Sleep";
        case StatusType::Flinch: return "Flinch";
        case StatusType::ToxicPoison: return "Toxic Poison";
        default: return "None";
    }
}
