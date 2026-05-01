#include "Battle/MoveTestRunner.h"

#include "Battle/Moves.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

namespace {
const std::set<std::string> kImplementedStatusNameRules = {
    "batonpass",
    "encore",
    "flash",
    "haze",
    "leechseed",
    "lightscreen",
    "milkdrink",
    "reflect",
    "sandattack",
    "recover",
    "smokescreen",
    "substitute",
    "softboiled",
    "trickroom",
};

std::string toLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string normalizeName(std::string value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return normalized;
}

std::string trim(const std::string& input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

bool isNonEmptyStringField(const nlohmann::json& entry, const char* key) {
    if (!entry.contains(key) || !entry[key].is_string()) {
        return false;
    }
    return !trim(entry[key].get<std::string>()).empty();
}

std::string chooseReadableMovesPath() {
    const std::vector<std::string> candidates = {
        "data/moves.json",
        "../data/moves.json",
        "../../data/moves.json"
    };

    for (const std::string& path : candidates) {
        std::ifstream file(path);
        if (file.is_open()) {
            return path;
        }
    }

    return "";
}

bool hasBaselineFieldIssues(const nlohmann::json& entry, std::string& reason) {
    if (!entry.contains("id") || !entry["id"].is_number_integer()) {
        reason = "id is missing or not integer";
        return true;
    }

    const int id = entry["id"].get<int>();
    if (id <= 0) {
        reason = "id must be > 0";
        return true;
    }

    if (!isNonEmptyStringField(entry, "name")) {
        reason = "name is missing or empty";
        return true;
    }

    if (!isNonEmptyStringField(entry, "apiName")) {
        reason = "apiName is missing or empty";
        return true;
    }

    if (!isNonEmptyStringField(entry, "type")) {
        reason = "type is missing or empty";
        return true;
    }

    if (!isNonEmptyStringField(entry, "category")) {
        reason = "category is missing or empty";
        return true;
    }

    if (!entry.contains("accuracy") || !entry["accuracy"].is_number_integer()) {
        reason = "accuracy is missing or not integer";
        return true;
    }

    const int accuracy = entry["accuracy"].get<int>();
    if (accuracy < 0 || accuracy > 100) {
        reason = "accuracy out of range [0,100]";
        return true;
    }

    if (!entry.contains("pp") || !entry["pp"].is_number_integer()) {
        reason = "pp is missing or not integer";
        return true;
    }

    const int pp = entry["pp"].get<int>();
    if (pp < 0) {
        reason = "pp must be >= 0";
        return true;
    }

    return false;
}
}  // namespace

MoveTestSummary runAllMoveTests(std::ostream& out, std::ostream& err) {
    MoveTestSummary summary;

    const std::string movesPath = chooseReadableMovesPath();
    if (movesPath.empty()) {
        err << "Unable to locate moves.json (tried data/, ../data/, ../../data/)" << std::endl;
        summary.failed = 1;
        summary.failedMoveNames.push_back("<moves.json-not-found>");
        return summary;
    }

    std::ifstream file(movesPath);
    nlohmann::json root;
    file >> root;

    if (!root.contains("moves") || !root["moves"].is_array()) {
        err << "Invalid moves.json format: root.moves must be an array" << std::endl;
        summary.failed = 1;
        summary.failedMoveNames.push_back("<invalid-moves-json-format>");
        return summary;
    }

    out << "=== Move tests (independent from item tests) ===" << std::endl;
    out << "Using moves file: " << movesPath << std::endl;

    std::set<std::string> failedNames;
    std::set<std::string> unsupportedStatusNames;

    summary.jsonMoveCount = static_cast<int>(root["moves"].size());
    summary.total = summary.jsonMoveCount;

    for (const auto& entry : root["moves"]) {
        std::string failureReason;
        std::string moveName = entry.value("name", std::string("<unknown>"));

        if (hasBaselineFieldIssues(entry, failureReason)) {
            failedNames.insert(moveName + " (baseline: " + failureReason + ")");
            continue;
        }

        const int id = entry["id"].get<int>();
        const MoveData parsed = getMoveDataById(id);
        if (parsed.id != id || trim(parsed.name).empty()) {
            failedNames.insert(moveName + " (parse: getMoveDataById mismatch)");
            continue;
        }

        const std::string category = toLower(trim(entry.value("category", "")));
        if (category == "status") {
            summary.statusMoveCount++;
            const std::string effect = toLower(trim(entry.value("effect", "none")));
            const std::string normalizedName = normalizeName(moveName);
            if (effect == "none" && kImplementedStatusNameRules.find(normalizedName) == kImplementedStatusNameRules.end()) {
                unsupportedStatusNames.insert(moveName);
            }
        }
    }

    summary.failedMoveNames.assign(failedNames.begin(), failedNames.end());
    summary.unsupportedStatusMoveNames.assign(unsupportedStatusNames.begin(), unsupportedStatusNames.end());

    summary.failed = static_cast<int>(summary.failedMoveNames.size());
    summary.passed = summary.total - summary.failed;

    summary.unsupportedStatusMoveCount = static_cast<int>(summary.unsupportedStatusMoveNames.size());
    summary.implementedStatusMoveCount = summary.statusMoveCount - summary.unsupportedStatusMoveCount;
    if (summary.implementedStatusMoveCount < 0) {
        summary.implementedStatusMoveCount = 0;
    }

    out << "Total moves: " << summary.total << std::endl;
    out << "Passed baseline+parse checks: " << summary.passed << std::endl;
    out << "Failed baseline+parse checks: " << summary.failed << std::endl;
    out << "Status moves: " << summary.statusMoveCount << std::endl;
    out << "Implemented status moves: " << summary.implementedStatusMoveCount << std::endl;
    out << "Unsupported status moves: " << summary.unsupportedStatusMoveCount << std::endl;

    return summary;
}
