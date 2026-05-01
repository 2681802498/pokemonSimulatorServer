#include "Battle/BuildFromJson.h"
#include "Battle/Types.h"
#include "Battle/Natures.h"
#include "Battle/Status.h"
#include <nlohmann/json.hpp>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>

using json = nlohmann::json;

// 函数声明
std::map<int, Species> loadSpeciesFromFile();
std::map<int, Ability> loadAbilitiesFromFile();
std::map<int, Item> loadItemsFromFile();
std::map<int, Move> loadMovesFromFile();

// 辅助函数：从字符串转换为Type枚举
Type stringToType(const std::string& typeStr) {
    if (typeStr == "normal") return Type::Normal;
    if (typeStr == "fire") return Type::Fire;
    if (typeStr == "water") return Type::Water;
    if (typeStr == "electric") return Type::Electric;
    if (typeStr == "grass") return Type::Grass;
    if (typeStr == "ice") return Type::Ice;
    if (typeStr == "fighting") return Type::Fighting;
    if (typeStr == "poison") return Type::Poison;
    if (typeStr == "ground") return Type::Ground;
    if (typeStr == "flying") return Type::Flying;
    if (typeStr == "psychic") return Type::Psychic;
    if (typeStr == "bug") return Type::Bug;
    if (typeStr == "rock") return Type::Rock;
    if (typeStr == "ghost") return Type::Ghost;
    if (typeStr == "dragon") return Type::Dragon;
    if (typeStr == "dark") return Type::Dark;
    if (typeStr == "steel") return Type::Steel;
    if (typeStr == "fairy") return Type::Fairy;
    return Type::Count;
}

// 辅助函数：从字符串转换为Nature枚举
Nature stringToNature(const std::string& natureStr) {
    if (natureStr == "hardy") return Nature::Hardy;
    if (natureStr == "lonely") return Nature::Lonely;
    if (natureStr == "brave") return Nature::Brave;
    if (natureStr == "adamant") return Nature::Adamant;
    if (natureStr == "naughty") return Nature::Naughty;
    if (natureStr == "bold") return Nature::Bold;
    if (natureStr == "docile") return Nature::Docile;
    if (natureStr == "relaxed") return Nature::Relaxed;
    if (natureStr == "impish") return Nature::Impish;
    if (natureStr == "lax") return Nature::Lax;
    if (natureStr == "timid") return Nature::Timid;
    if (natureStr == "hasty") return Nature::Hasty;
    if (natureStr == "serious") return Nature::Serious;
    if (natureStr == "jolly") return Nature::Jolly;
    if (natureStr == "naive") return Nature::Naive;
    if (natureStr == "modest") return Nature::Modest;
    if (natureStr == "mild") return Nature::Mild;
    if (natureStr == "quiet") return Nature::Quiet;
    if (natureStr == "bashful") return Nature::Bashful;
    if (natureStr == "rash") return Nature::Rash;
    if (natureStr == "calm") return Nature::Calm;
    if (natureStr == "gentle") return Nature::Gentle;
    if (natureStr == "sassy") return Nature::Sassy;
    if (natureStr == "careful") return Nature::Careful;
    if (natureStr == "quirky") return Nature::Quirky;
    return Nature::Hardy;
}

// 辅助函数：从字符串转换为AbilityType枚举
AbilityType stringToAbility(const std::string& abilityStr) {
    return getAbilityTypeByNameFromData(abilityStr);
}

AbilityType parseAbilityValue(const json& abilityValue) {
    if (abilityValue.is_string()) {
        return stringToAbility(abilityValue.get<std::string>());
    }
    if (abilityValue.is_number_integer()) {
        return getAbilityTypeById(abilityValue.get<int>());
    }
    if (abilityValue.is_object()) {
        if (abilityValue.contains("id") && abilityValue["id"].is_number_integer()) {
            const AbilityType byId = getAbilityTypeById(abilityValue["id"].get<int>());
            if (byId != AbilityType::None) {
                return byId;
            }
        }
        if (abilityValue.contains("name") && abilityValue["name"].is_string()) {
            return stringToAbility(abilityValue["name"].get<std::string>());
        }
        if (abilityValue.contains("apiName") && abilityValue["apiName"].is_string()) {
            return stringToAbility(abilityValue["apiName"].get<std::string>());
        }
    }
    return AbilityType::None;
}

