#pragma once

#include <string>
#include <functional>

class Pokemon; // Forward declaration

enum class Trigger {
    OnEntry,        // 出场时
    OnExit,         // 退场时
    OnTurnStart,    // 回合开始
    OnTurnEnd,      // 回合结束
    OnDamage,       // 受到伤害
    OnDealDamage,   // 造成伤害
    OnAttack,       // 使用攻击
    OnFaint,        // 濒死时
    OnStatusInflicted, // 被施加状态
    OnWeatherChange,    // 天气变化
    OnTerrainChange,    // 场地变化
    Count
};

