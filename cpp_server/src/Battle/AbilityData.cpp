#include "Battle/Abilities.h"

#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
std::mutex gAbilityCacheMutex;

const std::vector<std::string> kAbilityJsonPaths = {
    "data/abilities.json",
    "../data/abilities.json",
    "../../data/abilities.json"
};

const std::vector<std::string> kSpeciesJsonPaths = {
    "data/species.json",
    "../data/species.json",
    "../../data/species.json"
};

const std::vector<std::string> kPokemonJsonRoots = {
    "data",
    "../data",
    "../../data"
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string normalizeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        if (ch == ' ' || ch == '-' || ch == '\'' || ch == '_') {
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
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

AbilityType parseAbilityType(const std::string& value) {
    return getAbilityTypeByName(value);
}

AbilityData fallbackAbilityDataById(int id) {
    switch (id) {
        // 兼容旧本地测试数据 ID。
        case 1: return {1, "Blaze", "blaze", "When HP is below 1/3, Fire-type moves do 1.5x damage", AbilityType::Blaze};
        case 2: return {2, "Torrent", "torrent", "When HP is below 1/3, Water-type moves do 1.5x damage", AbilityType::Torrent};
        case 3: return {3, "Overgrow", "overgrow", "When HP is below 1/3, Grass-type moves do 1.5x damage", AbilityType::Overgrow};
        case 4: return {4, "Intimidate", "intimidate", "Lowers the foe's Attack stat on entry", AbilityType::Intimidate};
        case 5: return {5, "Multiscale", "multiscale", "Reduces damage when at full HP", AbilityType::Multiscale};

        // PokeAPI canonical IDs for abilities currently implemented by battle logic.
        case 9: return {9, "Static", "static", "Contact with the Pokemon may cause paralysis.", AbilityType::Static};
        case 10: return {10, "Volt Absorb", "volt-absorb", "Restores HP if hit by an Electric-type move.", AbilityType::VoltAbsorb};
        case 11: return {11, "Water Absorb", "water-absorb", "Restores HP if hit by a Water-type move.", AbilityType::WaterAbsorb};
        case 13: return {13, "Cloud Nine", "cloud-nine", "Eliminates the effects of weather.", AbilityType::CloudNine};
        case 15: return {15, "Insomnia", "insomnia", "Prevents the Pokemon from falling asleep.", AbilityType::Insomnia};
        case 17: return {17, "Immunity", "immunity", "Prevents the Pokemon from becoming poisoned.", AbilityType::Immunity};
        case 18: return {18, "Flash Fire", "flash-fire", "Powers up Fire-type moves if hit by one.", AbilityType::FlashFire};
        case 38: return {38, "Poison Point", "poison-point", "Contact with the Pokemon may poison the attacker.", AbilityType::PoisonPoint};
        case 24: return {24, "Rough Skin", "rough-skin", "Damages attackers on contact.", AbilityType::RoughSkin};
        case 22: return {22, "Intimidate", "intimidate", "Lowers the foe's Attack stat on entry", AbilityType::Intimidate};
        case 26: return {26, "Levitate", "levitate", "Gives full immunity to Ground-type moves.", AbilityType::Levitate};
        case 29: return {29, "Clear Body", "clear-body", "Prevents other Pokemon from lowering stats.", AbilityType::ClearBody};
        case 51: return {51, "Keen Eye", "keen-eye", "Prevents other Pokemon from lowering accuracy.", AbilityType::KeenEye};
        case 52: return {52, "Hyper Cutter", "hyper-cutter", "Prevents other Pokemon from lowering Attack.", AbilityType::HyperCutter};
        case 37: return {37, "Huge Power", "huge-power", "Doubles the Pokemon's Attack stat.", AbilityType::HugePower};
        case 39: return {39, "Inner Focus", "inner-focus", "Protects the Pokemon from flinching.", AbilityType::InnerFocus};
        case 47: return {47, "Thick Fat", "thick-fat", "Halves damage from Fire- and Ice-type moves.", AbilityType::ThickFat};
        case 49: return {49, "Flame Body", "flame-body", "Contact with the Pokemon may burn the attacker.", AbilityType::FlameBody};
        case 45: return {45, "Sand Stream", "sand-stream", "Summons a sandstorm in battle.", AbilityType::SandStream};
        case 62: return {62, "Guts", "guts", "Boosts Attack when affected by a major status condition.", AbilityType::Guts};
        case 63: return {63, "Marvel Scale", "marvel-scale", "Boosts Defense when affected by a status condition.", AbilityType::MarvelScale};
        case 65: return {65, "Overgrow", "overgrow", "When HP is below 1/3, Grass-type moves do 1.5x damage", AbilityType::Overgrow};
        case 66: return {66, "Blaze", "blaze", "When HP is below 1/3, Fire-type moves do 1.5x damage", AbilityType::Blaze};
        case 67: return {67, "Torrent", "torrent", "When HP is below 1/3, Water-type moves do 1.5x damage", AbilityType::Torrent};
        case 72: return {72, "Vital Spirit", "vital-spirit", "Prevents the Pokemon from falling asleep.", AbilityType::VitalSpirit};
        case 73: return {73, "White Smoke", "white-smoke", "Prevents other Pokemon from lowering stats.", AbilityType::WhiteSmoke};
        case 70: return {70, "Drought", "drought", "Summons harsh sunlight in battle.", AbilityType::Drought};
        case 91: return {91, "Adaptability", "adaptability", "Powers up moves of the same type as the Pokemon.", AbilityType::Adaptability};
        case 78: return {78, "Motor Drive", "motor-drive", "Boosts Speed if hit by an Electric-type move.", AbilityType::MotorDrive};
        case 101: return {101, "Technician", "technician", "Powers up moves with low base power.", AbilityType::Technician};
        case 111: return {111, "Filter", "filter", "Reduces damage from supereffective attacks.", AbilityType::Filter};
        case 114: return {114, "Storm Drain", "storm-drain", "Draws in all Water-type moves and boosts Sp. Atk.", AbilityType::StormDrain};
        case 116: return {116, "Solid Rock", "solid-rock", "Reduces damage from supereffective attacks.", AbilityType::SolidRock};
        case 128: return {128, "Defiant", "defiant", "Raises Attack sharply when stats are lowered.", AbilityType::Defiant};
        case 125: return {125, "Sheer Force", "sheer-force", "Removes additional effects to increase move power.", AbilityType::SheerForce};
        case 127: return {127, "Unnerve", "unnerve", "Makes the opposing team too nervous to eat Berries.", AbilityType::Unnerve};
        case 136: return {136, "Multiscale", "multiscale", "Reduces damage when at full HP", AbilityType::Multiscale};
        case 153: return {153, "Moxie", "moxie", "Boosts Attack after knocking out any Pokemon.", AbilityType::Moxie};
        case 157: return {157, "Sap Sipper", "sap-sipper", "Boosts Attack if hit by a Grass-type move.", AbilityType::SapSipper};
        case 160: return {160, "Iron Barbs", "iron-barbs", "Damages attackers on contact.", AbilityType::IronBarbs};
        case 158: return {158, "Prankster", "prankster", "Gives priority to a status move.", AbilityType::Prankster};
        case 172: return {172, "Competitive", "competitive", "Raises Sp. Atk sharply when stats are lowered.", AbilityType::Competitive};
        case 168: return {168, "Protean", "protean", "Changes the Pokemon's type to the type of the move it's about to use.", AbilityType::Protean};
        case 226: return {226, "Electric Surge", "electric-surge", "Turns the ground into Electric Terrain when entering battle.", AbilityType::ElectricSurge};
        case 227: return {227, "Psychic Surge", "psychic-surge", "Turns the ground into Psychic Terrain when entering battle.", AbilityType::PsychicSurge};
        case 228: return {228, "Misty Surge", "misty-surge", "Turns the ground into Misty Terrain when entering battle.", AbilityType::MistySurge};
        case 229: return {229, "Grassy Surge", "grassy-surge", "Turns the ground into Grassy Terrain when entering battle.", AbilityType::GrassySurge};
        case 240: return {240, "Mirror Armor", "mirror-armor", "Bounces back only stat-lowering effects.", AbilityType::MirrorArmor};
        case 284: return {284, "Beads of Ruin", "beads-of-ruin", "Lowers the Sp. Def stat of all other Pokemon.", AbilityType::BeadsOfRuin};
        case 285: return {285, "Sword of Ruin", "sword-of-ruin", "Lowers the Def stat of all other Pokemon.", AbilityType::SwordOfRuin};
        case 286: return {286, "Tablets of Ruin", "tablets-of-ruin", "Lowers the Atk stat of all other Pokemon.", AbilityType::TabletsOfRuin};
        case 287: return {287, "Vessel of Ruin", "vessel-of-ruin", "Lowers the Sp. Atk stat of all other Pokemon.", AbilityType::VesselOfRuin};
        case 151: return {151, "Infiltrator", "infiltrator", "Passes through substitute and screens.", AbilityType::Infiltrator};
        case 236: return {236, "Libero", "libero", "Changes the Pokemon's type to the type of the move it's about to use.", AbilityType::Libero};
        case 289: return {289, "Hadron Engine", "hadron-engine", "Creates Electric Terrain and boosts the Pokemon's future paradox power.", AbilityType::HadronEngine};
        case 292: return {292, "Sharpness", "sharpness", "Powers up slicing moves.", AbilityType::Sharpness};
        case 297: return {297, "Earth Eater", "earth-eater", "Restores HP if hit by a Ground-type move.", AbilityType::EarthEater};
        case 272: return {272, "Purifying Salt", "purifying-salt", "Protects from status conditions and weakens Ghost-type attacks.", AbilityType::PurifyingSalt};
        case 273: return {273, "Well-Baked Body", "well-baked-body", "Immune to Fire-type moves and sharply raises Defense when hit.", AbilityType::WellBakedBody};
        case 274: return {274, "Wind Rider", "wind-rider", "Immune to wind moves and boosts Attack when triggered.", AbilityType::WindRider};
        case 295: return {295, "Toxic Debris", "toxic-debris", "Scatters Toxic Spikes when hit by physical moves.", AbilityType::ToxicDebris};
        case 268: return {268, "Lingering Aroma", "lingering-aroma", "On contact, the attacker's Ability becomes Lingering Aroma.", AbilityType::LingeringAroma};
        case 296: return {296, "Armor Tail", "armor-tail", "Blocks opposing priority moves that target this Pokemon.", AbilityType::ArmorTail};
        case 283: return {283, "Good as Gold", "good-as-gold", "Makes the Pokemon immune to status moves used by other Pokemon.", AbilityType::GoodAsGold};
        case 170: return {170, "Stakeout", "stakeout", "Doubles damage dealt to a target that switched in this turn.", AbilityType::Stakeout};
        case 291: return {291, "Cud Chew", "cud-chew", "Chews up a Berry again at the end of the next turn after it is eaten.", AbilityType::CudChew};
        case 104: return {104, "Mold Breaker", "mold-breaker", "Moves can be used regardless of the target's Abilities.", AbilityType::MoldBreaker};
        case 106: return {106, "Aftermath", "aftermath", "Damages the attacker if it makes contact and causes a knockout.", AbilityType::Aftermath};
        case 152: return {152, "Mummy", "mummy", "Contact with the Pokemon changes the attacker's Ability to Mummy.", AbilityType::Mummy};
        case 281: return {281, "Protosynthesis", "protosynthesis", "Boosts the Pokemon's most proficient stat in harsh sunlight.", AbilityType::Protosynthesis};
        case 282: return {282, "Quark Drive", "quark-drive", "Boosts the Pokemon's most proficient stat on Electric Terrain.", AbilityType::QuarkDrive};
        case 293: return {293, "Supreme Overlord", "supreme-overlord", "Powers up attacks for each fainted ally.", AbilityType::SupremeOverlord};
        case 117: return {117, "Snow Warning", "snow-warning", "Summons a snowstorm in battle.", AbilityType::SnowWarning};
        case 1002: return {1002, "Drizzle", "drizzle", "Summons rain in battle.", AbilityType::Drizzle};
        case 30: return {30, "Natural Cure", "natural-cure", "All status conditions heal when switching out.", AbilityType::NaturalCure};
        case 98: return {98, "Magic Guard", "magic-guard", "Only takes damage from attacks.", AbilityType::MagicGuard};
        case 109: return {109, "Unaware", "unaware", "Ignores other Pokemon's stat changes when attacking or defending.", AbilityType::Unaware};
        case 144: return {144, "Regenerator", "regenerator", "Restores a little HP when withdrawn from battle.", AbilityType::Regenerator};
        default: return {0, "None", "none", "", AbilityType::None};
    }
}

std::string resolveReadablePath(const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        std::ifstream f(path);
        if (f.is_open()) {
            return path;
        }
    }
    return paths.front();
}

json loadAbilityRoot() {
    const std::string path = resolveReadablePath(kAbilityJsonPaths);
    std::ifstream file(path);
    if (!file.is_open()) {
        return json{{"abilities", json::array()}};
    }
    json root;
    file >> root;
    if (!root.contains("abilities") || !root["abilities"].is_array()) {
        root["abilities"] = json::array();
    }
    return root;
}

AbilityData parseAbilityEntry(const json& entry) {
    AbilityData data;
    data.id = entry.value("id", 0);
    data.name = entry.value("name", "");
    data.apiName = entry.value("apiName", "");
    data.description = entry.value("description", "");

    if (data.apiName.empty() && !data.name.empty()) {
        std::string api;
        api.reserve(data.name.size());
        for (char c : data.name) {
            api.push_back(c == ' ' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        data.apiName = api;
    }
    if (data.name.empty() && !data.apiName.empty()) {
        data.name = slugToDisplayName(data.apiName);
    }

    data.type = parseAbilityType(entry.value("mappedType", data.apiName));
    if (data.type == AbilityType::None) {
        data.type = parseAbilityType(data.name);
    }

    if (data.type == AbilityType::None) {
        AbilityData legacy = fallbackAbilityDataById(data.id);
        if (legacy.id != 0) {
            data.type = legacy.type;
        }
    }

    return data;
}

size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total = size * nmemb;
    std::string* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

bool fetchAbilityPayload(int id, json& outPayload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string response;
    const std::string url = "https://pokeapi.co/api/v2/ability/" + std::to_string(id);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PokemonSimulator/1.0");

    CURLcode res = curl_easy_perform(curl);
    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || statusCode != 200) {
        return false;
    }

    outPayload = json::parse(response, nullptr, false);
    return !outPayload.is_discarded();
}

AbilityData abilityDataFromPayload(const json& payload) {
    AbilityData data;
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

    data.type = parseAbilityType(data.apiName);
    if (data.type == AbilityType::None) {
        AbilityData fallback = fallbackAbilityDataById(data.id);
        if (fallback.id != 0) {
            data.type = fallback.type;
            data.name = fallback.name;
            if (data.description.empty()) {
                data.description = fallback.description;
            }
        }
    }

    return data;
}

bool writeAbilityRootAtomically(const std::string& path, const json& root) {
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

std::string resolveWritableAbilityPath() {
    for (const std::string& path : kAbilityJsonPaths) {
        std::ifstream f(path);
        if (f.is_open()) {
            return path;
        }
    }
    return kAbilityJsonPaths.front();
}

bool upsertAbilityEntry(const AbilityData& data, const json& payload) {
    if (data.id <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(gAbilityCacheMutex);
    json root = loadAbilityRoot();
    json entry;
    entry["id"] = data.id;
    entry["name"] = data.name;
    entry["apiName"] = data.apiName;
    entry["description"] = data.description;
    entry["mappedType"] = [data]() {
        switch (data.type) {
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
            case AbilityType::Insomnia: return "insomnia";
            case AbilityType::VitalSpirit: return "vital-spirit";
            case AbilityType::Guts: return "guts";
            case AbilityType::HugePower: return "huge-power";
            case AbilityType::ThickFat: return "thick-fat";
            case AbilityType::MarvelScale: return "marvel-scale";
            case AbilityType::SapSipper: return "sap-sipper";
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
            case AbilityType::Protosynthesis: return "protosynthesis";
            case AbilityType::QuarkDrive: return "quark-drive";
            case AbilityType::SupremeOverlord: return "supreme-overlord";
            default: return "none";
        }
    }();

    if (payload.contains("effect_entries")) {
        entry["effect_entries"] = payload["effect_entries"];
    }
    if (payload.contains("pokemon")) {
        entry["pokemon_count"] = static_cast<int>(payload["pokemon"].size());
    }

    bool replaced = false;
    for (auto& existing : root["abilities"]) {
        if (existing.value("id", -1) == data.id) {
            existing = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        root["abilities"].push_back(entry);
    }

    std::sort(root["abilities"].begin(), root["abilities"].end(), [](const json& a, const json& b) {
        return a.value("id", 0) < b.value("id", 0);
    });

    return writeAbilityRootAtomically(resolveWritableAbilityPath(), root);
}

std::set<int> collectReferencedAbilityIds() {
    std::set<int> ids;

    for (const std::string& path : kSpeciesJsonPaths) {
        std::ifstream f(path);
        if (!f.is_open()) {
            continue;
        }
        json root;
        f >> root;
        if (root.contains("species") && root["species"].is_array()) {
            for (const auto& spec : root["species"]) {
                if (!spec.contains("abilities") || !spec["abilities"].is_array()) {
                    continue;
                }
                for (const auto& aid : spec["abilities"]) {
                    if (aid.is_number_integer()) {
                        ids.insert(aid.get<int>());
                    }
                }
            }
        }
        break;
    }

    // 对仅字符串能力名的队伍文件，先映射到本地能力类型，再映射回 canonical id。
    for (const std::string& rootPath : kPokemonJsonRoots) {
        std::error_code ec;
        if (!std::filesystem::exists(rootPath, ec)) {
            continue;
        }
        for (const auto& file : std::filesystem::directory_iterator(rootPath, ec)) {
            if (ec || !file.is_regular_file() || file.path().extension() != ".json") {
                continue;
            }
            std::ifstream in(file.path());
            if (!in.is_open()) {
                continue;
            }
            json obj;
            in >> obj;
            if (!obj.contains("ability") || !obj["ability"].is_string()) {
                continue;
            }
            AbilityData ability = getAbilityDataByName(obj["ability"].get<std::string>());
            if (ability.id > 0) {
                ids.insert(ability.id);
            }
        }
        break;
    }

    return ids;
}
}  // namespace

AbilityData getAbilityDataById(int id) {
    if (id <= 0) {
        return fallbackAbilityDataById(0);
    }

    json root = loadAbilityRoot();
    for (const auto& entry : root["abilities"]) {
        if (entry.value("id", -1) != id) {
            continue;
        }
        AbilityData data = parseAbilityEntry(entry);
        if (data.id == 0) {
            data.id = id;
        }
        if (data.type == AbilityType::None) {
            AbilityData fallback = fallbackAbilityDataById(id);
            if (fallback.id != 0) {
                return fallback;
            }
        }
        return data;
    }

    AbilityData fallback = fallbackAbilityDataById(id);
    if (fallback.id != 0) {
        return fallback;
    }
    return {0, "None", "none", "", AbilityType::None};
}

AbilityData getAbilityDataByName(const std::string& name) {
    const std::string normalized = normalizeName(name);
    json root = loadAbilityRoot();
    for (const auto& entry : root["abilities"]) {
        AbilityData data = parseAbilityEntry(entry);
        if (normalizeName(data.name) == normalized || normalizeName(data.apiName) == normalized) {
            return data;
        }
    }

    // 兜底：遍历常见已实现能力 ID。
    const int fallbackIds[] = {1, 2, 3, 4, 5, 9, 10, 11, 13, 15, 17, 18, 22, 26, 29, 30, 37, 39, 45, 47, 51, 52, 62, 63, 65, 66, 67, 70, 72, 73, 78, 91, 98, 101, 109, 111, 114, 116, 117, 125, 127, 128, 136, 144, 151, 153, 157, 158, 168, 170, 172, 226, 227, 228, 229, 236, 240, 268, 272, 273, 274, 281, 282, 283, 284, 285, 286, 287, 289, 292, 293, 295, 296, 297, 1002};
    for (int id : fallbackIds) {
        AbilityData fallback = fallbackAbilityDataById(id);
        if (fallback.id > 0 && (normalizeName(fallback.name) == normalized || normalizeName(fallback.apiName) == normalized)) {
            return fallback;
        }
    }

    return {0, "None", "none", "", AbilityType::None};
}

AbilityType getAbilityTypeById(int id) {
    return getAbilityDataById(id).type;
}

AbilityType getAbilityTypeByNameFromData(const std::string& name) {
    return getAbilityDataByName(name).type;
}

AbilityData getAbilityData(AbilityType type) {
    json root = loadAbilityRoot();
    for (const auto& entry : root["abilities"]) {
        AbilityData data = parseAbilityEntry(entry);
        if (data.type == type && data.id > 0) {
            return data;
        }
    }

    switch (type) {
        case AbilityType::Blaze: return fallbackAbilityDataById(66);
        case AbilityType::Torrent: return fallbackAbilityDataById(67);
        case AbilityType::Overgrow: return fallbackAbilityDataById(65);
        case AbilityType::Intimidate: return fallbackAbilityDataById(22);
        case AbilityType::Multiscale: return fallbackAbilityDataById(136);
        case AbilityType::Levitate: return fallbackAbilityDataById(26);
        case AbilityType::WaterAbsorb: return fallbackAbilityDataById(11);
        case AbilityType::VoltAbsorb: return fallbackAbilityDataById(10);
        case AbilityType::FlashFire: return fallbackAbilityDataById(18);
        case AbilityType::Static: return fallbackAbilityDataById(9);
        case AbilityType::Insomnia: return fallbackAbilityDataById(15);
        case AbilityType::VitalSpirit: return fallbackAbilityDataById(72);
        case AbilityType::Guts: return fallbackAbilityDataById(62);
        case AbilityType::HugePower: return fallbackAbilityDataById(37);
        case AbilityType::ThickFat: return fallbackAbilityDataById(47);
        case AbilityType::MarvelScale: return fallbackAbilityDataById(63);
        case AbilityType::SapSipper: return fallbackAbilityDataById(157);
        case AbilityType::StormDrain: return fallbackAbilityDataById(114);
        case AbilityType::MotorDrive: return fallbackAbilityDataById(78);
        case AbilityType::Immunity: return fallbackAbilityDataById(17);
        case AbilityType::Technician: return fallbackAbilityDataById(101);
        case AbilityType::Filter: return fallbackAbilityDataById(111);
        case AbilityType::SolidRock: return fallbackAbilityDataById(116);
        case AbilityType::Moxie: return fallbackAbilityDataById(153);
        case AbilityType::InnerFocus: return fallbackAbilityDataById(39);
        case AbilityType::Regenerator: return fallbackAbilityDataById(144);
        case AbilityType::NaturalCure: return fallbackAbilityDataById(30);
        case AbilityType::MagicGuard: return fallbackAbilityDataById(98);
        case AbilityType::Unaware: return fallbackAbilityDataById(109);
        case AbilityType::Prankster: return fallbackAbilityDataById(158);
        case AbilityType::ClearBody: return fallbackAbilityDataById(29);
        case AbilityType::Defiant: return fallbackAbilityDataById(128);
        case AbilityType::Competitive: return fallbackAbilityDataById(172);
        case AbilityType::WhiteSmoke: return fallbackAbilityDataById(73);
        case AbilityType::MirrorArmor: return fallbackAbilityDataById(240);
        case AbilityType::HyperCutter: return fallbackAbilityDataById(52);
        case AbilityType::KeenEye: return fallbackAbilityDataById(51);
        case AbilityType::Drizzle: return fallbackAbilityDataById(1002);
        case AbilityType::Drought: return fallbackAbilityDataById(70);
        case AbilityType::SandStream: return fallbackAbilityDataById(45);
        case AbilityType::SnowWarning: return fallbackAbilityDataById(117);
        case AbilityType::CloudNine: return fallbackAbilityDataById(13);
        case AbilityType::GrassySurge: return fallbackAbilityDataById(229);
        case AbilityType::ElectricSurge: return fallbackAbilityDataById(226);
        case AbilityType::PsychicSurge: return fallbackAbilityDataById(227);
        case AbilityType::MistySurge: return fallbackAbilityDataById(228);
        case AbilityType::HadronEngine: return fallbackAbilityDataById(289);
        case AbilityType::Protean: return fallbackAbilityDataById(168);
        case AbilityType::Libero: return fallbackAbilityDataById(236);
        case AbilityType::Adaptability: return fallbackAbilityDataById(91);
        case AbilityType::SheerForce: return fallbackAbilityDataById(125);
        case AbilityType::Infiltrator: return fallbackAbilityDataById(151);
        case AbilityType::BeadsOfRuin: return fallbackAbilityDataById(284);
        case AbilityType::SwordOfRuin: return fallbackAbilityDataById(285);
        case AbilityType::TabletsOfRuin: return fallbackAbilityDataById(286);
        case AbilityType::VesselOfRuin: return fallbackAbilityDataById(287);
        case AbilityType::Unnerve: return fallbackAbilityDataById(127);
        case AbilityType::Sharpness: return fallbackAbilityDataById(292);
        case AbilityType::EarthEater: return fallbackAbilityDataById(297);
        case AbilityType::PurifyingSalt: return fallbackAbilityDataById(272);
        case AbilityType::WellBakedBody: return fallbackAbilityDataById(273);
        case AbilityType::WindRider: return fallbackAbilityDataById(274);
        case AbilityType::ToxicDebris: return fallbackAbilityDataById(295);
        case AbilityType::LingeringAroma: return fallbackAbilityDataById(268);
        case AbilityType::ArmorTail: return fallbackAbilityDataById(296);
        case AbilityType::GoodAsGold: return fallbackAbilityDataById(283);
        case AbilityType::Stakeout: return fallbackAbilityDataById(170);
        case AbilityType::Protosynthesis: return fallbackAbilityDataById(281);
        case AbilityType::QuarkDrive: return fallbackAbilityDataById(282);
        case AbilityType::SupremeOverlord: return fallbackAbilityDataById(293);
        default: return {0, "None", "none", "", AbilityType::None};
    }
}

bool prefetchAbilitiesFromPokeAPI(bool refreshExisting) {
    const std::set<int> abilityIds = collectReferencedAbilityIds();
    if (abilityIds.empty()) {
        return false;
    }

    bool success = true;
    for (int id : abilityIds) {
        AbilityData local = getAbilityDataById(id);
        const bool hasLocal = local.id == id && !local.name.empty() && local.name != "None";
        if (!refreshExisting && hasLocal) {
            continue;
        }

        json payload;
        if (!fetchAbilityPayload(id, payload)) {
            success = false;
            continue;
        }

        AbilityData fetched = abilityDataFromPayload(payload);
        if (fetched.id != id) {
            success = false;
            continue;
        }

        if (!upsertAbilityEntry(fetched, payload)) {
            success = false;
            continue;
        }
    }

    return success;
}