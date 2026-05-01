#pragma once

#include <functional>
#include <string>
#include <unordered_map>

class Battle;
class Pokemon;
class Move;

class MoveRuleRegistry {
public:
    using MoveRuleHandler = std::function<bool(Battle& battle, Pokemon* attacker, Pokemon* defender, const Move& move)>;

    void registerRule(const std::string& moveName, MoveRuleHandler handler);
    bool apply(Battle& battle, Pokemon* attacker, Pokemon* defender, const Move& move) const;

private:
    static std::string normalizeMoveName(const std::string& moveName);

    std::unordered_map<std::string, MoveRuleHandler> rules;
};
