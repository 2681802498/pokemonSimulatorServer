#pragma once

#include "Battle.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class BattleSession {
public:
    static std::optional<BattleSession> createFromPokemonFiles(const std::string& sideAPath,
                                                               const std::string& sideBPath,
                                                               uint32_t seed = 0,
                                                               std::string* error = nullptr);

    static std::optional<BattleSession> createFromJson(const nlohmann::json& initRequest,
                                                       std::string* error = nullptr);

    nlohmann::json processTurn(const nlohmann::json& turnRequest);
    Battle* getBattle() { return battle.get(); }
    const Battle* getBattle() const { return battle.get(); }

    static std::string normalizeKey(const std::string& value);

private:
    std::vector<std::unique_ptr<Pokemon>> storage;
    std::unique_ptr<Battle> battle;

    static ItemType parseItemType(const std::string& value);
    static Pokemon* selectActor(Side& side, const nlohmann::json& actionJson);
    static Pokemon* selectTarget(Battle& battle, Side& actorSide, const nlohmann::json& actionJson);
};