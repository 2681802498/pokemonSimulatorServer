#pragma once

#include "BattleData.h"  // IWYU pragma: keep
#include "Side.h"
#include "Field.h"
#include "Weather.h"
#include "BattleQueue.h"
#include "EventSystem.h"
#include "BattleActions.h"
#include "MoveRuleRegistry.h"
#include <string>
#include <unordered_map>

class Battle {
    friend void initializeCoreMoveRules(Battle& battle);
public:
    Battle(Side sideA, Side sideB);

    Side& getSideA();
    const Side& getSideA() const;
    Side& getSideB();
    const Side& getSideB() const;
    Side& getOpponentSide(const Side& side);
    Field& getField();
    Weather& getWeather();
    EventSystem& getEventSystem();
    const EventSystem& getEventSystem() const;

    void enqueueAction(const BattleAction& action);
    void processTurn();
    void resolveNextAction();
    void applyStatusEffects();
    void applyWeatherEffects();
    void applyFieldEffects();

    int calculateDamage(Pokemon* attacker, Pokemon* defender, const Move& move) const;
    bool switchPokemon(Side& side, int newIndex);
    
    // 特性相关方法
    void triggerAbility(Pokemon* pokemon, Trigger trigger, Pokemon* opponent = nullptr, void* context = nullptr);
    void triggerAbilities(Trigger trigger, Pokemon* target = nullptr, void* context = nullptr);
    
    // 物品相关方法
    void triggerItemEffect(Pokemon* pokemon, ItemTrigger trigger, Pokemon* opponent, void* context = nullptr);
    void triggerItemEffects(ItemTrigger trigger, Pokemon* target = nullptr);
    
    // 处理技能追加效果
    void processMoveEffects(Pokemon* attacker, Pokemon* defender, const Move& move);
    
    // 数值修改方法
    float applyStatModifiers(Pokemon* pokemon, float baseValue, const std::string& statName) const;
    float applyDamageModifiers(Pokemon* attacker, Pokemon* defender, const Move& move, float baseModifier) const;
    
    // 免疫检查方法
    bool isImmuneToStatus(Pokemon* pokemon, StatusType status) const;
    bool isImmuneToType(Pokemon* pokemon, int typeId, bool& shouldHeal, int& healPercent) const;
    bool isImmuneToWeather(Pokemon* pokemon, const Weather& weather) const;
    bool checkTypeImmunity(Pokemon* defender, int typeId, Pokemon* attacker);
    
    // 获取当前回合数
    int getTurnNumber() const { return turnNumber; }
    
    // 获取对手的宝可梦（辅助方法）
    Pokemon* getOpponentPokemon(Pokemon* self) const;
    static Side* findSideForPokemon(Battle& battle, Pokemon* pokemon);
    static const Side* findSideForPokemon(const Battle& battle, Pokemon* pokemon);
    bool canBeForcedToSwitch(Pokemon* defender) const;
public:
    enum class SemiInvulnerableState {
        None,
        Underground,
        Airborne,
        Underwater,
        Phased,
    };

private:
    struct EncoreState {
        std::string lockedMoveName;
        int remainingTurns = 0;
    };

    struct DisableState {
        std::string lockedMoveName;
        int remainingTurns = 0;
    };

    struct TimedState {
        int remainingTurns = 0;
    };

    struct WishState {
        int remainingTurns = 0;
        int healAmount = 0;
    };

    struct SwitchInRecoveryState {
        bool restorePP = false;
    };

    struct LockOnState {
        Pokemon* target = nullptr;
        int remainingTurns = 0;
    };

    struct CudChewState {
        ItemType berry = ItemType::None;
        int dueTurn = 0;
    };

