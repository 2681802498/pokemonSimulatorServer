#pragma once

#include "BattleData.h"  // IWYU pragma: keep
#include "Natures.h"
#include <string>
#include <vector>
#include <array>

enum class StatIndex {
    HP,
    Attack,
    Defense,
    SpecialAttack,
    SpecialDefense,
    Speed,
    Count
};

class Pokemon {
private:
    Species species;
    std::string name;
    Type type1;
    Type type2; // Use Type::Count for no second type
    Nature nature;
    AbilityType ability;
    bool isHiddenAbility;
    ItemType itemType;
    int level;
    int maxHP;
    int currentHP;
    int attack;
    int defense;
    int specialAttack;
    int specialDefense;
    int speed;
    std::vector<std::pair<StatusType, int>> statuses;
    std::vector<Move> moves;
    std::array<int, static_cast<int>(StatIndex::Count)> ivs;
    std::array<int, static_cast<int>(StatIndex::Count)> evs;
    std::array<int, 7> statStages;
    bool isProtected; // 保护状态
    int substituteHP;
    Pokemon* leechSeedSource;
    int accuracyStage;
    int evasionStage;

    int calculateStat(int base, int iv, int ev, int level, float natureModifier, bool isHP) const;
    void recalculateStats();

public:
    Pokemon(const Species& species, Nature nat, AbilityType abil, bool isHiddenAbil,
            int lvl,
            const std::array<int, static_cast<int>(StatIndex::Count)>& ivs,
            const std::array<int, static_cast<int>(StatIndex::Count)>& evs,
            ItemType heldItem = ItemType::None);
    
    // 拷贝构造函数
    Pokemon(const Pokemon& other);
    
    // 保护状态相关方法
    bool getIsProtected() const { return isProtected; }
    void setIsProtected(bool protectedStatus) { isProtected = protectedStatus; }

    // Getters
    const Species& getSpecies() const { return species; }
    std::string getName() const { return name; }
    Type getType1() const { return type1; }
    Type getType2() const { return type2; }
    Nature getNature() const { return nature; }
    AbilityType getAbility() const { return ability; }
    int getLevel() const { return level; }
    int getMaxHP() const { return maxHP; }
    int getCurrentHP() const { return currentHP; }
    int getAttack() const { return attack; }
    int getDefense() const { return defense; }
    int getSpecialAttack() const { return specialAttack; }
    int getSpecialDefense() const { return specialDefense; }
    int getSpeed() const { return speed; }
    const std::vector<std::pair<StatusType, int>>& getStatuses() const { return statuses; }
    void addStatus(StatusType s, int duration = -1);
    void removeStatus(StatusType s);
    bool tickStatusDuration(StatusType s);
    void clearStatuses();
    bool hasStatus(StatusType s) const;
    const std::vector<Move>& getMoves() const { return moves; }
    int getIV(StatIndex index) const { return ivs[static_cast<int>(index)]; }
    int getEV(StatIndex index) const { return evs[static_cast<int>(index)]; }
    int getStatStage(StatIndex index) const;
    ItemType getItemType() const { return itemType; }
    Item getHeldItem() const { return getItem(itemType); }
    int getSubstituteHP() const { return substituteHP; }
    Pokemon* getLeechSeedSource() const { return leechSeedSource; }
    int getAccuracyStage() const { return accuracyStage; }
    int getEvasionStage() const { return evasionStage; }

    // Setters
    void setCurrentHP(int hp) { currentHP = hp; if (currentHP < 0) currentHP = 0; if (currentHP > maxHP) currentHP = maxHP; }
    void setStatus();
    void setAbility(AbilityType newAbility) { ability = newAbility; }
    void setItemType(ItemType item) { itemType = item; }
    void setTypes(Type primary, Type secondary) { type1 = primary; type2 = secondary; }
    void resetTypesToSpecies() { type1 = species.type1; type2 = species.type2; }
    void removeItem() { itemType = ItemType::None; }
    void setSubstituteHP(int hp) { substituteHP = hp < 0 ? 0 : hp; }
    void clearSubstitute() { substituteHP = 0; }
    void setLeechSeedSource(Pokemon* source) { leechSeedSource = source; }
    void clearLeechSeedSource() { leechSeedSource = nullptr; }
    void changeAccuracyStage(int delta);
    void changeEvasionStage(int delta);
    
    // 携带物品函数
    void holdItem(ItemType item) { setItemType(item); }
    void changeStatStage(StatIndex index, int delta);
    void setIV(StatIndex index, int value) { ivs[static_cast<int>(index)] = value; recalculateStats(); }
    void setEV(StatIndex index, int value) { evs[static_cast<int>(index)] = value; recalculateStats(); }
    void addMove(const Move& move) { if (moves.size() < 4) moves.push_back(move); }    
    bool reduceMovePPByName(const std::string& moveName, int amount);
    bool replaceMoveByName(const std::string& existingMoveName, const Move& replacement);
    void replaceMoves(const std::vector<Move>& newMoves);
    // Utility
    bool isFainted() const { return currentHP <= 0; }
    float getTypeEffectiveness(Type attackType) const;
    float getNatureModifier(int statIndex) const { return ::getNatureModifier(nature, statIndex); }
};