#include "Moves.h"

#include "Battle/Battle.h"
#include "Battle/PRNG.h"
#include "Battle/Utils.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace {
std::string normalizeMoveName(const std::string& moveName) {
    std::string normalized;
    normalized.reserve(moveName.size());
    for (char ch : moveName) {
        if (ch == ' ' || ch == '-' || ch == '\'' || ch == '_') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

bool moveNameEquals(const Move& move, const char* expectedLowerNoPunct) {
    return normalizeMoveName(move.getName()) == expectedLowerNoPunct;
}

bool isSelfStatDropMove(const Move& move) {
    return moveNameEquals(move, "closecombat") || moveNameEquals(move, "superpower")
        || moveNameEquals(move, "overheat") || moveNameEquals(move, "leafstorm")
        || moveNameEquals(move, "dracometeor") || moveNameEquals(move, "fleurcannon")
        || moveNameEquals(move, "steelbeam") || moveNameEquals(move, "vcreate")
        || moveNameEquals(move, "clangoroussoulblaze");
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

void applySelfStatDropMove(Pokemon* attacker, const Move& move) {
    if (!attacker || !isSelfStatDropMove(move)) {
        return;
    }

    if (moveNameEquals(move, "closecombat") || moveNameEquals(move, "superpower")) {
        attacker->changeStatStage(StatIndex::Defense, -1);
        attacker->changeStatStage(StatIndex::SpecialDefense, -1);
        return;
    }

    if (moveNameEquals(move, "overheat") || moveNameEquals(move, "leafstorm") || moveNameEquals(move, "dracometeor")
        || moveNameEquals(move, "fleurcannon")) {
        attacker->changeStatStage(StatIndex::SpecialAttack, -2);
        return;
    }

    if (moveNameEquals(move, "steelbeam")) {
        attacker->setCurrentHP(attacker->getCurrentHP() - std::max(1, attacker->getMaxHP() / 2));
        return;
    }

    if (moveNameEquals(move, "vcreate")) {
        attacker->changeStatStage(StatIndex::Defense, -1);
        attacker->changeStatStage(StatIndex::SpecialDefense, -1);
        attacker->changeStatStage(StatIndex::Speed, -1);
        return;
    }

    if (moveNameEquals(move, "clangoroussoulblaze")) {
        attacker->changeStatStage(StatIndex::Attack, 1);
        attacker->changeStatStage(StatIndex::Defense, 1);
        attacker->changeStatStage(StatIndex::SpecialAttack, 1);
        attacker->changeStatStage(StatIndex::SpecialDefense, 1);
        attacker->changeStatStage(StatIndex::Speed, 1);
    }
}

void resetPokemonStatStages(Pokemon* pokemon) {
    if (!pokemon) {
        return;
    }

    constexpr std::array<StatIndex, 5> kResettableStats = {
        StatIndex::Attack,
        StatIndex::Defense,
        StatIndex::SpecialAttack,
        StatIndex::SpecialDefense,
        StatIndex::Speed,
    };

    for (StatIndex stat : kResettableStats) {
        const int currentStage = pokemon->getStatStage(stat);
        if (currentStage != 0) {
            pokemon->changeStatStage(stat, -currentStage);
        }
    }
}

int firstAvailableSwitchIndex(const Side& side) {
    const int activeIndex = side.getActiveIndex();
    const auto& team = side.getTeam();
    const int teamSize = side.getPokemonCount();

    for (int i = 0; i < teamSize; ++i) {
        if (i == activeIndex) {
            continue;
        }
        if (i >= 0 && i < static_cast<int>(team.size()) && team[i] && !team[i]->isFainted()) {
            return i;
        }
    }

    return -1;
}

void triggerEntryEffectsAfterSwitch(Battle& battle, Side& side) {
    Pokemon* newActive = side.getActivePokemon();
    if (!newActive) {
        return;
    }

    Pokemon* opponentPokemon = (&side == &battle.getSideA()) ? battle.getSideB().getActivePokemon() : battle.getSideA().getActivePokemon();
    battle.triggerAbility(newActive, Trigger::OnEntry, opponentPokemon);
    battle.triggerItemEffect(newActive, ItemTrigger::OnEntry, opponentPokemon);
}

bool forceSwitchSide(Battle& battle, Side& side) {
    const int nextIndex = firstAvailableSwitchIndex(side);
    if (nextIndex < 0) {
        return false;
    }

    if (!battle.switchPokemon(side, nextIndex)) {
        return false;
    }

    triggerEntryEffectsAfterSwitch(battle, side);
    return true;
}

bool canForceSwitchTarget(Battle& battle, Pokemon* defender) {
    return battle.canBeForcedToSwitch(defender);
}

bool applyConfusionMove(Battle& battle, Pokemon* defender, const Move& move) {
    if (!defender || defender->getSubstituteHP() > 0 || defender->hasStatus(StatusType::Confusion)) {
        return true;
    }

    defender->addStatus(StatusType::Confusion);
    // 这里直接记录状态变化，避免把行为留在 Battle.cpp
    (void)battle;
    return true;
}

Type findResistingTypeForAttack(Type attackType) {
    for (int candidate = 0; candidate < static_cast<int>(Type::Count); ++candidate) {
        const Type candidateType = static_cast<Type>(candidate);
        if (candidateType == Type::Count) {
            continue;
        }
        const float effectiveness = Utils::TypeEffectiveness[static_cast<int>(attackType)][candidate];
        if (effectiveness <= 0.5f) {
            return candidateType;
        }
    }
    return Type::Normal;
}
}  // namespace

Move::Move()
    : data{0, "", "", "", Type::Normal, Category::Status, 0, 100, 0, MoveEffect::None, 0, 0, 0, 0, Target::Opponent}, maxPP(0) {}

Move::Move(const MoveData& data)
    : data(data), maxPP(data.pp) {}

Move::Move(std::string name, Type type, Category category, int power, int accuracy, int pp)
    : data{0, std::move(name), "", "", type, category, power, accuracy, pp, MoveEffect::None, 0, 0, 0, 0, Target::Opponent}, maxPP(pp) {}

Move::Move(std::string name, Type type, Category category, int power, int accuracy, int pp, MoveEffect effect, int effectChance, int effectParam1, int effectParam2, int priority, Target target)
    : data{0, std::move(name), "", "", type, category, power, accuracy, pp, effect, effectChance, effectParam1, effectParam2, priority, target}, maxPP(pp) {}

Move::Move(const Move& other)
    : data(other.data), maxPP(other.maxPP) {}

Move& Move::operator=(const Move& other) {
    if (this != &other) {
        data = other.data;
        maxPP = other.maxPP;
    }
    return *this;
}

Move::~Move() {}

void initializeCoreMoveRules(Battle& battle) {
    auto& moveRuleRegistry = battle.moveRuleRegistry;
    auto& runtimeMoveState = battle.runtimeMoveState;

    auto registerProtectLikeRule = [&moveRuleRegistry](const std::string& name, const std::string& variant = std::string()) {
        moveRuleRegistry.registerRule(name, [variant](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
            Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
            if (!actorSide) {
                return true;
            }

            const int protectCount = actorSide->getProtectCount();
            float successRate = 1.0f;
            if (protectCount > 0) {
                for (int i = 0; i < protectCount; ++i) {
                    successRate *= (1.0f / 3.0f);
                }
            }

            const bool success = PRNG::nextFloat(0.0f, 1.0f) <= successRate;
            if (success) {
                attacker->setIsProtected(true);
                actorSide->setProtectCount(protectCount + 1);
                if (!variant.empty()) {
                    battleInstance.runtimeMoveState.protectionVariant[attacker] = variant;
                }
            } else {
                actorSide->resetProtectCount();
                battleInstance.runtimeMoveState.protectionVariant.erase(attacker);
            }
            return true;
        });
    };

    auto registerSideGuardLikeRule = [&moveRuleRegistry](const std::string& name, bool quickGuard) {
        moveRuleRegistry.registerRule(name, [quickGuard](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
            Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
            if (!actorSide) {
                return true;
            }

            const int protectCount = actorSide->getProtectCount();
            float successRate = 1.0f;
            if (protectCount > 0) {
                for (int i = 0; i < protectCount; ++i) {
                    successRate *= (1.0f / 3.0f);
                }
            }

            const bool success = PRNG::nextFloat(0.0f, 1.0f) <= successRate;
            if (success) {
                actorSide->setProtectCount(protectCount + 1);
                if (quickGuard) {
                    battleInstance.runtimeMoveState.quickGuardActive[actorSide] = true;
                } else {
                    battleInstance.runtimeMoveState.wideGuardActive[actorSide] = true;
                }
            } else {
                actorSide->resetProtectCount();
            }
            return true;
        });
    };

    auto registerRecoverLikeRule = [&moveRuleRegistry](const std::string& name) {
        moveRuleRegistry.registerRule(name, [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
            const int healAmount = std::max(1, attacker->getMaxHP() / 2);
            attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
            return true;
        });
    };

    auto registerEvasionDropRule = [&moveRuleRegistry](const std::string& name) {
        moveRuleRegistry.registerRule(name, [](Battle& battleInstance, Pokemon* attacker, Pokemon* defender, const Move&) {
            if (defender->getSubstituteHP() > 0) {
                return true;
            }

            Side* defenderSide = Battle::findSideForPokemon(battleInstance, defender);
            if (defenderSide && defenderSide->hasMist()) {
                return true;
            }

            const AbilityType defenderAbility = defender->getAbility();
            if (abilityBlocksEvasionDrops(defenderAbility)) {
                return true;
            }
            if (abilityReflectsStatDrops(defenderAbility) && attacker) {
                attacker->changeEvasionStage(1);
                return true;
            }

            defender->changeEvasionStage(1);
            return true;
        });
    };

    auto registerAccuracyDropRule = [&moveRuleRegistry](const std::string& name, int stages) {
        moveRuleRegistry.registerRule(name, [stages](Battle& battleInstance, Pokemon* attacker, Pokemon* defender, const Move&) {
            if (!defender || stages <= 0) {
                return true;
            }
            if (defender->getSubstituteHP() > 0) {
                return true;
            }

            Side* defenderSide = Battle::findSideForPokemon(battleInstance, defender);
            if (defenderSide && defenderSide->hasMist()) {
                return true;
            }

            const AbilityType defenderAbility = defender->getAbility();
            if (abilityBlocksAccuracyDrops(defenderAbility)) {
                return true;
            }
            if (abilityReflectsStatDrops(defenderAbility) && attacker) {
                attacker->changeAccuracyStage(-stages);
                return true;
            }

            defender->changeAccuracyStage(-stages);
            return true;
        });
    };

    moveRuleRegistry.registerRule("trickroom", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        if (battleInstance.getField().isTrickRoom()) {
            battleInstance.getField().setField(FieldType::None, 0);
        } else {
            battleInstance.getField().setField(FieldType::TrickRoom, 5);
        }
        return true;
    });

    moveRuleRegistry.registerRule("gravity", [&runtimeMoveState](Battle&, Pokemon*, Pokemon*, const Move&) {
        runtimeMoveState.gravityTurns = 5;
        return true;
    });

    moveRuleRegistry.registerRule("raindance", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getWeather().setWeather(WeatherType::Rain, 5);
        return true;
    });
    moveRuleRegistry.registerRule("sunnyday", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getWeather().setWeather(WeatherType::Sun, 5);
        return true;
    });
    moveRuleRegistry.registerRule("sandstorm", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getWeather().setWeather(WeatherType::Sandstorm, 5);
        return true;
    });
    moveRuleRegistry.registerRule("hail", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getWeather().setWeather(WeatherType::Hail, 5);
        return true;
    });
    moveRuleRegistry.registerRule("snowscape", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getWeather().setWeather(WeatherType::Hail, 5);
        return true;
    });

    moveRuleRegistry.registerRule("psychicterrain", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getField().setField(FieldType::Psychic, 5);
        return true;
    });
    moveRuleRegistry.registerRule("electricterrain", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getField().setField(FieldType::Electric, 5);
        return true;
    });
    moveRuleRegistry.registerRule("grassyterrain", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getField().setField(FieldType::Grassy, 5);
        return true;
    });
    moveRuleRegistry.registerRule("mistyterrain", [](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        battleInstance.getField().setField(FieldType::Misty, 5);
        return true;
    });

    moveRuleRegistry.registerRule("mist", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (actorSide) {
            actorSide->setMistTurns(5);
        }
        return true;
    });

    moveRuleRegistry.registerRule("safeguard", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (actorSide) {
            actorSide->setSafeguardTurns(5);
        }
        return true;
    });

    moveRuleRegistry.registerRule("mudsport", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (actorSide) {
            actorSide->setMudSportTurns(5);
        }
        return true;
    });

    moveRuleRegistry.registerRule("watersport", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (actorSide) {
            actorSide->setWaterSportTurns(5);
        }
        return true;
    });

    registerProtectLikeRule("protect");
    registerProtectLikeRule("detect");
    registerProtectLikeRule("spikyshield", "spikyshield");
    registerProtectLikeRule("kingsshield", "kingsshield");
    registerProtectLikeRule("obstruct", "obstruct");
    registerProtectLikeRule("banefulbunker", "banefulbunker");
    registerProtectLikeRule("burningbulwark", "burningbulwark");
    registerSideGuardLikeRule("quickguard", true);
    registerSideGuardLikeRule("wideguard", false);

    registerRecoverLikeRule("recover");
    registerRecoverLikeRule("softboiled");
    registerRecoverLikeRule("milkdrink");
    registerRecoverLikeRule("slackoff");

    moveRuleRegistry.registerRule("shoreup", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        int healAmount = std::max(1, attacker->getMaxHP() / 2);
        if (battleInstance.getWeather().type == WeatherType::Sandstorm) {
            healAmount = std::max(1, (attacker->getMaxHP() * 2) / 3);
        }
        attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
        return true;
    });

    moveRuleRegistry.registerRule("healpulse", [](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender || defender->isFainted()) {
            return true;
        }
        const int healAmount = std::max(1, defender->getMaxHP() / 2);
        defender->setCurrentHP(defender->getCurrentHP() + healAmount);
        return true;
    });

    moveRuleRegistry.registerRule("lifedew", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker || attacker->isFainted()) {
            return true;
        }
        const int healAmount = std::max(1, attacker->getMaxHP() / 4);
        attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
        return true;
    });

    moveRuleRegistry.registerRule("lunarblessing", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker || attacker->isFainted()) {
            return true;
        }
        const int healAmount = std::max(1, attacker->getMaxHP() / 4);
        attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
        attacker->clearStatuses();
        return true;
    });

    moveRuleRegistry.registerRule("morningsun", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        const int healAmount = weatherRecoveryAmount(battleInstance.getWeather(), attacker);
        attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
        return true;
    });
    moveRuleRegistry.registerRule("synthesis", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        const int healAmount = weatherRecoveryAmount(battleInstance.getWeather(), attacker);
        attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
        return true;
    });
    moveRuleRegistry.registerRule("moonlight", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        const int healAmount = weatherRecoveryAmount(battleInstance.getWeather(), attacker);
        attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
        return true;
    });

    moveRuleRegistry.registerRule("focusenergy", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        int& stage = runtimeMoveState.criticalHitStage[attacker];
        stage = std::min(4, stage + 2);
        return true;
    });

    moveRuleRegistry.registerRule("rest", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }

        if (resolveStatusImmunity(attacker->getAbility(), StatusType::Sleep)) {
            return true;
        }

        attacker->setCurrentHP(attacker->getMaxHP());
        attacker->removeStatus(StatusType::Paralysis);
        attacker->removeStatus(StatusType::Burn);
        attacker->removeStatus(StatusType::Poison);
        attacker->removeStatus(StatusType::ToxicPoison);
        attacker->removeStatus(StatusType::Freeze);
        attacker->removeStatus(StatusType::Sleep);
        attacker->addStatus(StatusType::Sleep, 2);
        return true;
    });

    moveRuleRegistry.registerRule("wish", [&battle, &runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        Side* actorSide = Battle::findSideForPokemon(battle, attacker);
        if (!actorSide) {
            return true;
        }
        runtimeMoveState.wishState[actorSide] = Battle::WishState{2, std::max(1, attacker->getMaxHP() / 2)};
        return true;
    });

    moveRuleRegistry.registerRule("healingwish", [&battle, &runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        Side* actorSide = Battle::findSideForPokemon(battle, attacker);
        if (!actorSide) {
            return true;
        }
        runtimeMoveState.switchInRecoveryState[actorSide] = Battle::SwitchInRecoveryState{false};
        attacker->setCurrentHP(0);
        return true;
    });

    moveRuleRegistry.registerRule("lunardance", [&battle, &runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        Side* actorSide = Battle::findSideForPokemon(battle, attacker);
        if (!actorSide) {
            return true;
        }
        runtimeMoveState.switchInRecoveryState[actorSide] = Battle::SwitchInRecoveryState{true};
        attacker->setCurrentHP(0);
        return true;
    });

    moveRuleRegistry.registerRule("metronome", [&battle](Battle& battleInstance, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender) {
            return true;
        }

        Move rolledMove;
        for (int tries = 0; tries < 32; ++tries) {
            const int randomMoveId = PRNG::nextInt(1, 938);
            Move candidate = createMoveById(randomMoveId);
            if (candidate.getName().empty()) {
                continue;
            }
            if (moveNameEquals(candidate, "metronome") || moveNameEquals(candidate, "mirrormove")) {
                continue;
            }
            rolledMove = candidate;
            break;
        }

        if (rolledMove.getName().empty()) {
            return true;
        }

        if (rolledMove.getCategory() != Category::Status) {
            const int damage = battleInstance.calculateDamage(attacker, defender, rolledMove);
            defender->setCurrentHP(defender->getCurrentHP() - damage);
        }

        battleInstance.processMoveEffects(attacker, defender, rolledMove);
        return true;
    });

    moveRuleRegistry.registerRule("mirrormove", [&battle](Battle& battleInstance, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender) {
            return true;
        }

        const auto copiedIt = battle.runtimeMoveState.lastUsedMoveName.find(defender);
        if (copiedIt == battle.runtimeMoveState.lastUsedMoveName.end() || copiedIt->second.empty()) {
            return true;
        }

        Move copiedMove = createMoveByName(copiedIt->second);
        if (copiedMove.getName().empty()) {
            return true;
        }
        if (moveNameEquals(copiedMove, "mirrormove") || moveNameEquals(copiedMove, "metronome")) {
            return true;
        }

        if (copiedMove.getCategory() != Category::Status) {
            const int damage = battleInstance.calculateDamage(attacker, defender, copiedMove);
            defender->setCurrentHP(defender->getCurrentHP() - damage);
        }

        battleInstance.processMoveEffects(attacker, defender, copiedMove);
        return true;
    });

    moveRuleRegistry.registerRule("trick", [](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || attacker->getSubstituteHP() > 0 || defender->getSubstituteHP() > 0) {
            return true;
        }

        const ItemType attackerItem = attacker->getItemType();
        const ItemType defenderItem = defender->getItemType();
        attacker->setItemType(defenderItem);
        defender->setItemType(attackerItem);
        return true;
    });

    moveRuleRegistry.registerRule("roleplay", [](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }

        const AbilityType copied = defender->getAbility();
        if (copied == AbilityType::None) {
            return true;
        }

        attacker->setAbility(copied);
        return true;
    });

    moveRuleRegistry.registerRule("skillswap", [](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }

        const AbilityType attackerAbility = attacker->getAbility();
        const AbilityType defenderAbility = defender->getAbility();
        attacker->setAbility(defenderAbility);
        defender->setAbility(attackerAbility);
        return true;
    });

    moveRuleRegistry.registerRule("foresight", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender || defender->getSubstituteHP() > 0) {
            return true;
        }
        runtimeMoveState.foresightMarked[defender] = true;
        return true;
    });

    moveRuleRegistry.registerRule("odorsleuth", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender || defender->getSubstituteHP() > 0) {
            return true;
        }
        runtimeMoveState.foresightMarked[defender] = true;
        return true;
    });

    moveRuleRegistry.registerRule("miracleeye", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender || defender->getSubstituteHP() > 0) {
            return true;
        }
        runtimeMoveState.miracleEyeMarked[defender] = true;
        return true;
    });

    moveRuleRegistry.registerRule("reflect", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (actorSide) {
            actorSide->setReflectTurns(5);
        }
        return true;
    });
    moveRuleRegistry.registerRule("lightscreen", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (actorSide) {
            actorSide->setLightScreenTurns(5);
        }
        return true;
    });

    moveRuleRegistry.registerRule("spikes", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (!actorSide) {
            return true;
        }
        Side& opponentSide = battleInstance.getOpponentSide(*actorSide);
        opponentSide.addSpikesLayer();
        return true;
    });
    moveRuleRegistry.registerRule("toxicspikes", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (!actorSide) {
            return true;
        }
        Side& opponentSide = battleInstance.getOpponentSide(*actorSide);
        opponentSide.addToxicSpikesLayer();
        return true;
    });
    moveRuleRegistry.registerRule("stealthrock", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (!actorSide) {
            return true;
        }
        Side& opponentSide = battleInstance.getOpponentSide(*actorSide);
        opponentSide.setStealthRock(true);
        return true;
    });

    moveRuleRegistry.registerRule("defog", [](Battle& battleInstance, Pokemon* attacker, Pokemon* defender, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        Side* targetSide = Battle::findSideForPokemon(battleInstance, defender);
        if (!actorSide || !targetSide) {
            return true;
        }

        actorSide->clearScreenEffects();
        actorSide->clearMist();
        actorSide->clearSafeguard();
        actorSide->clearMudSport();
        actorSide->clearWaterSport();
        actorSide->clearEntryHazards();

        targetSide->clearScreenEffects();
        targetSide->clearMist();
        targetSide->clearSafeguard();
        targetSide->clearMudSport();
        targetSide->clearWaterSport();
        targetSide->clearEntryHazards();

        if (defender) {
            defender->changeEvasionStage(-1);
        }
        return true;
    });

    moveRuleRegistry.registerRule("courtchange", [](Battle& battleInstance, Pokemon* attacker, Pokemon* defender, const Move&) {
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        Side* targetSide = Battle::findSideForPokemon(battleInstance, defender);
        if (!actorSide || !targetSide) {
            return true;
        }

        const int actorReflect = actorSide->getReflectTurns();
        const int actorLightScreen = actorSide->getLightScreenTurns();
        const int actorMist = actorSide->getMistTurns();
        const int actorSafeguard = actorSide->getSafeguardTurns();
        const int actorMudSport = actorSide->getMudSportTurns();
        const int actorWaterSport = actorSide->getWaterSportTurns();
        const int actorSpikes = actorSide->getSpikesLayers();
        const int actorToxicSpikes = actorSide->getToxicSpikesLayers();
        const bool actorStealthRock = actorSide->hasStealthRock();

        actorSide->setReflectTurns(targetSide->getReflectTurns());
        actorSide->setLightScreenTurns(targetSide->getLightScreenTurns());
        actorSide->setMistTurns(targetSide->getMistTurns());
        actorSide->setSafeguardTurns(targetSide->getSafeguardTurns());
        actorSide->setMudSportTurns(targetSide->getMudSportTurns());
        actorSide->setWaterSportTurns(targetSide->getWaterSportTurns());
        actorSide->clearEntryHazards();
        if (targetSide->getSpikesLayers() > 0) {
            for (int i = 0; i < targetSide->getSpikesLayers(); ++i) {
                actorSide->addSpikesLayer();
            }
        }
        if (targetSide->getToxicSpikesLayers() > 0) {
            for (int i = 0; i < targetSide->getToxicSpikesLayers(); ++i) {
                actorSide->addToxicSpikesLayer();
            }
        }
        actorSide->setStealthRock(targetSide->hasStealthRock());

        targetSide->setReflectTurns(actorReflect);
        targetSide->setLightScreenTurns(actorLightScreen);
        targetSide->setMistTurns(actorMist);
        targetSide->setSafeguardTurns(actorSafeguard);
        targetSide->setMudSportTurns(actorMudSport);
        targetSide->setWaterSportTurns(actorWaterSport);
        targetSide->clearEntryHazards();
        if (actorSpikes > 0) {
            for (int i = 0; i < actorSpikes; ++i) {
                targetSide->addSpikesLayer();
            }
        }
        if (actorToxicSpikes > 0) {
            for (int i = 0; i < actorToxicSpikes; ++i) {
                targetSide->addToxicSpikesLayer();
            }
        }
        targetSide->setStealthRock(actorStealthRock);
        return true;
    });

    moveRuleRegistry.registerRule("leechseed", [](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (defender->getSubstituteHP() > 0) {
            return true;
        }
        if (defender->getType1() == Type::Grass || defender->getType2() == Type::Grass) {
            return true;
        }
        defender->setLeechSeedSource(attacker);
        return true;
    });
    moveRuleRegistry.registerRule("substitute", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (attacker->getSubstituteHP() > 0) {
            return true;
        }

        const int substituteCost = std::max(1, attacker->getMaxHP() / 4);
        if (attacker->getCurrentHP() <= substituteCost) {
            return true;
        }

        attacker->setCurrentHP(attacker->getCurrentHP() - substituteCost);
        attacker->setSubstituteHP(substituteCost);
        return true;
    });

    moveRuleRegistry.registerRule("disable", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move&) {
        battleInstance.applyDisableToTarget(defender);
        return true;
    });

    moveRuleRegistry.registerRule("refresh", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        attacker->removeStatus(StatusType::Paralysis);
        attacker->removeStatus(StatusType::Burn);
        attacker->removeStatus(StatusType::Poison);
        attacker->removeStatus(StatusType::ToxicPoison);
        return true;
    });

    moveRuleRegistry.registerRule("imprison", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        runtimeMoveState.imprisonActive[attacker] = true;
        return true;
    });

    moveRuleRegistry.registerRule("camouflage", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }

        Type newType = Type::Normal;
        if (battleInstance.getField().isActive()) {
            switch (battleInstance.getField().type) {
                case FieldType::Electric:
                    newType = Type::Electric;
                    break;
                case FieldType::Grassy:
                    newType = Type::Grass;
                    break;
                case FieldType::Misty:
                    newType = Type::Fairy;
                    break;
                case FieldType::Psychic:
                    newType = Type::Psychic;
                    break;
                default:
                    newType = Type::Normal;
                    break;
            }
        }

        attacker->setTypes(newType, Type::Count);
        return true;
    });

    moveRuleRegistry.registerRule("yawn", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender || defender->getSubstituteHP() > 0 || hasMajorStatus(defender)) {
            return true;
        }
        runtimeMoveState.yawnState[defender] = Battle::TimedState{2};
        return true;
    });

    moveRuleRegistry.registerRule("taunt", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender) {
            return true;
        }
        runtimeMoveState.tauntState[defender] = Battle::TimedState{3};
        return true;
    });

    moveRuleRegistry.registerRule("torment", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender) {
            return true;
        }
        runtimeMoveState.tormentState[defender] = Battle::TimedState{3};
        return true;
    });

    moveRuleRegistry.registerRule("healblock", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender) {
            return true;
        }
        runtimeMoveState.healBlockState[defender] = Battle::TimedState{5};
        return true;
    });

    moveRuleRegistry.registerRule("embargo", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender) {
            return true;
        }
        runtimeMoveState.embargoState[defender] = Battle::TimedState{5};
        return true;
    });

    moveRuleRegistry.registerRule("endure", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        runtimeMoveState.endureActive[attacker] = true;
        return true;
    });

    moveRuleRegistry.registerRule("lockon", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender) {
            return true;
        }
        runtimeMoveState.lockOnState[attacker] = Battle::LockOnState{defender, 2};
        return true;
    });

    moveRuleRegistry.registerRule("mindreader", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender) {
            return true;
        }
        runtimeMoveState.lockOnState[attacker] = Battle::LockOnState{defender, 2};
        return true;
    });

    moveRuleRegistry.registerRule("spite", [&runtimeMoveState](Battle&, Pokemon*, Pokemon* defender, const Move&) {
        if (!defender) {
            return true;
        }
        const auto lastMoveIt = runtimeMoveState.lastUsedMoveName.find(defender);
        if (lastMoveIt == runtimeMoveState.lastUsedMoveName.end() || lastMoveIt->second.empty()) {
            return true;
        }
        defender->reduceMovePPByName(lastMoveIt->second, 4);
        return true;
    });

    moveRuleRegistry.registerRule("whirlwind", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move&) {
        if (!canForceSwitchTarget(battleInstance, defender)) {
            return true;
        }

        Side* targetSide = Battle::findSideForPokemon(battleInstance, defender);
        if (targetSide) {
            forceSwitchSide(battleInstance, *targetSide);
        }
        return true;
    });
    moveRuleRegistry.registerRule("roar", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move&) {
        if (!canForceSwitchTarget(battleInstance, defender)) {
            return true;
        }

        Side* targetSide = Battle::findSideForPokemon(battleInstance, defender);
        if (targetSide) {
            forceSwitchSide(battleInstance, *targetSide);
        }
        return true;
    });
    moveRuleRegistry.registerRule("dragontail", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move&) {
        if (!canForceSwitchTarget(battleInstance, defender)) {
            return true;
        }

        Side* targetSide = Battle::findSideForPokemon(battleInstance, defender);
        if (targetSide) {
            forceSwitchSide(battleInstance, *targetSide);
        }
        return true;
    });
    moveRuleRegistry.registerRule("circlethrow", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move&) {
        if (!canForceSwitchTarget(battleInstance, defender)) {
            return true;
        }

        Side* targetSide = Battle::findSideForPokemon(battleInstance, defender);
        if (targetSide) {
            forceSwitchSide(battleInstance, *targetSide);
        }
        return true;
    });

    moveRuleRegistry.registerRule("supersonic", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move& move) {
        return applyConfusionMove(battleInstance, defender, move);
    });

    moveRuleRegistry.registerRule("confuseray", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move& move) {
        return applyConfusionMove(battleInstance, defender, move);
    });
    moveRuleRegistry.registerRule("sweetkiss", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move& move) {
        return applyConfusionMove(battleInstance, defender, move);
    });
    moveRuleRegistry.registerRule("teeterdance", [](Battle& battleInstance, Pokemon*, Pokemon* defender, const Move& move) {
        return applyConfusionMove(battleInstance, defender, move);
    });

    moveRuleRegistry.registerRule("mimic", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }

        const auto lastMoveIt = runtimeMoveState.lastUsedMoveName.find(defender);
        if (lastMoveIt == runtimeMoveState.lastUsedMoveName.end() || lastMoveIt->second.empty()) {
            return true;
        }

        const std::string copiedMoveName = normalizeMoveName(lastMoveIt->second);
        if (copiedMoveName.empty() || copiedMoveName == "mimic" || copiedMoveName == "sketch" || copiedMoveName == "struggle") {
            return true;
        }

        Move copiedMove = createMoveByName(lastMoveIt->second);
        if (copiedMove.getName().empty()) {
            return true;
        }

        copiedMove.setPP(std::min(5, copiedMove.getMaxPP()));
        attacker->replaceMoveByName("Mimic", copiedMove);
        return true;
    });

    moveRuleRegistry.registerRule("transform", [](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }

        attacker->setTypes(defender->getType1(), defender->getType2());

        std::vector<Move> copiedMoves = defender->getMoves();
        for (Move& copiedMove : copiedMoves) {
            copiedMove.setPP(std::min(5, copiedMove.getMaxPP()));
        }
        attacker->replaceMoves(copiedMoves);
        return true;
    });

    moveRuleRegistry.registerRule("conversion", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        for (const Move& knownMove : attacker->getMoves()) {
            if (!knownMove.getName().empty() && knownMove.getType() != Type::Count) {
                attacker->setTypes(knownMove.getType(), Type::Count);
                break;
            }
        }
        return true;
    });

    moveRuleRegistry.registerRule("conversion2", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender) {
            return true;
        }

        const auto lastMoveIt = runtimeMoveState.lastUsedMoveName.find(defender);
        if (lastMoveIt == runtimeMoveState.lastUsedMoveName.end() || lastMoveIt->second.empty()) {
            return true;
        }

        const Move lastMove = createMoveByName(lastMoveIt->second);
        if (lastMove.getName().empty() || lastMove.getType() == Type::Count) {
            return true;
        }

        attacker->setTypes(findResistingTypeForAttack(lastMove.getType()), Type::Count);
        return true;
    });

    moveRuleRegistry.registerRule("sketch", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }

        const auto lastMoveIt = runtimeMoveState.lastUsedMoveName.find(defender);
        if (lastMoveIt == runtimeMoveState.lastUsedMoveName.end() || lastMoveIt->second.empty()) {
            return true;
        }

        const std::string copiedMoveName = normalizeMoveName(lastMoveIt->second);
        if (copiedMoveName.empty() || copiedMoveName == "sketch" || copiedMoveName == "struggle") {
            return true;
        }

        Move copiedMove = createMoveByName(lastMoveIt->second);
        if (copiedMove.getName().empty()) {
            return true;
        }

        attacker->replaceMoveByName("Sketch", copiedMove);
        return true;
    });

    moveRuleRegistry.registerRule("roost", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        const int healAmount = std::max(1, attacker->getMaxHP() / 2);
        attacker->setCurrentHP(attacker->getCurrentHP() + healAmount);
        return true;
    });

    moveRuleRegistry.registerRule("bellydrum", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        const int hpCost = std::max(1, attacker->getMaxHP() / 2);
        if (attacker->getCurrentHP() <= hpCost) {
            return true;
        }
        attacker->setCurrentHP(attacker->getCurrentHP() - hpCost);
        const int currentStage = attacker->getStatStage(StatIndex::Attack);
        attacker->changeStatStage(StatIndex::Attack, 6 - currentStage);
        return true;
    });

    moveRuleRegistry.registerRule("ingrain", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        runtimeMoveState.ingrainActive[attacker] = true;
        return true;
    });

    moveRuleRegistry.registerRule("perishsong", [&runtimeMoveState](Battle& battleInstance, Pokemon*, Pokemon*, const Move&) {
        Pokemon* activeA = battleInstance.getSideA().getActivePokemon();
        Pokemon* activeB = battleInstance.getSideB().getActivePokemon();
        if (activeA && !activeA->isFainted()) {
            runtimeMoveState.perishSongState[activeA] = Battle::TimedState{3};
        }
        if (activeB && !activeB->isFainted()) {
            runtimeMoveState.perishSongState[activeB] = Battle::TimedState{3};
        }
        return true;
    });

    moveRuleRegistry.registerRule("attract", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }
        runtimeMoveState.infatuationSource[defender] = attacker;
        return true;
    });

    moveRuleRegistry.registerRule("destinybond", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        runtimeMoveState.destinyBondActive[attacker] = true;
        return true;
    });

    moveRuleRegistry.registerRule("grudge", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        runtimeMoveState.grudgeActive[attacker] = true;
        return true;
    });

    moveRuleRegistry.registerRule("curse", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker) {
            return true;
        }

        const bool isGhostType = attacker->getType1() == Type::Ghost || attacker->getType2() == Type::Ghost;
        if (isGhostType) {
            if (!defender || defender->getSubstituteHP() > 0) {
                return true;
            }
            const int hpCost = std::max(1, attacker->getMaxHP() / 2);
            if (attacker->getCurrentHP() <= hpCost) {
                return true;
            }
            attacker->setCurrentHP(attacker->getCurrentHP() - hpCost);
            runtimeMoveState.ghostCurseActive[defender] = true;
            return true;
        }

        attacker->changeStatStage(StatIndex::Attack, 1);
        attacker->changeStatStage(StatIndex::Defense, 1);
        attacker->changeStatStage(StatIndex::Speed, -1);
        return true;
    });

    moveRuleRegistry.registerRule("sleeptalk", [&battle, &runtimeMoveState](Battle& battleInstance, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !attacker->hasStatus(StatusType::Sleep)) {
            return true;
        }

        std::vector<Move> callableMoves;
        for (const Move& knownMove : attacker->getMoves()) {
            if (knownMove.getName().empty()) {
                continue;
            }
            if (moveNameEquals(knownMove, "sleeptalk")) {
                continue;
            }
            callableMoves.push_back(knownMove);
        }

        if (callableMoves.empty()) {
            return true;
        }

        const int index = PRNG::nextInt(0, static_cast<int>(callableMoves.size()));
        Move calledMove = callableMoves[std::min(static_cast<int>(callableMoves.size()) - 1, index)];

        if (calledMove.getCategory() != Category::Status && defender && !defender->isFainted()) {
            const int damage = battleInstance.calculateDamage(attacker, defender, calledMove);
            defender->setCurrentHP(defender->getCurrentHP() - damage);
        }

        if (defender && !defender->isFainted()) {
            battleInstance.processMoveEffects(attacker, defender, calledMove);
        }
        return true;
    });

    moveRuleRegistry.registerRule("spiderweb", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }
        runtimeMoveState.trappedBySource[defender] = attacker;
        return true;
    });

    moveRuleRegistry.registerRule("meanlook", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }
        runtimeMoveState.trappedBySource[defender] = attacker;
        return true;
    });

    moveRuleRegistry.registerRule("block", [&runtimeMoveState](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender || defender->getSubstituteHP() > 0) {
            return true;
        }
        runtimeMoveState.trappedBySource[defender] = attacker;
        return true;
    });

    moveRuleRegistry.registerRule("doubleteam", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (attacker) {
            attacker->changeEvasionStage(1);
        }
        return true;
    });
    moveRuleRegistry.registerRule("minimize", [](Battle&, Pokemon* attacker, Pokemon*, const Move&) {
        if (attacker) {
            attacker->changeEvasionStage(2);
        }
        return true;
    });
    moveRuleRegistry.registerRule("splash", [](Battle&, Pokemon*, Pokemon*, const Move&) {
        return true;
    });

    moveRuleRegistry.registerRule("teleport", [](Battle& battleInstance, Pokemon* attacker, Pokemon*, const Move&) {
        if (!attacker) {
            return true;
        }
        Side* actorSide = Battle::findSideForPokemon(battleInstance, attacker);
        if (actorSide) {
            actorSide->autoSwitchNext();
        }
        return true;
    });

    moveRuleRegistry.registerRule("painsplit", [](Battle&, Pokemon* attacker, Pokemon* defender, const Move&) {
        if (!attacker || !defender) {
            return true;
        }
        const int sharedHp = (attacker->getCurrentHP() + defender->getCurrentHP()) / 2;
        attacker->setCurrentHP(sharedHp);
        defender->setCurrentHP(sharedHp);
        return true;
    });

    registerEvasionDropRule("sandattack");
    registerEvasionDropRule("smokescreen");
    registerEvasionDropRule("flash");

    registerAccuracyDropRule("kinesis", 1);

    moveRuleRegistry.registerRule("closecombat", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("superpower", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("overheat", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("leafstorm", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("dracometeor", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("steelbeam", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("vcreate", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("fleurcannon", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
    moveRuleRegistry.registerRule("clangoroussoulblaze", [](Battle&, Pokemon* attacker, Pokemon*, const Move& move) { applySelfStatDropMove(attacker, move); return true; });
}
