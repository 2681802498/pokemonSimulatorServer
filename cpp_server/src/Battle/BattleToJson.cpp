#include "Battle/BattleToJson.h"
#include "Battle/Types.h"
#include "Battle/Natures.h"
#include "Battle/Abilities.h"
#include "Battle/Items.h"
#include "Battle/Status.h"
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>

// 将Type转换为字符串
std::string BattleToJson::typeToString(Type type) {
    switch (type) {
        case Type::Normal: return "normal";
        case Type::Fire: return "fire";
        case Type::Water: return "water";
        case Type::Electric: return "electric";
        case Type::Grass: return "grass";
        case Type::Ice: return "ice";
        case Type::Fighting: return "fighting";
        case Type::Poison: return "poison";
        case Type::Ground: return "ground";
        case Type::Flying: return "flying";
        case Type::Psychic: return "psychic";
        case Type::Bug: return "bug";
        case Type::Rock: return "rock";
        case Type::Ghost: return "ghost";
        case Type::Dragon: return "dragon";
        case Type::Dark: return "dark";
        case Type::Steel: return "steel";
        case Type::Fairy: return "fairy";
        default: return "count";
    }
}

// 将ActionType转换为字符串
std::string BattleToJson::actionTypeToString(ActionType type) {
    switch (type) {
        case ActionType::Attack: return "attack";
        case ActionType::Switch: return "switch";
        case ActionType::UseItem: return "use_item";
        case ActionType::Pass: return "pass";
        default: return "pass";
    }
}

// 将Nature转换为字符串
std::string BattleToJson::natureToString(Nature nature) {
    switch (nature) {
        case Nature::Hardy: return "hardy";
        case Nature::Lonely: return "lonely";
        case Nature::Brave: return "brave";
        case Nature::Adamant: return "adamant";
        case Nature::Naughty: return "naughty";
        case Nature::Bold: return "bold";
        case Nature::Docile: return "docile";
        case Nature::Relaxed: return "relaxed";
        case Nature::Impish: return "impish";
        case Nature::Lax: return "lax";
        case Nature::Timid: return "timid";
        case Nature::Hasty: return "hasty";
        case Nature::Serious: return "serious";
        case Nature::Jolly: return "jolly";
        case Nature::Naive: return "naive";
        case Nature::Modest: return "modest";
        case Nature::Mild: return "mild";
        case Nature::Quiet: return "quiet";
        case Nature::Bashful: return "bashful";
        case Nature::Rash: return "rash";
        case Nature::Calm: return "calm";
        case Nature::Gentle: return "gentle";
        case Nature::Sassy: return "sassy";
        case Nature::Careful: return "careful";
        case Nature::Quirky: return "quirky";
        default: return "hardy";
    }
}