// 辅助函数：从字符串转换为ItemType枚举
ItemType stringToItem(const std::string& itemStr) {
    return getItemTypeByName(itemStr);
}

ItemType parseItemValue(const json& itemValue) {
    if (itemValue.is_string()) {
        return stringToItem(itemValue.get<std::string>());
    }
    if (itemValue.is_number_integer()) {
        return getItemTypeById(itemValue.get<int>());
    }
    if (itemValue.is_object()) {
        if (itemValue.contains("id") && itemValue["id"].is_number_integer()) {
            const ItemType byId = getItemTypeById(itemValue["id"].get<int>());
            if (byId != ItemType::None) {
                return byId;
            }
        }
        if (itemValue.contains("name") && itemValue["name"].is_string()) {
            return stringToItem(itemValue["name"].get<std::string>());
        }
        if (itemValue.contains("apiName") && itemValue["apiName"].is_string()) {
            return stringToItem(itemValue["apiName"].get<std::string>());
        }
    }
    return ItemType::None;
}

Move parseMoveValue(const json& moveValue) {
    if (moveValue.is_number_integer()) {
        return createMoveById(moveValue.get<int>());
    }
    if (moveValue.is_string()) {
        return createMoveByName(moveValue.get<std::string>());
    }
    if (moveValue.is_object()) {
        if (moveValue.contains("id") && moveValue["id"].is_number_integer()) {
            return createMoveById(moveValue["id"].get<int>());
        }
        if (moveValue.contains("name") && moveValue["name"].is_string()) {
            return createMoveByName(moveValue["name"].get<std::string>());
        }
        if (moveValue.contains("apiName") && moveValue["apiName"].is_string()) {
            return createMoveByName(moveValue["apiName"].get<std::string>());
        }
    }
    return createMoveByName("Tackle");
}

