#pragma once

#include "Types.h"
#include "Moves.h"
#include "Abilities.h"
#include <string>
#include <vector>
#include <array>

enum class EggGroup {
    None,
    Monster,
    Water1,
    Water2,
    Bug,
    Flying,
    Field,
    Fairy,
    Grass,
    HumanLike,
    Mineral,
    Amorphous,
    Ditto,
    Dragon,
    Undiscovered,
    Count
};

struct Species {
    int id;
    std::string name;
    int height;
    int weight;
    Type type1;
    Type type2; // Use Type::Count for single type
    std::array<int, 6> baseStats; // HP, Atk, Def, SpAtk, SpDef, Spe
    std::vector<EggGroup> eggGroups;
    std::vector<Move> learnableMoves;
    float maleRatio; // 0.0 - 1.0
    int nextEvolutionID; // -1 if none
    int evolutionLevel; // 0 if no evolution by level
    std::vector<AbilityType> abilities;
    AbilityType hiddenAbility;

    bool hasEvolution() const { return nextEvolutionID >= 0; }
    float getFemaleRatio() const { return maleRatio < 0.0f ? 0.0f : 1.0f - maleRatio; }
    bool isGenderless() const { return maleRatio < 0.0f; }
};
