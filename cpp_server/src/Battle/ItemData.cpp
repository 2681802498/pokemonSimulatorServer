#include "Battle/Items.h"

#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
std::mutex gItemCacheMutex;

const std::vector<std::string> kItemJsonPaths = {
    "data/items.json",
    "../data/items.json",
    "../../data/items.json"
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

std::string normalizeName(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == ' ' || ch == '-' || ch == '_' || ch == '\'') {
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

std::string toSlug(const std::string& displayName) {
    std::string s;
    s.reserve(displayName.size());
    for (char ch : displayName) {
        if (ch == ' ') {
            s.push_back('-');
        } else {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return s;
}

std::string itemTypeToMapped(ItemType type) {
    return toSlug(getItemName(type));
}

ItemType parseMappedItemType(const std::string& mapped) {
    const std::string key = normalizeName(mapped);

    // Normalize common held-item aliases to already implemented item families.
    static const std::unordered_map<std::string, ItemType> kAliasMap = {
        {"oddincense", ItemType::TwistedSpoon},
        {"rockincense", ItemType::HardStone},
        {"waveincense", ItemType::MysticWater},
        {"roseincense", ItemType::MiracleSeed},

        {"dracoplate", ItemType::DragonFang},
        {"dreadplate", ItemType::BlackGlasses},

        {"firegem", ItemType::Charcoal},
        {"watergem", ItemType::MysticWater},
        {"electricgem", ItemType::Magnet},
        {"grassgem", ItemType::MiracleSeed},
        {"icegem", ItemType::NeverMeltIce},
        {"fightinggem", ItemType::BlackBelt},
        {"poisongem", ItemType::PoisonBarb},
        {"groundgem", ItemType::EarthPlate},
        {"flyinggem", ItemType::SharpBeak},
        {"psychicgem", ItemType::TwistedSpoon},
        {"buggem", ItemType::SilverPowder},
        {"rockgem", ItemType::HardStone},
        {"ghostgem", ItemType::SpellTag},
        {"darkgem", ItemType::BlackGlasses},
        {"steelgem", ItemType::MetalCoat},
        {"dragongem", ItemType::DragonFang},
        {"normalgem", ItemType::SilkScarf}
    };

    auto aliasIt = kAliasMap.find(key);
    if (aliasIt != kAliasMap.end()) {
        return aliasIt->second;
    }

    // Dynamax crystals encode species id in a gem-like suffix and are not held battle items here.
    if (key.rfind("dynamaxcrystalgem", 0) == 0) {
        return ItemType::None;
    }

    for (int i = 0; i < static_cast<int>(ItemType::Count); ++i) {
        ItemType t = static_cast<ItemType>(i);
        if (normalizeName(getItemName(t)) == key) {
            return t;
        }
    }
    return ItemType::None;
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

std::string resolveWritablePath(const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        std::ifstream f(path);
        if (f.is_open()) {
            return path;
        }
    }
    return paths.front();
}

json loadItemRoot() {
    const std::string path = resolveReadablePath(kItemJsonPaths);
    std::ifstream file(path);
    if (!file.is_open()) {
        return json{{"items", json::array()}};
    }
    json root;
    file >> root;
    if (!root.contains("items") || !root["items"].is_array()) {
        root["items"] = json::array();
    }
    return root;
}

ItemData parseItemEntry(const json& entry) {
    ItemData data;
    data.id = entry.value("id", 0);
    data.name = entry.value("name", "");
    data.apiName = entry.value("apiName", "");
    data.description = entry.value("description", "");
    data.isBattle = entry.value("isBattle", true);

    if (data.apiName.empty() && !data.name.empty()) {
        data.apiName = toSlug(data.name);
    }
    if (data.name.empty() && !data.apiName.empty()) {
        data.name = slugToDisplayName(data.apiName);
    }

    data.mappedType = parseMappedItemType(entry.value("mappedType", data.name));
    if (data.mappedType == ItemType::None) {
        data.mappedType = parseMappedItemType(data.apiName);
    }
    return data;
}

size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total = size * nmemb;
    std::string* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

bool fetchItemPayload(int id, json& outPayload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string response;
    const std::string url = "https://pokeapi.co/api/v2/item/" + std::to_string(id);
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

ItemData itemDataFromPayload(const json& payload) {
    ItemData data;
    data.id = payload.value("id", 0);
    data.apiName = payload.value("name", "");
    data.name = slugToDisplayName(data.apiName);
    data.description = "";
    data.isBattle = false;
    data.mappedType = parseMappedItemType(data.apiName);

    if (payload.contains("effect_entries") && payload["effect_entries"].is_array()) {
        for (const auto& entry : payload["effect_entries"]) {
            if (entry.value("language", json::object()).value("name", "") == "en") {
                data.description = entry.value("short_effect", entry.value("effect", ""));
                break;
            }
        }
    }

    if (payload.contains("attributes") && payload["attributes"].is_array()) {
        for (const auto& attr : payload["attributes"]) {
            const std::string attrName = toLower(attr.value("name", ""));
            if (attrName.find("holdable") != std::string::npos || attrName.find("usable-in-battle") != std::string::npos) {
                data.isBattle = true;
                break;
            }
        }
    }

    return data;
}

bool writeItemRootAtomically(const std::string& path, const json& root) {
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

bool upsertItemEntry(const ItemData& data, const json& payload) {
    if (data.id <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(gItemCacheMutex);
    json root = loadItemRoot();

    json entry;
    entry["id"] = data.id;
    entry["name"] = data.name;
    entry["apiName"] = data.apiName;
    entry["description"] = data.description;
    entry["isBattle"] = data.isBattle;
    entry["mappedType"] = itemTypeToMapped(data.mappedType);

    if (payload.contains("category")) {
        entry["category"] = payload["category"].value("name", "");
    }
    if (payload.contains("attributes")) {
        entry["attributes"] = payload["attributes"];
    }

    bool replaced = false;
    for (auto& existing : root["items"]) {
        if (existing.value("id", -1) == data.id) {
            existing = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        root["items"].push_back(entry);
    }

    std::sort(root["items"].begin(), root["items"].end(), [](const json& a, const json& b) {
        return a.value("id", 0) < b.value("id", 0);
    });

    return writeItemRootAtomically(resolveWritablePath(kItemJsonPaths), root);
}

std::set<int> collectReferencedItemIds() {
    std::set<int> ids;

    // 从本地对战队伍 JSON 的 item 名反查 id。
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
            if (!obj.contains("item") || !obj["item"].is_string()) {
                continue;
            }
            ItemData item = getItemDataByName(obj["item"].get<std::string>());
            if (item.id > 0) {
                ids.insert(item.id);
            }
        }
        break;
    }

    if (!ids.empty()) {
        return ids;
    }

    // 若无引用，保底拉取前 400 个 item（覆盖主线常见道具）。
    for (int i = 1; i <= 400; ++i) {
        ids.insert(i);
    }
    return ids;
}
}  // namespace

ItemData getItemDataById(int id) {
    if (id <= 0) {
        return {0, "None", "none", "", false, ItemType::None};
    }

    json root = loadItemRoot();
    for (const auto& entry : root["items"]) {
        if (entry.value("id", -1) != id) {
            continue;
        }
        ItemData data = parseItemEntry(entry);
        if (data.id == 0) {
            data.id = id;
        }
        return data;
    }

    return {0, "None", "none", "", false, ItemType::None};
}

ItemData getItemDataByName(const std::string& name) {
    const std::string key = normalizeName(name);
    json root = loadItemRoot();
    for (const auto& entry : root["items"]) {
        ItemData data = parseItemEntry(entry);
        if (normalizeName(data.name) == key || normalizeName(data.apiName) == key) {
            return data;
        }
    }

    // 尝试通过已实现 ItemType 名称匹配。
    ItemType t = parseMappedItemType(name);
    if (t != ItemType::None) {
        return {0, getItemName(t), toSlug(getItemName(t)), "", true, t};
    }

    return {0, "None", "none", "", false, ItemType::None};
}

ItemType getItemTypeById(int id) {
    return getItemDataById(id).mappedType;
}

ItemType getItemTypeByName(const std::string& name) {
    return getItemDataByName(name).mappedType;
}

Item createItemFromData(const ItemData& data) {
    if (data.mappedType != ItemType::None) {
        return getItem(data.mappedType);
    }
    Item item(ItemType::None, data.name.empty() ? "None" : data.name);
    return item;
}

Item createItemById(int id) {
    return createItemFromData(getItemDataById(id));
}

Item createItemByName(const std::string& name) {
    return createItemFromData(getItemDataByName(name));
}

bool prefetchItemsFromPokeAPI(bool refreshExisting) {
    const std::set<int> itemIds = collectReferencedItemIds();
    if (itemIds.empty()) {
        return false;
    }

    bool success = true;
    for (int id : itemIds) {
        ItemData local = getItemDataById(id);
        const bool hasLocal = local.id == id && !local.name.empty() && local.name != "None";
        if (!refreshExisting && hasLocal) {
            continue;
        }

        json payload;
        if (!fetchItemPayload(id, payload)) {
            success = false;
            continue;
        }

        ItemData fetched = itemDataFromPayload(payload);
        if (fetched.id != id) {
            success = false;
            continue;
        }

        if (!upsertItemEntry(fetched, payload)) {
            success = false;
            continue;
        }
    }

    return success;
}