// 辅助函数：从字符串转换为EggGroup枚举
EggGroup stringToEggGroup(const std::string& eggGroupStr) {
    std::string key;
    key.reserve(eggGroupStr.size());
    for (char ch : eggGroupStr) {
        if (ch == '-' || ch == '_' || ch == ' ') {
            continue;
        }
        key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (key == "monster") return EggGroup::Monster;
    if (key == "water1") return EggGroup::Water1;
    if (key == "water2") return EggGroup::Water2;
    if (key == "bug") return EggGroup::Bug;
    if (key == "flying") return EggGroup::Flying;
    if (key == "field") return EggGroup::Field;
    if (key == "fairy") return EggGroup::Fairy;
    if (key == "grass") return EggGroup::Grass;
    if (key == "humanlike") return EggGroup::HumanLike;
    if (key == "mineral") return EggGroup::Mineral;
    if (key == "amorphous") return EggGroup::Amorphous;
    if (key == "ditto") return EggGroup::Ditto;
    if (key == "dragon") return EggGroup::Dragon;
    if (key == "undiscovered") return EggGroup::Undiscovered;
    return EggGroup::Monster; // 默认值
}

// 从Json创建Species对象
Species BuildFromJson::buildSpecies(const json& jsonData) {
    Species species;
    
    // 基本信息
    species.id = jsonData.value("id", 0);
    species.name = jsonData.value("name", "");
    species.height = jsonData.value("height", 0);
    species.weight = jsonData.value("weight", 0);
    species.type1 = stringToType(jsonData.value("type1", "normal"));
    species.type2 = stringToType(jsonData.value("type2", "count"));
    
    // 基础属性
    if (jsonData.contains("baseStats")) {
        const auto& stats = jsonData["baseStats"];
        for (int i = 0; i < 6 && i < stats.size(); i++) {
            species.baseStats[i] = stats[i].get<int>();
        }
    }
    
    // 蛋群
    if (jsonData.contains("eggGroups")) {
        for (const auto& eggGroup : jsonData["eggGroups"]) {
            species.eggGroups.push_back(stringToEggGroup(eggGroup.get<std::string>()));
        }
    }
    
    // 特性
    if (jsonData.contains("abilities")) {
        for (const auto& abilityId : jsonData["abilities"]) {
            int id = abilityId.get<int>();
            AbilityType abilityType = getAbilityTypeById(id);
            species.abilities.push_back(abilityType);
        }
        
        // 优先读取显式 hiddenAbilityID；没有时兼容旧格式（abilities 的第2个元素）。
        if (jsonData.contains("hiddenAbilityID")) {
            species.hiddenAbility = getAbilityTypeById(jsonData["hiddenAbilityID"].get<int>());
        } else if (jsonData["abilities"].size() > 1) {
            int hiddenAbilityId = jsonData["abilities"][1].get<int>();
            species.hiddenAbility = getAbilityTypeById(hiddenAbilityId);
        }
    }
    
    // 可学习技能
    if (jsonData.contains("learnableMoves")) {
        for (const auto& moveId : jsonData["learnableMoves"]) {
            int id = moveId.get<int>();
            species.learnableMoves.push_back(createMoveById(id));
        }
    }
    
    // 性别比例
    species.maleRatio = jsonData.value("maleRatio", 0.5f);
    
    // 进化信息
    species.nextEvolutionID = jsonData.value("nextEvolutionID", -1);
    species.evolutionLevel = jsonData.value("evolutionLevel", 0);
    
    return species;
}

// 从Json创建Pokemon对象
Pokemon BuildFromJson::buildPokemon(const json& jsonData, const Species& species) {
    // 初始化IVs和EVs
    std::array<int, static_cast<int>(StatIndex::Count)> ivs;
    std::array<int, static_cast<int>(StatIndex::Count)> evs;
    
    // 默认值
    ivs.fill(31); // 满IV
    evs.fill(0);  // 0 EV
    
    // 从Json读取IVs
    if (jsonData.contains("ivs")) {
        const auto& ivData = jsonData["ivs"];
        if (ivData.contains("hp")) ivs[static_cast<int>(StatIndex::HP)] = ivData["hp"].get<int>();
        if (ivData.contains("attack")) ivs[static_cast<int>(StatIndex::Attack)] = ivData["attack"].get<int>();
        if (ivData.contains("defense")) ivs[static_cast<int>(StatIndex::Defense)] = ivData["defense"].get<int>();
        if (ivData.contains("specialAttack")) ivs[static_cast<int>(StatIndex::SpecialAttack)] = ivData["specialAttack"].get<int>();
        if (ivData.contains("specialDefense")) ivs[static_cast<int>(StatIndex::SpecialDefense)] = ivData["specialDefense"].get<int>();
        if (ivData.contains("speed")) ivs[static_cast<int>(StatIndex::Speed)] = ivData["speed"].get<int>();
    }
    
    // 从Json读取EVs
    if (jsonData.contains("evs")) {
        const auto& evData = jsonData["evs"];
        if (evData.contains("hp")) evs[static_cast<int>(StatIndex::HP)] = evData["hp"].get<int>();
        if (evData.contains("attack")) evs[static_cast<int>(StatIndex::Attack)] = evData["attack"].get<int>();
        if (evData.contains("defense")) evs[static_cast<int>(StatIndex::Defense)] = evData["defense"].get<int>();
        if (evData.contains("specialAttack")) evs[static_cast<int>(StatIndex::SpecialAttack)] = evData["specialAttack"].get<int>();
        if (evData.contains("specialDefense")) evs[static_cast<int>(StatIndex::SpecialDefense)] = evData["specialDefense"].get<int>();
        if (evData.contains("speed")) evs[static_cast<int>(StatIndex::Speed)] = evData["speed"].get<int>();
    }
    
    // 读取其他属性
    Nature nature = stringToNature(jsonData.value("nature", "hardy"));
    AbilityType ability = AbilityType::None;
    if (jsonData.contains("ability")) {
        ability = parseAbilityValue(jsonData["ability"]);
    }
    int level = jsonData.value("level", 50);
    ItemType heldItem = ItemType::None;
    if (jsonData.contains("item")) {
        heldItem = parseItemValue(jsonData["item"]);
    }
    
    // 判断是否为隐藏特性
    bool isHiddenAbil = (species.hiddenAbility == ability);
    
    // 创建Pokemon对象
    Pokemon pokemon(species, nature, ability, isHiddenAbil, level, ivs, evs, heldItem);
    
    // 设置技能
    if (jsonData.contains("moves")) {
        for (const auto& moveId : jsonData["moves"]) {
            pokemon.addMove(parseMoveValue(moveId));
        }
    }
    
    return pokemon;
}

// 从species.json加载所有种族信息
std::map<int, Species> loadSpeciesFromFile() {
    std::map<int, Species> speciesMap;
    
    // 尝试使用不同的路径
    std::string paths[] = {
        "data/species.json",
        "../data/species.json",
        "../../data/species.json"
    };
    
    std::ifstream file;
    bool fileOpened = false;
    
    for (const auto& path : paths) {
        file.open(path);
        if (file.is_open()) {
            fileOpened = true;
            break;
        }
        file.close();
    }
    
    if (!fileOpened) {
        std::cerr << "Error: Could not open species.json file" << std::endl;
        return speciesMap;
    }
    
    json jsonData;
    file >> jsonData;
    file.close();
    
    if (jsonData.contains("species")) {
        for (const auto& speciesData : jsonData["species"]) {
            Species species = BuildFromJson::buildSpecies(speciesData);
            speciesMap[species.id] = species;
        }
    }
    
    return speciesMap;
}

// 从abilities.json加载所有特性信息
std::map<int, Ability> loadAbilitiesFromFile() {
    std::map<int, Ability> abilityMap;
    
    // 尝试使用不同的路径
    std::string paths[] = {
        "data/abilities.json",
        "../data/abilities.json",
        "../../data/abilities.json"
    };
    
    std::ifstream file;
    bool fileOpened = false;
    
    for (const auto& path : paths) {
        file.open(path);
        if (file.is_open()) {
            fileOpened = true;
            break;
        }
        file.close();
    }
    
    if (!fileOpened) {
        std::cerr << "Error: Could not open abilities.json file" << std::endl;
        return abilityMap;
    }
    
    json jsonData;
    file >> jsonData;
    file.close();
    
    if (jsonData.contains("abilities")) {
        for (const auto& abilityData : jsonData["abilities"]) {
            int id = abilityData.value("id", 0);
            abilityMap[id] = getAbility(getAbilityTypeById(id));
        }
    }
    
    return abilityMap;
}

// 从items.json加载所有物品信息
std::map<int, Item> loadItemsFromFile() {
    std::map<int, Item> itemMap;
    
    // 尝试使用不同的路径
    std::string paths[] = {
        "data/items.json",
        "../data/items.json",
        "../../data/items.json"
    };
    
    std::ifstream file;
    bool fileOpened = false;
    
    for (const auto& path : paths) {
        file.open(path);
        if (file.is_open()) {
            fileOpened = true;
            break;
        }
        file.close();
    }
    
    if (!fileOpened) {
        std::cerr << "Error: Could not open items.json file" << std::endl;
        return itemMap;
    }
    
    json jsonData;
    file >> jsonData;
    file.close();
    
    if (jsonData.contains("items")) {
        for (const auto& itemData : jsonData["items"]) {
            int id = itemData.value("id", 0);
            std::string name = itemData.value("name", "");
            std::string description = itemData.value("description", "");
            
            // 通过switch case获取物品实例
            Item item;
            switch (id) {
                case 1:
                    item = getItem(ItemType::None); // 暂时使用None代替Leppa Berry
                    break;
                default:
                    item = getItem(ItemType::None);
                    break;
            }
            
            itemMap[id] = item;
        }
    }
    
    return itemMap;
}

// 从moves.json加载所有技能信息
std::map<int, Move> loadMovesFromFile() {
    std::map<int, Move> moveMap;
    
    // 尝试使用不同的路径
    std::string paths[] = {
        "data/moves.json",
        "../data/moves.json",
        "../../data/moves.json"
    };
    
    std::ifstream file;
    bool fileOpened = false;
    
    for (const auto& path : paths) {
        file.open(path);
        if (file.is_open()) {
            fileOpened = true;
            break;
        }
        file.close();
    }
    
    if (!fileOpened) {
        std::cerr << "Error: Could not open moves.json file" << std::endl;
        return moveMap;
    }
    
    json jsonData;
    file >> jsonData;
    file.close();
    
    if (jsonData.contains("moves") && jsonData["moves"].is_array()) {
        for (const auto& moveData : jsonData["moves"]) {
            Move move = createMoveByName("Tackle");
            int id = moveData.value("id", 0);

            if (id > 0) {
                move = createMoveById(id);
            } else if (moveData.contains("apiName") && moveData["apiName"].is_string()) {
                move = createMoveByName(moveData["apiName"].get<std::string>());
            } else if (moveData.contains("name") && moveData["name"].is_string()) {
                move = createMoveByName(moveData["name"].get<std::string>());
            }

            const int moveId = move.getData().id > 0 ? move.getData().id : id;
            if (moveId > 0) {
                moveMap[moveId] = move;
            }
        }
    }
    
    return moveMap;
}

// 从Json文件加载宝可梦数据
Pokemon BuildFromJson::loadPokemonFromFile(const std::string& filePath) {
    // 尝试使用不同的路径
    std::string paths[] = {
        filePath,
        "../" + filePath,
        "../../" + filePath
    };
    
    std::ifstream file;
    bool fileOpened = false;
    
    for (const auto& path : paths) {
        file.open(path);
        if (file.is_open()) {
            fileOpened = true;
            break;
        }
        file.close();
    }
    
    if (!fileOpened) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        // 返回一个默认的宝可梦
        Species defaultSpecies;
        Pokemon pokemon(defaultSpecies, Nature::Hardy, AbilityType::None, false, 50, {}, {});
        return pokemon;
    }
    
    json jsonData;
    file >> jsonData;
    file.close();

    Species species;
    if (jsonData.contains("species") && jsonData["species"].is_object()) {
        species = BuildFromJson::buildSpecies(jsonData["species"]);
    } else {
        // 加载种族信息
        std::map<int, Species> speciesMap = loadSpeciesFromFile();

        // 从宝可梦数据中获取speciesID
        int speciesID = jsonData.value("speciesID", 0);

        // 查找对应的种族
        if (speciesMap.find(speciesID) != speciesMap.end()) {
            species = speciesMap[speciesID];
        } else {
            std::cerr << "Error: Species with ID " << speciesID << " not found" << std::endl;
            // 使用默认的种族
        }
    }

    // 构建Pokemon
    Pokemon pokemon = buildPokemon(jsonData, species);
    return pokemon;
}

// 从Json字符串加载宝可梦数据
Pokemon BuildFromJson::loadPokemonFromString(const std::string& jsonString) {
    json jsonData = json::parse(jsonString);

    Species species;
    if (jsonData.contains("species") && jsonData["species"].is_object()) {
        species = BuildFromJson::buildSpecies(jsonData["species"]);
    } else {
        // 加载种族信息
        std::map<int, Species> speciesMap = loadSpeciesFromFile();

        // 从宝可梦数据中获取speciesID
        int speciesID = jsonData.value("speciesID", 0);

        // 查找对应的种族
        if (speciesMap.find(speciesID) != speciesMap.end()) {
            species = speciesMap[speciesID];
        } else {
            std::cerr << "Error: Species with ID " << speciesID << " not found" << std::endl;
            // 使用默认的种族
        }
    }

    // 构建Pokemon
    Pokemon pokemon = buildPokemon(jsonData, species);
    return pokemon;
}
