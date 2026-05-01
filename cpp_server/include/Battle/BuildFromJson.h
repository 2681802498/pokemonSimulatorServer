#pragma once

#include "Pokemon.h"
#include "Species.h"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class BuildFromJson {
public:
    // 从Json创建Species对象
    static Species buildSpecies(const json& jsonData);
    
    // 从Json创建Pokemon对象
    static Pokemon buildPokemon(const json& jsonData, const Species& species);
    
    // 从Json文件加载宝可梦数据
    static Pokemon loadPokemonFromFile(const std::string& filePath);
    
    // 从Json字符串加载宝可梦数据
    static Pokemon loadPokemonFromString(const std::string& jsonString);
};