// 将AbilityType转换为字符串
std::string BattleToJson::abilityTypeToString(AbilityType ability) {
    switch (ability) {
        case AbilityType::Blaze: return "blaze";
        case AbilityType::Torrent: return "torrent";
        case AbilityType::Overgrow: return "overgrow";
        case AbilityType::Intimidate: return "intimidate";
        case AbilityType::Multiscale: return "multiscale";
        case AbilityType::Levitate: return "levitate";
        case AbilityType::WaterAbsorb: return "water-absorb";
        case AbilityType::VoltAbsorb: return "volt-absorb";
        case AbilityType::FlashFire: return "flash-fire";
        case AbilityType::Static: return "static";
        case AbilityType::PoisonPoint: return "poison-point";
        case AbilityType::Aftermath: return "aftermath";
        case AbilityType::Mummy: return "mummy";
        case AbilityType::RoughSkin: return "rough-skin";
        case AbilityType::FlameBody: return "flame-body";
        case AbilityType::Insomnia: return "insomnia";
        case AbilityType::VitalSpirit: return "vital-spirit";
        case AbilityType::Guts: return "guts";
        case AbilityType::HugePower: return "huge-power";
        case AbilityType::ThickFat: return "thick-fat";
        case AbilityType::MarvelScale: return "marvel-scale";
        case AbilityType::SapSipper: return "sap-sipper";
        case AbilityType::IronBarbs: return "iron-barbs";
        case AbilityType::StormDrain: return "storm-drain";
        case AbilityType::MotorDrive: return "motor-drive";
        case AbilityType::Immunity: return "immunity";
        case AbilityType::Technician: return "technician";
        case AbilityType::Filter: return "filter";
        case AbilityType::SolidRock: return "solid-rock";
        case AbilityType::Moxie: return "moxie";
        case AbilityType::InnerFocus: return "inner-focus";
        case AbilityType::Regenerator: return "regenerator";
        case AbilityType::NaturalCure: return "natural-cure";
        case AbilityType::MagicGuard: return "magic-guard";
        case AbilityType::Unaware: return "unaware";
        case AbilityType::Prankster: return "prankster";
        case AbilityType::ClearBody: return "clear-body";
        case AbilityType::Defiant: return "defiant";
        case AbilityType::Competitive: return "competitive";
        case AbilityType::WhiteSmoke: return "white-smoke";
        case AbilityType::MirrorArmor: return "mirror-armor";
        case AbilityType::HyperCutter: return "hyper-cutter";
        case AbilityType::KeenEye: return "keen-eye";
        case AbilityType::Drizzle: return "drizzle";
        case AbilityType::Drought: return "drought";
        case AbilityType::SandStream: return "sand-stream";
        case AbilityType::SnowWarning: return "snow-warning";
        case AbilityType::CloudNine: return "cloud-nine";
        case AbilityType::GrassySurge: return "grassy-surge";
        case AbilityType::ElectricSurge: return "electric-surge";
        case AbilityType::PsychicSurge: return "psychic-surge";
        case AbilityType::MistySurge: return "misty-surge";
        case AbilityType::HadronEngine: return "hadron-engine";
        case AbilityType::Protean: return "protean";
        case AbilityType::Libero: return "libero";
        case AbilityType::Adaptability: return "adaptability";
        case AbilityType::SheerForce: return "sheer-force";
        case AbilityType::Infiltrator: return "infiltrator";
        case AbilityType::BeadsOfRuin: return "beads-of-ruin";
        case AbilityType::SwordOfRuin: return "sword-of-ruin";
        case AbilityType::TabletsOfRuin: return "tablets-of-ruin";
        case AbilityType::VesselOfRuin: return "vessel-of-ruin";
        case AbilityType::Unnerve: return "unnerve";
        case AbilityType::Sharpness: return "sharpness";
        case AbilityType::EarthEater: return "earth-eater";
        case AbilityType::PurifyingSalt: return "purifying-salt";
        case AbilityType::WellBakedBody: return "well-baked-body";
        case AbilityType::WindRider: return "wind-rider";
        case AbilityType::ToxicDebris: return "toxic-debris";
        case AbilityType::LingeringAroma: return "lingering-aroma";
        case AbilityType::ArmorTail: return "armor-tail";
        case AbilityType::GoodAsGold: return "good-as-gold";
        case AbilityType::Stakeout: return "stakeout";
        case AbilityType::CudChew: return "cud-chew";
        case AbilityType::MoldBreaker: return "mold-breaker";
        case AbilityType::Protosynthesis: return "protosynthesis";
        case AbilityType::QuarkDrive: return "quark-drive";
        case AbilityType::SupremeOverlord: return "supreme-overlord";
        default: return "none";
    }
}

// 将ItemType转换为字符串
std::string BattleToJson::itemTypeToString(ItemType item) {
    if (item == ItemType::None) {
        return "none";
    }
    return getItemName(item);
}

std::string BattleToJson::statusTypeToString(StatusType status) {
    switch (status) {
        case StatusType::Burn: return "burn";
        case StatusType::Freeze: return "freeze";
        case StatusType::Paralysis: return "paralysis";
        case StatusType::Poison: return "poison";
        case StatusType::Sleep: return "sleep";
        case StatusType::Flinch: return "flinch";
        case StatusType::ToxicPoison: return "toxic_poison";
        case StatusType::Confusion: return "confusion";
        default: return "none";
    }
}

std::string BattleToJson::weatherTypeToString(WeatherType weather) {
    switch (weather) {
        case WeatherType::Rain: return "rain";
        case WeatherType::Sun: return "sun";
        case WeatherType::Sandstorm: return "sandstorm";
        case WeatherType::Hail: return "hail";
        case WeatherType::Snow: return "snow";
        default: return "clear";
    }
}

std::string BattleToJson::fieldTypeToString(FieldType field) {
    switch (field) {
        case FieldType::Psychic: return "psychic";
        case FieldType::Electric: return "electric";
        case FieldType::Grassy: return "grassy";
        case FieldType::Misty: return "misty";
        case FieldType::TrickRoom: return "trick_room";
        default: return "none";
    }
}

// 将BattleAction转换为Json
json BattleToJson::actionToJson(const BattleAction& action) {
    json jsonAction;
    jsonAction["type"] = actionTypeToString(action.type);
    jsonAction["priority"] = action.priority;
    
    if (action.actor) {
        jsonAction["actor"] = action.actor->getName();
    }
    
    if (action.target) {
        jsonAction["target"] = action.target->getName();
    }
    
    if (action.type == ActionType::Attack) {
        jsonAction["move"] = action.move.getName();
    } else if (action.type == ActionType::Switch) {
        jsonAction["switch_index"] = action.switchIndex;
    } else if (action.type == ActionType::UseItem) {
        jsonAction["item"] = itemTypeToString(action.item);
    }
    
    return jsonAction;
}

