#pragma once

#include "Battle.h"
#include "BattleActions.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class BattleToJson {
public:
    // 将BattleAction转换为Json
    static json actionToJson(const BattleAction& action);
    
    // 将Pokemon转换为Json
    static json pokemonToJson(const Pokemon* pokemon);
    
    // 将Side转换为Json
    static json sideToJson(const Side& side);
    
    // 将Battle转换为Json
    static json battleToJson(Battle& battle);
    
    // 将Type转换为字符串
    static std::string typeToString(Type type);
    
    // 将ActionType转换为字符串
    static std::string actionTypeToString(ActionType type);
    
    // 将Nature转换为字符串
    static std::string natureToString(Nature nature);
    
    // 将AbilityType转换为字符串
    static std::string abilityTypeToString(AbilityType ability);
    
    // 将ItemType转换为字符串
    static std::string itemTypeToString(ItemType item);
    
    // 将StatusType转换为字符串
    static std::string statusTypeToString(StatusType status);
    
    // 将WeatherType转换为字符串
    static std::string weatherTypeToString(WeatherType weather);
    
    // 将FieldType转换为字符串
    static std::string fieldTypeToString(FieldType field);
    
    // 将Json数据写入到cache文件中
    static void writeToCache(const json& data, const std::string& filename);
};