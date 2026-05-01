#pragma once

#include "Types.h"
#include <string>

class Battle;

// 技能效果类型枚举
enum class MoveEffect {
    None,           // 无特殊效果
    Pursuit,        // 追击效果
    Encore,         // 再来一次
    Dig,            // 挖洞（两回合）
    Round,          // 轮唱
    KnockOff,       // 拍落
    WeatherBall,    // 气象球
    Status,         // 状态变化
    StatChange,     // 能力变化
    Recoil,         // 反伤
    Drain,          // 吸血
    Flinch,         // 畏缩
    Paralyze,       // 麻痹
    Sleep,          // 睡眠
    Freeze,         // 冰冻
    Burn,           // 烧伤
    Poison,         // 中毒
    Confuse,        // 混乱
    LeechSeed,      // 寄生种子
    Reflect,        // 反射壁
    LightScreen,    // 光墙
    Safeguard,      // 守护
    Tailwind,       // 顺风
    StealthRock,    // 隐形岩
    Spikes,         // 撒菱
    ToxicSpikes     // 毒菱
};

// 能力变化类型枚举
enum class StatChangeType {
    Attack,         // 攻击
    Defense,        // 防御
    SpAttack,       // 特攻
    SpDefense,      // 特防
    Speed,          // 速度
    Accuracy,       // 命中
    Evasion         // 闪避
};

enum class Target{
    Self,
    Ally,
    Opponent,
    AllAllies,
    AllOpponents,
    All
};

struct MoveData {
    int id;
    std::string name;
    std::string apiName;
    std::string description;
    Type type;
    Category category;
    int power;
    int accuracy;
    int pp;
    MoveEffect effect;
    int effectChance;
    int effectParam1;
    int effectParam2;
    int priority;
    Target target;
};

class Move {
private:
    MoveData data;
    int maxPP;

    //MoveData metadata;
public:
    Move();
    explicit Move(const MoveData& data);
    Move(std::string name, Type type, Category category, int power, int accuracy, int pp);
    Move(std::string name, Type type, Category category, int power, int accuracy, int pp, MoveEffect effect, int effectChance, int effectParam1 = 0, int effectParam2 = 0, int priority = 0, Target target = Target::Opponent);
    Move(const Move& other);
    Move& operator=(const Move& other);
    ~Move();
    // Getters
    const MoveData& getData() const { return data; }
    const std::string& getName() const { return data.name; }
    const Type& getType() const { return data.type; }
    const Category& getCategory() const { return data.category; }
    const int& getPower() const { return data.power; }
    const int& getAccuracy() const { return data.accuracy; }
    const int& getPP() const { return data.pp; }
    const int& getMaxPP() const { return maxPP; }
    const MoveEffect& getEffect() const { return data.effect; }
    const int& getEffectChance() const { return data.effectChance; }
    const int& getPriority() const { return data.priority; }
    const int& getEffectParam1() const { return data.effectParam1; }
    const int& getEffectParam2() const { return data.effectParam2; }
    const Target& getTarget() const { return data.target; }

    // Setters
    void setPP(int p) { data.pp = p; if (data.pp < 0) data.pp = 0; if (data.pp > maxPP) data.pp = maxPP; }
    void setEffect(MoveEffect e) { data.effect = e; }
    void setEffectChance(int chance) { data.effectChance = chance; if (data.effectChance < 0) data.effectChance = 0; if (data.effectChance > 100) data.effectChance = 100; }
    void setPriority(int value) { data.priority = value; }
    void setEffectParam1(int param) { data.effectParam1 = param; }
    void setEffectParam2(int param) { data.effectParam2 = param; }
    void setTarget(Target value) { data.target = value; }

    // Utility
    bool canUse() const { return data.pp > 0; }
    void use() { if (data.pp > 0) data.pp--; }
    bool isStatusMove() const { return data.category == Category::Status; }
    bool isPursuit() const { return data.effect == MoveEffect::Pursuit; }
};

MoveData getMoveDataById(int id);
MoveData getMoveDataByName(const std::string& name);
Move createMoveFromData(const MoveData& data);
Move createMoveById(int id);
Move createMoveByName(const std::string& name);

// 预拉取项目中引用到的 move id 并写回 data/moves.json。
bool prefetchMovesFromPokeAPI(bool refreshExisting = false);

// Register the core move rules into a battle instance.
void initializeCoreMoveRules(Battle& battle);

