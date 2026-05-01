#pragma once

#include "Pokemon.h"
#include "Effect.h"
#include <array>
#include <string>
#include <vector>

class Side {
public:
    Side() = default;
    explicit Side(std::string name);

    bool addPokemon(Pokemon* pokemon);
    Pokemon* getActivePokemon() const;
    int getActiveIndex() const { return activeIndex; }
    bool canSwitch() const;
    bool switchActive(int newIndex);
    bool autoSwitchNext();
    bool hasRemainingPokemon() const;
    bool isEmpty() const;

    const std::string& getName() const { return name; }
    const std::vector<FieldEffect>& getFieldEffects() const { return fieldEffects; }

    void addFieldEffect(const FieldEffect& effect);
    void clearFieldEffects();

    const std::array<Pokemon*, 6>& getTeam() const { return team; }
    int getPokemonCount() const { return count; }
    
    // 连续守护相关方法
    int getProtectCount() const { return protectCount; }
    void setProtectCount(int count) { protectCount = count; }
    void resetProtectCount() { protectCount = 0; }

    int getReflectTurns() const { return reflectTurns; }
    int getLightScreenTurns() const { return lightScreenTurns; }
    bool hasReflect() const { return reflectTurns > 0; }
    bool hasLightScreen() const { return lightScreenTurns > 0; }
    void setReflectTurns(int turns) { reflectTurns = turns > 0 ? turns : 0; }
    void setLightScreenTurns(int turns) { lightScreenTurns = turns > 0 ? turns : 0; }
    void clearScreenEffects() { reflectTurns = 0; lightScreenTurns = 0; }
    void tickScreenEffects();

    int getMistTurns() const { return mistTurns; }
    bool hasMist() const { return mistTurns > 0; }
    void setMistTurns(int turns) { mistTurns = turns > 0 ? turns : 0; }
    void clearMist() { mistTurns = 0; }
    void tickMistTurns();

    int getSafeguardTurns() const { return safeguardTurns; }
    bool hasSafeguard() const { return safeguardTurns > 0; }
    void setSafeguardTurns(int turns) { safeguardTurns = turns > 0 ? turns : 0; }
    void clearSafeguard() { safeguardTurns = 0; }
    void tickSafeguardTurns();

    int getMudSportTurns() const { return mudSportTurns; }
    bool hasMudSport() const { return mudSportTurns > 0; }
    void setMudSportTurns(int turns) { mudSportTurns = turns > 0 ? turns : 0; }
    void clearMudSport() { mudSportTurns = 0; }
    void tickMudSportTurns();

    int getWaterSportTurns() const { return waterSportTurns; }
    bool hasWaterSport() const { return waterSportTurns > 0; }
    void setWaterSportTurns(int turns) { waterSportTurns = turns > 0 ? turns : 0; }
    void clearWaterSport() { waterSportTurns = 0; }
    void tickWaterSportTurns();

    int getSpikesLayers() const { return spikesLayers; }
    int getToxicSpikesLayers() const { return toxicSpikesLayers; }
    bool hasStealthRock() const { return stealthRock; }
    void addSpikesLayer();
    void addToxicSpikesLayer();
    void setStealthRock(bool enabled) { stealthRock = enabled; }
    void clearEntryHazards();

private:
    std::string name;
    std::array<Pokemon*, 6> team{};
    int count = 0;
    int activeIndex = 0;
    std::vector<FieldEffect> fieldEffects;
    int protectCount = 0; // 连续守护的次数
    int reflectTurns = 0;
    int lightScreenTurns = 0;
    int mistTurns = 0;
    int safeguardTurns = 0;
    int mudSportTurns = 0;
    int waterSportTurns = 0;
    int spikesLayers = 0;
    int toxicSpikesLayers = 0;
    bool stealthRock = false;
};