    struct RuntimeMoveState {
        std::unordered_map<Pokemon*, std::string> lastUsedMoveName;
        std::unordered_map<Pokemon*, EncoreState> encoreState;
        std::unordered_map<Pokemon*, DisableState> disableState;
        std::unordered_map<Pokemon*, TimedState> tauntState;
        std::unordered_map<Pokemon*, TimedState> tormentState;
        std::unordered_map<Pokemon*, TimedState> healBlockState;
        std::unordered_map<Pokemon*, TimedState> embargoState;
        std::unordered_map<Pokemon*, TimedState> yawnState;
        std::unordered_map<Pokemon*, bool> nightmareActive;
        std::unordered_map<Pokemon*, Pokemon*> trappedBySource;
        std::unordered_map<Pokemon*, bool> ingrainActive;
        std::unordered_map<Pokemon*, TimedState> perishSongState;
        std::unordered_map<Pokemon*, Pokemon*> infatuationSource;
        std::unordered_map<Pokemon*, bool> destinyBondActive;
        std::unordered_map<Pokemon*, bool> grudgeActive;
        std::unordered_map<Pokemon*, std::string> protectionVariant;
        std::unordered_map<Pokemon*, bool> ghostCurseActive;
        std::unordered_map<Pokemon*, bool> imprisonActive;
        std::unordered_map<Pokemon*, bool> endureActive;
        std::unordered_map<Pokemon*, LockOnState> lockOnState;
        std::unordered_map<const Side*, WishState> wishState;
        std::unordered_map<const Side*, SwitchInRecoveryState> switchInRecoveryState;
        std::unordered_map<Pokemon*, std::string> chargingMoveName;
        std::unordered_map<Pokemon*, SemiInvulnerableState> semiInvulnerableState;
        std::unordered_map<Pokemon*, int> criticalHitStage;
        std::unordered_map<Pokemon*, bool> typeShiftUsed;
        std::unordered_map<Pokemon*, bool> foresightMarked;
        std::unordered_map<Pokemon*, bool> miracleEyeMarked;
        std::unordered_map<Pokemon*, int> switchedInTurn;
        std::unordered_map<Pokemon*, CudChewState> cudChewPending;
        std::unordered_map<const Side*, bool> quickGuardActive;
        std::unordered_map<const Side*, bool> wideGuardActive;
        int gravityTurns = 0;
        Pokemon* pursuitSwitchTarget = nullptr;
        bool roundUsedThisTurn = false;
    };

    void clearPokemonRuntimeState(Pokemon* pokemon);
    void clearSideRuntimeState(const Side& side);
    void tickEncoreForActor(Pokemon* actor);
    void tickDisableForActor(Pokemon* actor);
    void tickTauntForActor(Pokemon* actor);
    void tickTormentForActor(Pokemon* actor);
    void tickHealBlockForActor(Pokemon* actor);
    void tickEmbargoForActor(Pokemon* actor);
    void tickYawnForActor(Pokemon* actor);
    void tickNightmareForActor(Pokemon* actor);
    void tickIngrainForActor(Pokemon* actor);
    void tickPerishSongForActor(Pokemon* actor);
    void tickGhostCurseForActor(Pokemon* actor);
    void tickWishForSide(Side& side);
    void applyPendingSwitchInRecovery(Side& side, Pokemon* enteringPokemon);
    void tickLockOnForActor(Pokemon* actor);
    void tickGravity();
    SemiInvulnerableState getSemiInvulnerableState(const Pokemon* pokemon) const;
    void beginTurn();
    void endTurn();
    void resetActiveProtection();
    Move applyEncoreOverride(Pokemon* actor, const Move& intendedMove) const;
    bool isMoveDisabledForActor(Pokemon* actor, const Move& move) const;
    bool isMoveBlockedByTaunt(Pokemon* actor, const Move& move) const;
    bool isMoveBlockedByTorment(Pokemon* actor, const Move& move) const;
    bool isMoveBlockedByHealBlock(Pokemon* actor, const Move& move) const;
    bool isItemUsageBlockedByEmbargo(Pokemon* actor) const;
    bool isMoveBlockedByQuickGuard(Pokemon* attacker, Pokemon* defender, const Move& move) const;
    bool isMoveBlockedByArmorTail(Pokemon* attacker, Pokemon* defender, const Move& move) const;
    bool isMoveBlockedByWideGuard(Pokemon* attacker, Pokemon* defender, const Move& move) const;
    bool isMoveBlockedByImprison(Pokemon* actor, const Move& move) const;
    void applyDisableToTarget(Pokemon* target);
    bool ignoresTargetEvasionForMove(const Move& move, Pokemon* defender) const;
    float adjustedTypeEffectivenessForMove(Pokemon* defender, Type moveType) const;
    bool handleTwoTurnChargeTurn(Pokemon* actor, const Move& selectedMove);
    void recordExecutedMove(Pokemon* actor, const Move& selectedMove);
    void handlePursuitOnSwitch(Pokemon* switchingPokemon, Side* switchingSide);
    void applyEntryHazardsOnSwitchIn(Side* enteringSide, Pokemon* enteringPokemon);
    void initializeCoreMoveRules();

    Side sideA;
    Side sideB;
    Field field;
    Weather weather;
    BattleQueue queue;
    EventSystem eventSystem;
    MoveRuleRegistry moveRuleRegistry;
    RuntimeMoveState runtimeMoveState;
    int turnNumber = 0;
};
