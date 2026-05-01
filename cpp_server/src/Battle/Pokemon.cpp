#include "Battle/Pokemon.h"
#include "Battle/Utils.h"
#include "Battle/PRNG.h"

#include <algorithm>
#include <cctype>

namespace {
std::string normalizeMoveToken(const std::string& moveName) {
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
}

Pokemon::Pokemon(const Species& species, Nature nat, AbilityType abil, bool isHiddenAbil,
                 int lvl,
                 const std::array<int, static_cast<int>(StatIndex::Count)>& ivs,
                                 const std::array<int, static_cast<int>(StatIndex::Count)>& evs,
                                 ItemType heldItem)
    : species(species), name(species.name),
      type1(species.type1),
      type2(species.type2),
            nature(nat), ability(abil), isHiddenAbility(isHiddenAbil), itemType(heldItem), level(lvl), statuses(),
        ivs(ivs), evs(evs), statStages{0, 0, 0, 0, 0, 0, 0}, isProtected(false), substituteHP(0), leechSeedSource(nullptr), accuracyStage(0), evasionStage(0) {
    recalculateStats();
    currentHP = maxHP;
}

int Pokemon::calculateStat(int base, int iv, int ev, int level, float natureModifier, bool isHP) const {
    if (isHP) {
        if (base <= 1) {
            return 1;
        }
        int value = ((2 * base + iv + ev / 4) * level) / 100 + level + 10;
        return value;
    }
    int value = ((2 * base + iv + ev / 4) * level) / 100 + 5;
    return static_cast<int>(value * natureModifier);
}

void Pokemon::recalculateStats() {
    maxHP = calculateStat(species.baseStats[0], ivs[static_cast<int>(StatIndex::HP)], evs[static_cast<int>(StatIndex::HP)], level, 1.0f, true);
    attack = calculateStat(species.baseStats[1], ivs[static_cast<int>(StatIndex::Attack)], evs[static_cast<int>(StatIndex::Attack)], level, getNatureModifier(static_cast<int>(StatIndex::Attack) - 1), false);
    defense = calculateStat(species.baseStats[2], ivs[static_cast<int>(StatIndex::Defense)], evs[static_cast<int>(StatIndex::Defense)], level, getNatureModifier(static_cast<int>(StatIndex::Defense) - 1), false);
    specialAttack = calculateStat(species.baseStats[3], ivs[static_cast<int>(StatIndex::SpecialAttack)], evs[static_cast<int>(StatIndex::SpecialAttack)], level, getNatureModifier(static_cast<int>(StatIndex::SpecialAttack) - 1), false);
    specialDefense = calculateStat(species.baseStats[4], ivs[static_cast<int>(StatIndex::SpecialDefense)], evs[static_cast<int>(StatIndex::SpecialDefense)], level, getNatureModifier(static_cast<int>(StatIndex::SpecialDefense) - 1), false);
    speed = calculateStat(species.baseStats[5], ivs[static_cast<int>(StatIndex::Speed)], evs[static_cast<int>(StatIndex::Speed)], level, getNatureModifier(static_cast<int>(StatIndex::Speed) - 1), false);
}

float Pokemon::getTypeEffectiveness(Type attackType) const {
    float effectiveness = Utils::TypeEffectiveness[static_cast<int>(attackType)][static_cast<int>(type1)];
    if (type2 != Type::Count) {
        effectiveness *= Utils::TypeEffectiveness[static_cast<int>(attackType)][static_cast<int>(type2)];
    }
    return effectiveness;
}

void Pokemon::addStatus(StatusType s, int duration) {
    if (ability == AbilityType::Immunity && (s == StatusType::Poison || s == StatusType::ToxicPoison)) {
        return;
    }
    removeStatus(s);
    if (duration == -1) {
        if (s == StatusType::Sleep) {
            duration = PRNG::nextInt(1, 4);
        } else {
            duration = -1; // Permanent
        }
    }
    statuses.emplace_back(s, duration);
}

void Pokemon::removeStatus(StatusType s) {
    statuses.erase(std::remove_if(statuses.begin(), statuses.end(),
                                  [s](const std::pair<StatusType, int>& status) {
                                      return status.first == s;
                                  }), statuses.end());
}

bool Pokemon::tickStatusDuration(StatusType s) {
    auto it = std::find_if(statuses.begin(), statuses.end(), [s](const std::pair<StatusType, int>& status) {
        return status.first == s;
    });
    if (it == statuses.end()) {
        return false;
    }

    if (it->second < 0) {
        return false;
    }

    if (it->second > 1) {
        --(it->second);
        return false;
    }

    statuses.erase(it);
    return true;
}

void Pokemon::clearStatuses() {
    statuses.clear();
}

bool Pokemon::hasStatus(StatusType s) const {
    return std::any_of(statuses.begin(), statuses.end(),
                       [s](const std::pair<StatusType, int>& status) {
                           return status.first == s;
                       });
}

void Pokemon::changeStatStage(StatIndex index, int delta) {
    int stageIndex = static_cast<int>(index);
    if (stageIndex <= static_cast<int>(StatIndex::HP) || stageIndex >= static_cast<int>(StatIndex::Count)) {
        return;
    }

    int internalIndex = stageIndex - 1;
    statStages[internalIndex] += delta;
    if (statStages[internalIndex] > 6) {
        statStages[internalIndex] = 6;
    }
    if (statStages[internalIndex] < -6) {
        statStages[internalIndex] = -6;
    }
}

void Pokemon::changeAccuracyStage(int delta) {
    accuracyStage += delta;
    if (accuracyStage > 6) {
        accuracyStage = 6;
    }
    if (accuracyStage < -6) {
        accuracyStage = -6;
    }
}

void Pokemon::changeEvasionStage(int delta) {
    evasionStage += delta;
    if (evasionStage > 6) {
        evasionStage = 6;
    }
    if (evasionStage < -6) {
        evasionStage = -6;
    }
}

int Pokemon::getStatStage(StatIndex index) const {
    int stageIndex = static_cast<int>(index);
    if (stageIndex <= static_cast<int>(StatIndex::HP) || stageIndex >= static_cast<int>(StatIndex::Count)) {
        return 0;
    }
    return statStages[stageIndex - 1];
}

bool Pokemon::reduceMovePPByName(const std::string& moveName, int amount) {
    if (amount <= 0) {
        return false;
    }

    const std::string targetName = normalizeMoveToken(moveName);
    for (Move& move : moves) {
        if (normalizeMoveToken(move.getName()) != targetName) {
            continue;
        }
        move.setPP(move.getPP() - amount);
        return true;
    }
    return false;
}

bool Pokemon::replaceMoveByName(const std::string& existingMoveName, const Move& replacement) {
    const std::string targetName = normalizeMoveToken(existingMoveName);
    for (Move& move : moves) {
        if (normalizeMoveToken(move.getName()) != targetName) {
            continue;
        }
        move = replacement;
        return true;
    }
    return false;
}

void Pokemon::replaceMoves(const std::vector<Move>& newMoves) {
    moves.clear();
    for (const Move& move : newMoves) {
        if (moves.size() >= 4) {
            break;
        }
        moves.push_back(move);
    }
}

// 拷贝构造函数实现
Pokemon::Pokemon(const Pokemon& other) :
    species(other.species),
    name(other.name),
    type1(other.type1),
    type2(other.type2),
    nature(other.nature),
    ability(other.ability),
    isHiddenAbility(other.isHiddenAbility),
    itemType(other.itemType),
    level(other.level),
    maxHP(other.maxHP),
    currentHP(other.currentHP),
    attack(other.attack),
    defense(other.defense),
    specialAttack(other.specialAttack),
    specialDefense(other.specialDefense),
    speed(other.speed),
    statuses(other.statuses),
    moves(other.moves),
    ivs(other.ivs),
    evs(other.evs),
    statStages(other.statStages),
    isProtected(other.isProtected),
    substituteHP(other.substituteHP),
    leechSeedSource(other.leechSeedSource),
    accuracyStage(other.accuracyStage),
    evasionStage(other.evasionStage) {
}