// 将Pokemon转换为Json
json BattleToJson::pokemonToJson(const Pokemon* pokemon) {
    if (!pokemon) {
        return json();
    }
    
    json jsonPokemon;
    jsonPokemon["name"] = pokemon->getName();
    jsonPokemon["level"] = pokemon->getLevel();
    jsonPokemon["current_hp"] = pokemon->getCurrentHP();
    jsonPokemon["max_hp"] = pokemon->getMaxHP();
    jsonPokemon["is_fainted"] = pokemon->isFainted();
    
    // 类型
    jsonPokemon["type1"] = typeToString(pokemon->getType1());
    jsonPokemon["type2"] = typeToString(pokemon->getType2());
    
    // 特性
    jsonPokemon["ability"] = abilityTypeToString(pokemon->getAbility());
    
    // 性格
    jsonPokemon["nature"] = natureToString(pokemon->getNature());
    
    // 物品
    jsonPokemon["item"] = itemTypeToString(pokemon->getItemType());
    
    // 技能
    json movesArray = json::array();
    for (const auto& move : pokemon->getMoves()) {
        json moveJson;
        moveJson["name"] = move.getName();
        moveJson["type"] = typeToString(move.getType());
        moveJson["power"] = move.getPower();
        moveJson["accuracy"] = move.getAccuracy();
        moveJson["pp"] = move.getPP();
        moveJson["max_pp"] = move.getMaxPP();
        movesArray.push_back(moveJson);
    }
    jsonPokemon["moves"] = movesArray;

    json statStages;
    statStages["attack"] = pokemon->getStatStage(StatIndex::Attack);
    statStages["defense"] = pokemon->getStatStage(StatIndex::Defense);
    statStages["special_attack"] = pokemon->getStatStage(StatIndex::SpecialAttack);
    statStages["special_defense"] = pokemon->getStatStage(StatIndex::SpecialDefense);
    statStages["speed"] = pokemon->getStatStage(StatIndex::Speed);
    jsonPokemon["stat_stages"] = statStages;

    json statuses = json::array();
    for (const auto& statusEntry : pokemon->getStatuses()) {
        json one;
        one["type"] = statusTypeToString(statusEntry.first);
        one["duration"] = statusEntry.second;
        statuses.push_back(one);
    }
    jsonPokemon["statuses"] = statuses;

    json stats;
    stats["attack"] = pokemon->getAttack();
    stats["defense"] = pokemon->getDefense();
    stats["special_attack"] = pokemon->getSpecialAttack();
    stats["special_defense"] = pokemon->getSpecialDefense();
    stats["speed"] = pokemon->getSpeed();
    jsonPokemon["stats"] = stats;
    
    return jsonPokemon;
}

// 将Side转换为Json
json BattleToJson::sideToJson(const Side& side) {
    json jsonSide;
    jsonSide["name"] = side.getName();
    jsonSide["active_index"] = side.getActiveIndex();
    jsonSide["pokemon_count"] = side.getPokemonCount();
    
    // 当前活跃的宝可梦
    if (side.getActivePokemon()) {
        jsonSide["active_pokemon"] = pokemonToJson(side.getActivePokemon());
    }

    json team = json::array();
    const auto& all = side.getTeam();
    for (int i = 0; i < side.getPokemonCount(); ++i) {
        team.push_back(pokemonToJson(all[i]));
    }
    jsonSide["team"] = team;
    
    return jsonSide;
}

// 将Battle转换为Json
json BattleToJson::battleToJson(Battle& battle) {
    json jsonBattle;
    jsonBattle["turn"] = battle.getTurnNumber();
    
    // 双方
    jsonBattle["side_a"] = sideToJson(battle.getSideA());
    jsonBattle["side_b"] = sideToJson(battle.getSideB());

    json fieldJson;
    fieldJson["type"] = fieldTypeToString(battle.getField().type);
    fieldJson["duration"] = battle.getField().duration;
    jsonBattle["field"] = fieldJson;

    json weatherJson;
    weatherJson["type"] = weatherTypeToString(battle.getWeather().type);
    weatherJson["duration"] = battle.getWeather().duration;
    jsonBattle["weather"] = weatherJson;
    
    return jsonBattle;
}

// 将Json数据写入到cache文件中
void BattleToJson::writeToCache(const json& data, const std::string& filename) {
    // 确保cache目录存在
    std::string cacheDir = "cache";
    std::filesystem::create_directories(cacheDir);
    
    // 构建完整的文件路径
    std::string filePath = cacheDir + "/" + filename;
    
    // 打开文件
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        return;
    }
    
    // 写入Json数据
    file << data.dump(2);
    file.close();
}