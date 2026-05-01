#include "Battle/MoveRuleRegistry.h"

#include "Battle/Moves.h"

#include <cctype>

std::string MoveRuleRegistry::normalizeMoveName(const std::string& moveName) {
    std::string normalized;
    normalized.reserve(moveName.size());
    for (char ch : moveName) {
        if (ch == ' ' || ch == '-' || ch == '\'' || ch == '_') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

void MoveRuleRegistry::registerRule(const std::string& moveName, MoveRuleHandler handler) {
    if (!handler) {
        return;
    }
    rules[normalizeMoveName(moveName)] = std::move(handler);
}

bool MoveRuleRegistry::apply(Battle& battle, Pokemon* attacker, Pokemon* defender, const Move& move) const {
    const auto it = rules.find(normalizeMoveName(move.getName()));
    if (it == rules.end()) {
        return false;
    }
    return it->second(battle, attacker, defender, move);
}
