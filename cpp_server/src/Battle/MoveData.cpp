#include "Battle/Moves.h"

#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
std::mutex gMoveCacheMutex;

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string normalizeName(const std::string& name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (char ch : name) {
        if (ch == ' ' || ch == '\'' || ch == '-') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

std::string slugToDisplayName(const std::string& slug) {
    std::string result;
    bool upper = true;
    for (char ch : slug) {
        if (ch == '-') {
            result.push_back(' ');
            upper = true;
            continue;
        }
        if (upper && std::isalpha(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            upper = false;
        } else {
            result.push_back(ch);
            upper = false;
        }
    }
    return result;
}

const std::vector<std::string> kMoveJsonPaths = {
    "data/moves.json",
    "../data/moves.json",
    "../../data/moves.json"
};

std::string resolveReadableMoveJsonPath() {
    for (const std::string& path : kMoveJsonPaths) {
        std::ifstream f(path);
        if (f.is_open()) {
            return path;
        }
    }
    return kMoveJsonPaths.front();
}

std::string resolveWritableMoveJsonPath() {
    for (const std::string& path : kMoveJsonPaths) {
        std::ifstream f(path);
        if (f.is_open()) {
            return path;
        }
    }
    return kMoveJsonPaths.front();
}

Type parseType(const std::string& value) {
    const std::string v = toLower(trim(value));
    if (v == "fire") return Type::Fire;
    if (v == "water") return Type::Water;
    if (v == "grass") return Type::Grass;
    if (v == "electric") return Type::Electric;
    if (v == "ice") return Type::Ice;
    if (v == "fighting") return Type::Fighting;
    if (v == "poison") return Type::Poison;
    if (v == "ground") return Type::Ground;
    if (v == "flying") return Type::Flying;
    if (v == "psychic") return Type::Psychic;
    if (v == "bug") return Type::Bug;
    if (v == "rock") return Type::Rock;
    if (v == "ghost") return Type::Ghost;
    if (v == "dragon") return Type::Dragon;
    if (v == "dark") return Type::Dark;
    if (v == "steel") return Type::Steel;
    if (v == "fairy") return Type::Fairy;
    return Type::Normal;
}

Category parseCategory(const std::string& value) {
    const std::string v = toLower(trim(value));
    if (v == "physical") return Category::Physical;
    if (v == "special") return Category::Special;
    return Category::Status;
}

MoveEffect parseEffect(const std::string& value) {
    const std::string v = toLower(trim(value));
    if (v == "pursuit") return MoveEffect::Pursuit;
    if (v == "encore") return MoveEffect::Encore;
    if (v == "dig") return MoveEffect::Dig;
    if (v == "round") return MoveEffect::Round;
    if (v == "knockoff" || v == "knock_off" || v == "knock-off") return MoveEffect::KnockOff;
    if (v == "weatherball" || v == "weather_ball" || v == "weather-ball") return MoveEffect::WeatherBall;
    if (v == "burn") return MoveEffect::Burn;
    if (v == "freeze") return MoveEffect::Freeze;
    if (v == "poison" || v == "bad-poison" || v == "badly-poisoned") return MoveEffect::Poison;
    if (v == "paralyze" || v == "paralysis") return MoveEffect::Paralyze;
    if (v == "sleep") return MoveEffect::Sleep;
    if (v == "confuse" || v == "confusion") return MoveEffect::Confuse;
    if (v == "drain") return MoveEffect::Drain;
    if (v == "recoil") return MoveEffect::Recoil;
    if (v == "flinch") return MoveEffect::Flinch;
    if (v == "safeguard" || v == "protect") return MoveEffect::Safeguard;
    if (v == "statchange" || v == "stat_change") return MoveEffect::StatChange;
    return MoveEffect::None;
}

Target parseTarget(const std::string& value) {
    const std::string v = toLower(trim(value));
    if (v == "user") return Target::Self;
    if (v == "ally") return Target::Ally;
    if (v == "all-allies") return Target::AllAllies;
    if (v == "all-opponents") return Target::AllOpponents;
    if (v == "all-pokemon" || v == "everyone") return Target::All;
    return Target::Opponent;
}

std::string effectToString(MoveEffect effect) {
    switch (effect) {
        case MoveEffect::Pursuit: return "Pursuit";
        case MoveEffect::Encore: return "Encore";
        case MoveEffect::Dig: return "Dig";
        case MoveEffect::Round: return "Round";
        case MoveEffect::KnockOff: return "KnockOff";
        case MoveEffect::WeatherBall: return "WeatherBall";
        case MoveEffect::Burn: return "Burn";
        case MoveEffect::Freeze: return "Freeze";
        case MoveEffect::Poison: return "Poison";
        case MoveEffect::Paralyze: return "Paralyze";
        case MoveEffect::Sleep: return "Sleep";
        case MoveEffect::Confuse: return "Confuse";
        case MoveEffect::Drain: return "Drain";
        case MoveEffect::Recoil: return "Recoil";
        case MoveEffect::Flinch: return "Flinch";
        case MoveEffect::Safeguard: return "Safeguard";
        case MoveEffect::StatChange: return "StatChange";
        default: return "None";
    }
}

int parseStatIndex(const std::string& statName) {
    const std::string key = toLower(trim(statName));
    if (key == "attack") return 1;
    if (key == "defense") return 2;
    if (key == "special-attack") return 3;
    if (key == "special-defense") return 4;
    if (key == "speed") return 5;
    return 0;
}

MoveData fallbackMoveDataById(int id) {
    switch (id) {
        case 1: return {1, "Ember", "ember", "A small fire attack that may burn the target", Type::Fire, Category::Special, 40, 100, 25, MoveEffect::Burn, 10, 0, 0, 0, Target::Opponent};
        case 2: return {2, "Dragon Claw", "dragon-claw", "A sharp claw attack with dragon power", Type::Dragon, Category::Physical, 80, 100, 15, MoveEffect::None, 0, 0, 0, 0, Target::Opponent};
        case 3: return {3, "Fly", "fly", "Flies up on the first turn, attacks on the second", Type::Flying, Category::Physical, 90, 95, 15, MoveEffect::None, 0, 0, 0, 0, Target::Opponent};
        case 4: return {4, "Earthquake", "earthquake", "A powerful ground attack that hits all Pokemon on the field", Type::Ground, Category::Physical, 100, 100, 10, MoveEffect::None, 0, 0, 0, 0, Target::AllOpponents};
        case 5: return {5, "Water Gun", "water-gun", "A basic water attack", Type::Water, Category::Special, 40, 100, 25, MoveEffect::None, 0, 0, 0, 0, Target::Opponent};
        case 6: return {6, "Hydro Pump", "hydro-pump", "A powerful water attack with low accuracy", Type::Water, Category::Special, 110, 80, 5, MoveEffect::None, 0, 0, 0, 0, Target::Opponent};
        case 7: return {7, "Ice Beam", "ice-beam", "An ice attack that may freeze the target", Type::Ice, Category::Special, 90, 100, 10, MoveEffect::Freeze, 10, 0, 0, 0, Target::Opponent};
        case 8: return {8, "Protect", "protect", "Protects the user from attacks for one turn", Type::Normal, Category::Status, 0, 100, 10, MoveEffect::Safeguard, 100, 0, 0, 4, Target::Self};
        default: return {0, "", "", "", Type::Normal, Category::Status, 0, 100, 0, MoveEffect::None, 0, 0, 0, 0, Target::Opponent};
    }
}

MoveData defaultTackleData() {
    return {0, "Tackle", "tackle", "Fallback move", Type::Normal, Category::Physical, 40, 100, 35, MoveEffect::None, 0, 0, 0, 0, Target::Opponent};
}

json loadMoveRoot() {
    const std::string path = resolveReadableMoveJsonPath();
    std::ifstream file(path);
    if (!file.is_open()) {
        return json{{"moves", json::array()}};
    }

    json root;
    file >> root;
    if (!root.contains("moves") || !root["moves"].is_array()) {
        root["moves"] = json::array();
    }
    return root;
}

MoveData parseMoveEntry(const json& entry) {
    MoveData data = {
        entry.value("id", 0),
        entry.value("name", ""),
        entry.value("apiName", ""),
        entry.value("description", ""),
        parseType(entry.value("type", "normal")),
        parseCategory(entry.value("category", "status")),
        entry.value("power", 0),
        entry.value("accuracy", 100),
        entry.value("pp", 0),
        parseEffect(entry.value("effect", "none")),
        entry.value("effectChance", 0),
        entry.value("effectParam1", 0),
        entry.value("effectParam2", 0),
        entry.value("priority", 0),
        parseTarget(entry.value("target", "selected-pokemon"))
    };

    if (data.apiName.empty() && !data.name.empty()) {
        std::string apiName;
        apiName.reserve(data.name.size());
        for (char c : data.name) {
            if (c == ' ') {
                apiName.push_back('-');
            } else {
                apiName.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        data.apiName = apiName;
    }

    // 兼容旧格式：只有 name/description 时，回退到 legacy 数值。
    if ((data.power == 0 && data.category != Category::Status) || data.pp == 0) {
        MoveData legacy = fallbackMoveDataById(data.id);
        if (legacy.id != 0) {
            if (data.name.empty()) data.name = legacy.name;
            if (data.apiName.empty()) data.apiName = legacy.apiName;
            if (data.description.empty()) data.description = legacy.description;
            if (entry.find("type") == entry.end()) data.type = legacy.type;
            if (entry.find("category") == entry.end()) data.category = legacy.category;
            if (entry.find("power") == entry.end()) data.power = legacy.power;
            if (entry.find("accuracy") == entry.end()) data.accuracy = legacy.accuracy;
            if (entry.find("pp") == entry.end()) data.pp = legacy.pp;
            if (entry.find("effect") == entry.end()) data.effect = legacy.effect;
            if (entry.find("effectChance") == entry.end()) data.effectChance = legacy.effectChance;
            if (entry.find("effectParam1") == entry.end()) data.effectParam1 = legacy.effectParam1;
            if (entry.find("effectParam2") == entry.end()) data.effectParam2 = legacy.effectParam2;
        }
    }

    if (data.name.empty() && !data.apiName.empty()) {
        data.name = slugToDisplayName(data.apiName);
    }
    return data;
}

size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total = size * nmemb;
    std::string* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

bool fetchMovePayload(int id, json& outPayload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string response;
    const std::string url = "https://pokeapi.co/api/v2/move/" + std::to_string(id);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PokemonSimulator/1.0");

    const CURLcode res = curl_easy_perform(curl);
    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || statusCode != 200) {
        return false;
    }

    outPayload = json::parse(response, nullptr, false);
    return !outPayload.is_discarded();
}

int jsonIntOr(const json& obj, const char* key, int fallback) {
    if (!obj.contains(key) || obj[key].is_null()) {
        return fallback;
    }
    if (obj[key].is_number_integer()) {
        return obj[key].get<int>();
    }
    if (obj[key].is_number_unsigned()) {
        return static_cast<int>(obj[key].get<unsigned int>());
    }
    if (obj[key].is_number_float()) {
        return static_cast<int>(obj[key].get<double>());
    }
    return fallback;
}

MoveData moveDataFromPokeAPI(const json& payload) {
    MoveData data = defaultTackleData();
    data.id = payload.value("id", 0);
    data.apiName = payload.value("name", "");
    data.name = slugToDisplayName(data.apiName);
    data.description = "";

    if (payload.contains("effect_entries") && payload["effect_entries"].is_array()) {
        for (const auto& entry : payload["effect_entries"]) {
            if (entry.value("language", json::object()).value("name", "") == "en") {
                data.description = entry.value("short_effect", entry.value("effect", ""));
                break;
            }
        }
    }

    data.type = parseType(payload.value("type", json::object()).value("name", "normal"));
    data.category = parseCategory(payload.value("damage_class", json::object()).value("name", "status"));
    data.power = jsonIntOr(payload, "power", 0);
    data.accuracy = jsonIntOr(payload, "accuracy", 100);
    data.pp = jsonIntOr(payload, "pp", 0);
    data.priority = jsonIntOr(payload, "priority", 0);
    data.effectChance = jsonIntOr(payload, "effect_chance", 0);
    data.effect = MoveEffect::None;
    data.effectParam1 = 0;
    data.effectParam2 = 0;

    const json meta = payload.value("meta", json::object());
    const int drain = jsonIntOr(meta, "drain", 0);
    const int flinchChance = jsonIntOr(meta, "flinch_chance", 0);
    const std::string ailment = meta.value("ailment", json::object()).value("name", "none");
    const int ailmentChance = jsonIntOr(meta, "ailment_chance", 0);

    const std::string moveTarget = payload.value("target", json::object()).value("name", "selected-pokemon");
    data.target = parseTarget(moveTarget);

    if (drain > 0) {
        data.effect = MoveEffect::Drain;
        data.effectParam1 = drain;
    } else if (drain < 0) {
        data.effect = MoveEffect::Recoil;
        data.effectParam1 = -drain;
    } else if (flinchChance > 0) {
        data.effect = MoveEffect::Flinch;
        data.effectChance = flinchChance;
    } else {
        const MoveEffect ailmentEffect = parseEffect(ailment);
        if (ailmentEffect != MoveEffect::None) {
            data.effect = ailmentEffect;
            if (ailmentChance > 0) {
                data.effectChance = ailmentChance;
            }
        }
    }

    if (data.effect == MoveEffect::None && payload.contains("stat_changes") && payload["stat_changes"].is_array() && !payload["stat_changes"].empty()) {
        const auto& statChange = payload["stat_changes"][0];
        const int statIndex = parseStatIndex(statChange.value("stat", json::object()).value("name", ""));
        const int stageDelta = jsonIntOr(statChange, "change", 0);
        if (statIndex > 0 && stageDelta != 0) {
            data.effect = MoveEffect::StatChange;
            const bool targetsSelf = (moveTarget == "user" || moveTarget == "users-field" || moveTarget == "user-and-allies");
            data.effectParam1 = targetsSelf ? -statIndex : statIndex;
            data.effectParam2 = stageDelta;
            if (data.effectChance <= 0) {
                data.effectChance = 100;
            }
        }
    }

    if (data.id == 182 || toLower(data.apiName) == "protect") {
        data.effect = MoveEffect::Safeguard;
        data.effectChance = 100;
    }

    if (toLower(data.apiName) == "toxic") {
        data.effect = MoveEffect::Poison;
        data.effectChance = 100;
        data.effectParam1 = 2; // 2 表示剧毒
    }

    if (data.id <= 0) {
        return defaultTackleData();
    }
    return data;
}

bool writeMoveRootAtomically(const std::string& path, const json& root) {
    const std::string tempPath = path + ".tmp";

    {
        std::ofstream out(tempPath, std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << root.dump(2);
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, path, ec);
    if (!ec) {
        return true;
    }

    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(tempPath, path, ec);
    return !ec;
}

bool upsertMoveEntry(const MoveData& data, const json& apiPayload) {
    if (data.id <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(gMoveCacheMutex);
    const std::string path = resolveWritableMoveJsonPath();
    json root = loadMoveRoot();

    json entry;
    entry["id"] = data.id;
    entry["name"] = data.name;
    entry["apiName"] = data.apiName;
    entry["description"] = data.description;
    entry["type"] = (data.type == Type::Count) ? "normal" : toLower(slugToDisplayName(std::to_string(static_cast<int>(data.type))));

    // 明确写入字符串字段，避免依赖枚举序号。
    switch (data.type) {
        case Type::Normal: entry["type"] = "normal"; break;
        case Type::Fire: entry["type"] = "fire"; break;
        case Type::Water: entry["type"] = "water"; break;
        case Type::Electric: entry["type"] = "electric"; break;
        case Type::Grass: entry["type"] = "grass"; break;
        case Type::Ice: entry["type"] = "ice"; break;
        case Type::Fighting: entry["type"] = "fighting"; break;
        case Type::Poison: entry["type"] = "poison"; break;
        case Type::Ground: entry["type"] = "ground"; break;
        case Type::Flying: entry["type"] = "flying"; break;
        case Type::Psychic: entry["type"] = "psychic"; break;
        case Type::Bug: entry["type"] = "bug"; break;
        case Type::Rock: entry["type"] = "rock"; break;
        case Type::Ghost: entry["type"] = "ghost"; break;
        case Type::Dragon: entry["type"] = "dragon"; break;
        case Type::Dark: entry["type"] = "dark"; break;
        case Type::Steel: entry["type"] = "steel"; break;
        case Type::Fairy: entry["type"] = "fairy"; break;
        default: entry["type"] = "normal"; break;
    }

    switch (data.category) {
        case Category::Physical: entry["category"] = "physical"; break;
        case Category::Special: entry["category"] = "special"; break;
        default: entry["category"] = "status"; break;
    }

    entry["power"] = data.power;
    entry["accuracy"] = data.accuracy;
    entry["pp"] = data.pp;
    entry["effect"] = effectToString(data.effect);
    entry["effectChance"] = data.effectChance;
    entry["effectParam1"] = data.effectParam1;
    entry["effectParam2"] = data.effectParam2;

    // 扩展字段：保留常用 API 元信息。
    entry["priority"] = jsonIntOr(apiPayload, "priority", 0);
    entry["target"] = apiPayload.value("target", json::object()).value("name", "selected-pokemon");
    entry["damageClass"] = apiPayload.value("damage_class", json::object()).value("name", "status");
    if (apiPayload.contains("meta")) {
        entry["meta"] = apiPayload["meta"];
    }

    bool replaced = false;
    for (auto& existing : root["moves"]) {
        if (existing.value("id", -1) == data.id) {
            existing = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        root["moves"].push_back(entry);
    }

    std::sort(root["moves"].begin(), root["moves"].end(), [](const json& a, const json& b) {
        return a.value("id", 0) < b.value("id", 0);
    });

    return writeMoveRootAtomically(path, root);
}

std::set<int> collectReferencedMoveIds() {
    std::set<int> ids;

    const std::vector<std::string> sourcePaths = {
        "data/species.json",
        "../data/species.json",
        "../../data/species.json"
    };

    for (const std::string& path : sourcePaths) {
        std::ifstream file(path);
        if (!file.is_open()) {
            continue;
        }
        json root;
        file >> root;
        if (root.contains("species") && root["species"].is_array()) {
            for (const auto& spec : root["species"]) {
                if (!spec.contains("learnableMoves") || !spec["learnableMoves"].is_array()) {
                    continue;
                }
                for (const auto& moveId : spec["learnableMoves"]) {
                    if (moveId.is_number_integer()) {
                        ids.insert(moveId.get<int>());
                    }
                }
            }
        }
        break;
    }

    const std::vector<std::string> dataRoots = {"data", "../data", "../../data"};
    for (const std::string& rootPath : dataRoots) {
        std::error_code ec;
        if (!std::filesystem::exists(rootPath, ec)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(rootPath, ec)) {
            if (ec || !entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() != ".json") {
                continue;
            }

            std::ifstream f(entry.path());
            if (!f.is_open()) {
                continue;
            }
            json obj;
            f >> obj;
            if (!obj.contains("moves") || !obj["moves"].is_array()) {
                continue;
            }

            for (const auto& moveId : obj["moves"]) {
                if (moveId.is_number_integer()) {
                    ids.insert(moveId.get<int>());
                }
            }
        }
        break;
    }

    return ids;
}
}  // namespace

MoveData getMoveDataById(int id) {
    if (id <= 0) {
        return defaultTackleData();
    }

    json root = loadMoveRoot();
    for (const auto& entry : root["moves"]) {
        if (entry.value("id", -1) != id) {
            continue;
        }
        MoveData data = parseMoveEntry(entry);
        if (data.id == 0) {
            data.id = id;
        }
        if (data.name.empty()) {
            MoveData legacy = fallbackMoveDataById(id);
            if (legacy.id != 0) {
                return legacy;
            }
            return defaultTackleData();
        }
        return data;
    }

    MoveData legacy = fallbackMoveDataById(id);
    if (legacy.id != 0) {
        return legacy;
    }
    return defaultTackleData();
}

MoveData getMoveDataByName(const std::string& name) {
    const std::string normalized = normalizeName(name);
    json root = loadMoveRoot();
    for (const auto& entry : root["moves"]) {
        const std::string entryName = entry.value("name", "");
        const std::string entryApiName = entry.value("apiName", "");
        if (normalizeName(entryName) == normalized || normalizeName(entryApiName) == normalized) {
            return parseMoveEntry(entry);
        }
    }

    for (int id = 1; id <= 8; ++id) {
        MoveData data = fallbackMoveDataById(id);
        if (normalizeName(data.name) == normalized || normalizeName(data.apiName) == normalized) {
            return data;
        }
    }

    return defaultTackleData();
}

Move createMoveFromData(const MoveData& data) {
    return Move(data.name, data.type, data.category, data.power, data.accuracy, data.pp,
                data.effect, data.effectChance, data.effectParam1, data.effectParam2, data.priority, data.target);
}

Move createMoveById(int id) {
    return createMoveFromData(getMoveDataById(id));
}

Move createMoveByName(const std::string& name) {
    return createMoveFromData(getMoveDataByName(name));
}

bool prefetchMovesFromPokeAPI(bool refreshExisting) {
    const std::set<int> moveIds = collectReferencedMoveIds();
    if (moveIds.empty()) {
        std::cerr << "No move IDs found in local JSON data." << std::endl;
        return false;
    }

    bool success = true;
    for (int id : moveIds) {
        const MoveData localData = getMoveDataById(id);
        const bool hasLocal = (localData.id == id && !localData.name.empty() && localData.name != "Tackle");
        if (!refreshExisting && hasLocal) {
            continue;
        }

        json payload;
        if (!fetchMovePayload(id, payload)) {
            std::cerr << "Failed to fetch move id " << id << " from PokeAPI." << std::endl;
            success = false;
            continue;
        }

        MoveData fetched = moveDataFromPokeAPI(payload);
        if (fetched.id != id) {
            std::cerr << "Fetched move id mismatch for id " << id << std::endl;
            success = false;
            continue;
        }

        if (!upsertMoveEntry(fetched, payload)) {
            std::cerr << "Failed to write move id " << id << " into moves.json." << std::endl;
            success = false;
            continue;
        }
    }

    return success;
}
