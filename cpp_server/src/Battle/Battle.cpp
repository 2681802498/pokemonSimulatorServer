#include "Battle/Battle.h"
#include "Battle/BattleMath.h"
#include "Battle/Moves.h"
#include "Battle/Items.h"
#include "Battle/MoveEffectHandler.h"
#include "Battle/PRNG.h"
#include "Battle/BattleToJson.h"
#include "Battle/Utils.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <unordered_map>

namespace {
std::string triggerToString(Trigger trigger) {
    switch (trigger) {
        case Trigger::OnEntry: return "on_entry";
        case Trigger::OnExit: return "on_exit";
        case Trigger::OnTurnStart: return "on_turn_start";
        case Trigger::OnTurnEnd: return "on_turn_end";
        case Trigger::OnDamage: return "on_damage";
        case Trigger::OnDealDamage: return "on_deal_damage";
        case Trigger::OnAttack: return "on_attack";
        case Trigger::OnFaint: return "on_faint";
        case Trigger::OnStatusInflicted: return "on_status_inflicted";
        case Trigger::OnWeatherChange: return "on_weather_change";
        case Trigger::OnTerrainChange: return "on_terrain_change";
        default: return "unknown";
    }
}

std::string itemTriggerToString(ItemTrigger trigger) {
    switch (trigger) {
        case ItemTrigger::OnEntry: return "on_entry";
        case ItemTrigger::OnTurnStart: return "on_turn_start";
        case ItemTrigger::OnTurnEnd: return "on_turn_end";
        case ItemTrigger::OnDamage: return "on_damage";
        case ItemTrigger::OnDealDamage: return "on_deal_damage";
        case ItemTrigger::AfterDamage: return "after_damage";
        case ItemTrigger::OnAttack: return "on_attack";
        case ItemTrigger::OnFaint: return "on_faint";
        case ItemTrigger::OnSwitchOut: return "on_switch_out";
        case ItemTrigger::OnStatChange: return "on_stat_change";
        case ItemTrigger::OnStatus: return "on_status";
        case ItemTrigger::OnEat: return "on_eat";
        default: return "unknown";
    }
}

std::vector<StatusType> snapshotStatuses(const Pokemon* pokemon) {
    std::vector<StatusType> statuses;
    if (!pokemon) {
        return statuses;
    }
    for (const auto& entry : pokemon->getStatuses()) {
        statuses.push_back(entry.first);
    }
    return statuses;
}

bool hasStatusInSnapshot(const std::vector<StatusType>& statuses, StatusType status) {
    for (StatusType s : statuses) {
        if (s == status) {
            return true;
        }
    }
    return false;
}

void resetSpecialEventFile() {
    std::filesystem::create_directories("cache");
    std::ofstream out("cache/event.json");
    if (out.is_open()) {
        out << "[]";
    }
}

void appendSpecialEvent(const Battle& battle, const std::string& eventType, const json& details) {
    std::filesystem::create_directories("cache");
    const std::string path = "cache/event.json";

    json root = json::array();
    std::ifstream in(path);
    if (in.is_open()) {
        json loaded = json::parse(in, nullptr, false);
        if (loaded.is_array()) {
            root = std::move(loaded);
        }
    }

    json event;
    event["turn"] = battle.getTurnNumber();
    event["event"] = eventType;
    event["details"] = details;
    root.push_back(event);

    std::ofstream out(path);
    if (out.is_open()) {
        out << root.dump(2);
    }
}

std::string normalizeMoveName(const std::string& moveName) {
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

bool moveNameEquals(const Move& move, const char* expectedLowerNoPunct) {
    return normalizeMoveName(move.getName()) == expectedLowerNoPunct;
}

bool isTwoTurnSemiInvulnerableMove(const Move& move) {
    return moveNameEquals(move, "dig") || moveNameEquals(move, "fly") || moveNameEquals(move, "bounce")
        || moveNameEquals(move, "dive") || moveNameEquals(move, "phantomforce")
        || moveNameEquals(move, "shadowforce");
}

bool isHealingMove(const Move& move) {
    const std::string normalized = normalizeMoveName(move.getName());
    static const std::array<const char*, 16> kHealingMoves = {
        "recover", "softboiled", "milkdrink", "slackoff", "shoreup", "healpulse", "morningsun", "synthesis",
        "moonlight", "rest", "wish", "lifedew", "lunarblessing", "roost", "healingwish", "lunardance"
    };

    for (const char* healingMove : kHealingMoves) {
        if (normalized == healingMove) {
            return true;
        }
    }
    return false;
}

Battle::SemiInvulnerableState stateForTwoTurnMove(const Move& move) {
    if (moveNameEquals(move, "dig")) {
        return Battle::SemiInvulnerableState::Underground;
    }
    if (moveNameEquals(move, "fly") || moveNameEquals(move, "bounce")) {
        return Battle::SemiInvulnerableState::Airborne;
    }
    if (moveNameEquals(move, "dive")) {
        return Battle::SemiInvulnerableState::Underwater;
    }
    if (moveNameEquals(move, "phantomforce") || moveNameEquals(move, "shadowforce")) {
        return Battle::SemiInvulnerableState::Phased;
    }
    return Battle::SemiInvulnerableState::None;
}

struct MultiHitConfig {
    int minHits;
    int maxHits;
    bool stopOnMiss;
};

std::optional<MultiHitConfig> getMultiHitConfig(const Move& move) {
    if (moveNameEquals(move, "populationbomb")) {
        return MultiHitConfig{10, 10, true};
    }

    if (moveNameEquals(move, "doublehit") || moveNameEquals(move, "twineedle") || moveNameEquals(move, "bonemerang")
        || moveNameEquals(move, "dualchop") || moveNameEquals(move, "dualwingbeat")
        || moveNameEquals(move, "doubleironbash") || moveNameEquals(move, "geargrind")) {
        return MultiHitConfig{2, 2, false};
    }

    if (moveNameEquals(move, "tripledive") || moveNameEquals(move, "triplekick") || moveNameEquals(move, "tripleaxel")
        || moveNameEquals(move, "surgingstrikes")) {
        return MultiHitConfig{3, 3, false};
    }

    if (moveNameEquals(move, "doubleslap") || moveNameEquals(move, "cometpunch") || moveNameEquals(move, "furyattack")
        || moveNameEquals(move, "pinmissile") || moveNameEquals(move, "iciclespear") || moveNameEquals(move, "rockblast")
        || moveNameEquals(move, "armthrust") || moveNameEquals(move, "tailslap") || moveNameEquals(move, "bulletseed")
        || moveNameEquals(move, "bonerush") || moveNameEquals(move, "watershuriken") || moveNameEquals(move, "barrage")
        || moveNameEquals(move, "furyswipes") || moveNameEquals(move, "spikecannon")
        || moveNameEquals(move, "scaleshot")) {
        return MultiHitConfig{2, 5, false};
    }

    return std::nullopt;
}

int rollHitCount(const MultiHitConfig& config) {
    if (config.minHits >= config.maxHits) {
        return config.minHits;
    }

    if (config.minHits == 2 && config.maxHits == 5) {
        const int roll = static_cast<int>(PRNG::nextFloat(0.0f, 100.0f));
        if (roll < 35) return 2;
        if (roll < 70) return 3;
        if (roll < 85) return 4;
        return 5;
    }

    const int span = config.maxHits - config.minHits + 1;
    const int roll = static_cast<int>(PRNG::nextFloat(0.0f, static_cast<float>(span)));
    return config.minHits + std::min(span - 1, std::max(0, roll));
}

bool isStatusInflictingEffect(MoveEffect effect) {
    switch (effect) {
        case MoveEffect::Paralyze:
        case MoveEffect::Sleep:
        case MoveEffect::Freeze:
        case MoveEffect::Burn:
        case MoveEffect::Poison:
        case MoveEffect::Confuse:
            return true;
        default:
            return false;
    }
}

bool isBerryItem(ItemType itemType) {
    const int value = static_cast<int>(itemType);
    return value >= static_cast<int>(ItemType::OranBerry)
        && value <= static_cast<int>(ItemType::RowapBerry);
}

void applyCudChewBerryEffect(Pokemon* pokemon, ItemType berry) {
    if (!pokemon || pokemon->isFainted()) {
        return;
    }

    switch (berry) {
        case ItemType::OranBerry:
            pokemon->setCurrentHP(pokemon->getCurrentHP() + 10);
            break;
        case ItemType::SitrusBerry:
            pokemon->setCurrentHP(pokemon->getCurrentHP() + std::max(1, pokemon->getMaxHP() / 4));
            break;
        case ItemType::LumBerry:
            pokemon->clearStatuses();
            break;
        case ItemType::ChestoBerry:
            pokemon->removeStatus(StatusType::Sleep);
            break;
        case ItemType::PechaBerry:
            pokemon->removeStatus(StatusType::Poison);
            pokemon->removeStatus(StatusType::ToxicPoison);
            break;
        case ItemType::RawstBerry:
            pokemon->removeStatus(StatusType::Burn);
            break;
        case ItemType::AspearBerry:
            pokemon->removeStatus(StatusType::Freeze);
            break;
        case ItemType::PersimBerry:
            pokemon->removeStatus(StatusType::Confusion);
            break;
        case ItemType::CheriBerry:
            pokemon->removeStatus(StatusType::Paralysis);
            break;
        case ItemType::FigyBerry:
        case ItemType::WikiBerry:
        case ItemType::MagoBerry:
        case ItemType::AguavBerry:
        case ItemType::IapapaBerry:
            pokemon->setCurrentHP(pokemon->getCurrentHP() + std::max(1, pokemon->getMaxHP() / 3));
            break;
        default:
            break;
    }
}

bool isCrashRecoilMove(const Move& move) {
    return moveNameEquals(move, "jumpkick") || moveNameEquals(move, "highjumpkick");
}

bool isHighCriticalMove(const Move& move) {
    return moveNameEquals(move, "slash")
        || moveNameEquals(move, "razorleaf")
        || moveNameEquals(move, "karatechop")
        || moveNameEquals(move, "crabhammer")
        || moveNameEquals(move, "aircutter")
        || moveNameEquals(move, "blazekick")
        || moveNameEquals(move, "crosspoison")
        || moveNameEquals(move, "drillrun")
        || moveNameEquals(move, "leafblade")
        || moveNameEquals(move, "nightslash")
        || moveNameEquals(move, "psychocut")
        || moveNameEquals(move, "shadowclaw")
        || moveNameEquals(move, "stoneedge")
        || moveNameEquals(move, "spacialrend");
}

Type findResistingTypeForAttack(Type attackType) {
    for (int candidate = 0; candidate < static_cast<int>(Type::Count); ++candidate) {
        const Type candidateType = static_cast<Type>(candidate);
        if (candidateType == Type::Count) {
            continue;
        }
        const float effectiveness = Utils::TypeEffectiveness[static_cast<int>(attackType)][candidate];
        if (effectiveness <= 0.5f) {
            return candidateType;
        }
    }
    return Type::Normal;
}

bool isSelfStatDropMove(const Move& move) {
    return moveNameEquals(move, "closecombat") || moveNameEquals(move, "superpower")
        || moveNameEquals(move, "overheat") || moveNameEquals(move, "leafstorm")
        || moveNameEquals(move, "dracometeor") || moveNameEquals(move, "fleurcannon")
        || moveNameEquals(move, "steelbeam") || moveNameEquals(move, "vcreate")
        || moveNameEquals(move, "clangoroussoulblaze");
}

void applyCrashRecoil(Pokemon* attacker, const Move& move) {
    if (!attacker || !isCrashRecoilMove(move)) {
        return;
    }

    const int recoilDamage = std::max(1, attacker->getMaxHP() / 2);
    attacker->setCurrentHP(attacker->getCurrentHP() - recoilDamage);
}

void applySelfStatDropMove(Pokemon* attacker, const Move& move) {
    if (!attacker || !isSelfStatDropMove(move)) {
        return;
    }

    if (moveNameEquals(move, "closecombat") || moveNameEquals(move, "superpower")) {
        attacker->changeStatStage(StatIndex::Attack, -1);
        attacker->changeStatStage(StatIndex::Defense, -1);
        return;
    }

    if (moveNameEquals(move, "overheat") || moveNameEquals(move, "leafstorm") || moveNameEquals(move, "dracometeor")
        || moveNameEquals(move, "fleurcannon")) {
        attacker->changeStatStage(StatIndex::SpecialAttack, -2);
        return;
    }

    if (moveNameEquals(move, "steelbeam")) {
        attacker->changeStatStage(StatIndex::SpecialAttack, -1);
        return;
    }

    if (moveNameEquals(move, "vcreate")) {
        attacker->changeStatStage(StatIndex::Defense, -1);
        attacker->changeStatStage(StatIndex::SpecialDefense, -1);
        attacker->changeStatStage(StatIndex::Speed, -1);
        return;
    }

    if (moveNameEquals(move, "clangoroussoulblaze")) {
        attacker->changeStatStage(StatIndex::Attack, -1);
        attacker->changeStatStage(StatIndex::Defense, -1);
        attacker->changeStatStage(StatIndex::SpecialAttack, -1);
        attacker->changeStatStage(StatIndex::SpecialDefense, -1);
        attacker->changeStatStage(StatIndex::Speed, -1);
    }
}

bool doesMoveHit(const Pokemon* attacker, const Pokemon* defender, const Move& move, bool ignoreTargetEvasion, bool gravityActive) {
    if (!attacker || !defender) {
        return false;
    }

    const int baseAccuracy = move.getAccuracy();
    if (baseAccuracy <= 0) {
        return false;
    }

    const int defenderEvasionStage = ignoreTargetEvasion ? 0 : defender->getEvasionStage();

    if (baseAccuracy >= 100 && attacker->getAccuracyStage() == 0 && defenderEvasionStage == 0) {
        return true;
    }

    float adjustedAccuracy = static_cast<float>(baseAccuracy);
    adjustedAccuracy *= stageMultiplier(attacker->getAccuracyStage() - defenderEvasionStage);
    if (gravityActive) {
        adjustedAccuracy *= (5.0f / 3.0f);
    }
    if (adjustedAccuracy > 100.0f) {
        adjustedAccuracy = 100.0f;
    }
    if (adjustedAccuracy < 1.0f) {
        adjustedAccuracy = 1.0f;
    }

    return PRNG::nextFloat(0.0f, 100.0f) <= adjustedAccuracy;
}

void clearVolatileSwitchState(Pokemon* pokemon) {
    if (!pokemon) {
        return;
    }

    pokemon->clearSubstitute();
    pokemon->clearLeechSeedSource();
    if (pokemon->getAccuracyStage() != 0) {
        pokemon->changeAccuracyStage(-pokemon->getAccuracyStage());
    }
    if (pokemon->getEvasionStage() != 0) {
        pokemon->changeEvasionStage(-pokemon->getEvasionStage());
    }
}

int firstAvailableSwitchIndex(const Side& side) {
    const int activeIndex = side.getActiveIndex();
    const auto& team = side.getTeam();
    const int teamSize = side.getPokemonCount();

    for (int i = 0; i < teamSize; ++i) {
        if (i == activeIndex) {
            continue;
        }
        Pokemon* pokemon = team[i];
        if (pokemon != nullptr && !pokemon->isFainted()) {
            return i;
        }
    }

    return -1;
}

void triggerEntryEffectsAfterSwitch(Battle& battle, Side& side) {
    Pokemon* newActive = side.getActivePokemon();
    if (!newActive) {
        return;
    }

    Pokemon* opponentPokemon = (&side == &battle.getSideA()) ? battle.getSideB().getActivePokemon() : battle.getSideA().getActivePokemon();
    battle.triggerAbility(newActive, Trigger::OnEntry, opponentPokemon);
    battle.triggerItemEffect(newActive, ItemTrigger::OnEntry, opponentPokemon);
}

bool forceSwitchSide(Battle& battle, Side& side) {
    const int nextIndex = firstAvailableSwitchIndex(side);
    if (nextIndex < 0) {
        return false;
    }

    if (!battle.switchPokemon(side, nextIndex)) {
        return false;
    }

    triggerEntryEffectsAfterSwitch(battle, side);
    return true;
}

bool applyConfusionMove(Battle& battle, Pokemon* defender, const Move& move) {
    if (!defender || defender->getSubstituteHP() > 0 || defender->hasStatus(StatusType::Confusion)) {
        return true;
    }

    defender->addStatus(StatusType::Confusion);
    appendSpecialEvent(battle, "status_apply", {
        {"pokemon", defender->getName()},
        {"status", BattleToJson::statusTypeToString(StatusType::Confusion)},
        {"reason", "move_effect"},
        {"move", move.getName()}
    });
    return true;
}

void resetPokemonStatStages(Pokemon* pokemon) {
    if (!pokemon) {
        return;
    }

    constexpr std::array<StatIndex, 5> kResettableStats = {
        StatIndex::Attack,
        StatIndex::Defense,
        StatIndex::SpecialAttack,
        StatIndex::SpecialDefense,
        StatIndex::Speed,
    };

    for (StatIndex stat : kResettableStats) {
        const int currentStage = pokemon->getStatStage(stat);
        if (currentStage != 0) {
            pokemon->changeStatStage(stat, -currentStage);
        }
    }
}

struct ScopedActionEventEmitter {
    EventSystem* eventSystem = nullptr;
    int turnNumber = 0;
    BattleAction* action = nullptr;
    bool active = false;

    ~ScopedActionEventEmitter() {
        if (!active || !eventSystem || !action) {
            return;
        }

        eventSystem->emitActionEvent(RuntimeActionEvent{
            RuntimeEventPhase::AfterActionResolve,
            turnNumber,
            *action,
        });
    }
};
}  // namespace

const Side* Battle::findSideForPokemon(const Battle& battle, Pokemon* pokemon) {
    if (battle.getSideA().getActivePokemon() == pokemon) return &battle.getSideA();
    if (battle.getSideB().getActivePokemon() == pokemon) return &battle.getSideB();
    for (Pokemon* member : battle.getSideA().getTeam()) {
        if (member == pokemon) return &battle.getSideA();
    }
    for (Pokemon* member : battle.getSideB().getTeam()) {
        if (member == pokemon) return &battle.getSideB();
    }
    return nullptr;
}

Side* Battle::findSideForPokemon(Battle& battle, Pokemon* pokemon) {
    return const_cast<Side*>(findSideForPokemon(static_cast<const Battle&>(battle), pokemon));
}

Battle::Battle(Side sideA, Side sideB)
    : sideA(std::move(sideA)), sideB(std::move(sideB)), field(), weather(), queue(), eventSystem(), turnNumber(0) {
    resetSpecialEventFile();
    initializeCoreMoveRules();
    clearSideRuntimeState(this->sideA);
    clearSideRuntimeState(this->sideB);
    runtimeMoveState.roundUsedThisTurn = false;

    // 触发双方活跃宝可梦的出场特性
    Pokemon* activeA = this->sideA.getActivePokemon();
    Pokemon* activeB = this->sideB.getActivePokemon();
    
    if (activeA) {
        triggerAbility(activeA, Trigger::OnEntry, activeB);
        triggerItemEffect(activeA, ItemTrigger::OnEntry, activeB);
    }
    
    if (activeB) {
        triggerAbility(activeB, Trigger::OnEntry, activeA);
        triggerItemEffect(activeB, ItemTrigger::OnEntry, activeA);
    }
}

void Battle::initializeCoreMoveRules() {
    ::initializeCoreMoveRules(*this);
}

void Battle::clearPokemonRuntimeState(Pokemon* pokemon) {
    if (!pokemon) {
        return;
    }

    runtimeMoveState.lastUsedMoveName.erase(pokemon);
    runtimeMoveState.encoreState.erase(pokemon);
    runtimeMoveState.disableState.erase(pokemon);
    runtimeMoveState.tauntState.erase(pokemon);
    runtimeMoveState.tormentState.erase(pokemon);
    runtimeMoveState.healBlockState.erase(pokemon);
    runtimeMoveState.embargoState.erase(pokemon);
    runtimeMoveState.yawnState.erase(pokemon);
    runtimeMoveState.nightmareActive.erase(pokemon);
    runtimeMoveState.trappedBySource.erase(pokemon);
    runtimeMoveState.ingrainActive.erase(pokemon);
    runtimeMoveState.perishSongState.erase(pokemon);
    runtimeMoveState.infatuationSource.erase(pokemon);
    runtimeMoveState.destinyBondActive.erase(pokemon);
    runtimeMoveState.grudgeActive.erase(pokemon);
    runtimeMoveState.ghostCurseActive.erase(pokemon);
    for (auto it = runtimeMoveState.trappedBySource.begin(); it != runtimeMoveState.trappedBySource.end();) {
        if (it->second == pokemon) {
            it = runtimeMoveState.trappedBySource.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = runtimeMoveState.infatuationSource.begin(); it != runtimeMoveState.infatuationSource.end();) {
        if (it->second == pokemon) {
            it = runtimeMoveState.infatuationSource.erase(it);
        } else {
            ++it;
        }
    }
    runtimeMoveState.imprisonActive.erase(pokemon);
    runtimeMoveState.endureActive.erase(pokemon);
    runtimeMoveState.lockOnState.erase(pokemon);
    runtimeMoveState.chargingMoveName.erase(pokemon);
    runtimeMoveState.semiInvulnerableState.erase(pokemon);
    runtimeMoveState.criticalHitStage.erase(pokemon);
    runtimeMoveState.typeShiftUsed.erase(pokemon);
    runtimeMoveState.foresightMarked.erase(pokemon);
    runtimeMoveState.miracleEyeMarked.erase(pokemon);
    runtimeMoveState.switchedInTurn.erase(pokemon);
    runtimeMoveState.cudChewPending.erase(pokemon);
    if (runtimeMoveState.pursuitSwitchTarget == pokemon) {
        runtimeMoveState.pursuitSwitchTarget = nullptr;
    }
}

bool Battle::ignoresTargetEvasionForMove(const Move& move, Pokemon* defender) const {
    if (!defender) {
        return false;
    }

    const auto foresightIt = runtimeMoveState.foresightMarked.find(defender);
    if (foresightIt != runtimeMoveState.foresightMarked.end() && foresightIt->second) {
        if (moveNameEquals(move, "foresight") || moveNameEquals(move, "odorsleuth")) {
            return false;
        }
        return true;
    }

    const auto miracleEyeIt = runtimeMoveState.miracleEyeMarked.find(defender);
    if (miracleEyeIt != runtimeMoveState.miracleEyeMarked.end() && miracleEyeIt->second) {
        if (moveNameEquals(move, "miracleeye")) {
            return false;
        }
        return true;
    }

    return false;
}

float Battle::adjustedTypeEffectivenessForMove(Pokemon* defender, Type moveType) const {
    if (!defender) {
        return 1.0f;
    }

    auto adjustedMultiplierForType = [this, moveType, defender](Type defendingType) {
        if (defendingType == Type::Count) {
            return 1.0f;
        }

        float multiplier = Utils::TypeEffectiveness[static_cast<int>(moveType)][static_cast<int>(defendingType)];
        if (defendingType == Type::Ghost
            && (moveType == Type::Normal || moveType == Type::Fighting)) {
            const auto it = runtimeMoveState.foresightMarked.find(defender);
            if (it != runtimeMoveState.foresightMarked.end() && it->second) {
                multiplier = 1.0f;
            }
        }

        if (defendingType == Type::Dark && moveType == Type::Psychic) {
            const auto it = runtimeMoveState.miracleEyeMarked.find(defender);
            if (it != runtimeMoveState.miracleEyeMarked.end() && it->second) {
                multiplier = 1.0f;
            }
        }

        return multiplier;
    };

    const float first = adjustedMultiplierForType(defender->getType1());
    const float second = adjustedMultiplierForType(defender->getType2());
    return first * second;
}

void Battle::clearSideRuntimeState(const Side& side) {
    runtimeMoveState.wishState.erase(&side);
    runtimeMoveState.switchInRecoveryState.erase(&side);
    runtimeMoveState.quickGuardActive.erase(&side);
    runtimeMoveState.wideGuardActive.erase(&side);
    for (Pokemon* member : side.getTeam()) {
        clearPokemonRuntimeState(member);
    }
}

void Battle::tickEncoreForActor(Pokemon* actor) {
    auto it = runtimeMoveState.encoreState.find(actor);
    if (it == runtimeMoveState.encoreState.end()) {
        return;
    }
    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0) {
        runtimeMoveState.encoreState.erase(it);
    }
}

void Battle::tickDisableForActor(Pokemon* actor) {
    auto it = runtimeMoveState.disableState.find(actor);
    if (it == runtimeMoveState.disableState.end()) {
        return;
    }
    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0) {
        runtimeMoveState.disableState.erase(it);
    }
}

void Battle::tickTauntForActor(Pokemon* actor) {
    auto it = runtimeMoveState.tauntState.find(actor);
    if (it == runtimeMoveState.tauntState.end()) {
        return;
    }
    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0) {
        runtimeMoveState.tauntState.erase(it);
    }
}

void Battle::tickTormentForActor(Pokemon* actor) {
    auto it = runtimeMoveState.tormentState.find(actor);
    if (it == runtimeMoveState.tormentState.end()) {
        return;
    }
    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0) {
        runtimeMoveState.tormentState.erase(it);
    }
}

void Battle::tickHealBlockForActor(Pokemon* actor) {
    auto it = runtimeMoveState.healBlockState.find(actor);
    if (it == runtimeMoveState.healBlockState.end()) {
        return;
    }
    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0) {
        runtimeMoveState.healBlockState.erase(it);
    }
}

void Battle::tickEmbargoForActor(Pokemon* actor) {
    auto it = runtimeMoveState.embargoState.find(actor);
    if (it == runtimeMoveState.embargoState.end()) {
        return;
    }
    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0) {
        runtimeMoveState.embargoState.erase(it);
    }
}

void Battle::tickYawnForActor(Pokemon* actor) {
    auto it = runtimeMoveState.yawnState.find(actor);
    if (it == runtimeMoveState.yawnState.end()) {
        return;
    }

    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns > 0) {
        return;
    }

    if (actor && !actor->isFainted()) {
        const bool blockedByAbility = resolveStatusImmunity(actor->getAbility(), StatusType::Sleep);
        if (!blockedByAbility && !hasMajorStatus(actor)) {
            actor->addStatus(StatusType::Sleep);
        }
    }

    runtimeMoveState.yawnState.erase(it);
}

void Battle::tickNightmareForActor(Pokemon* actor) {
    auto it = runtimeMoveState.nightmareActive.find(actor);
    if (it == runtimeMoveState.nightmareActive.end()) {
        return;
    }

    if (!actor || actor->isFainted() || !actor->hasStatus(StatusType::Sleep)) {
        runtimeMoveState.nightmareActive.erase(it);
        return;
    }

    const int damage = std::max(1, actor->getMaxHP() / 4);
    actor->setCurrentHP(actor->getCurrentHP() - damage);

    if (actor->isFainted() || !actor->hasStatus(StatusType::Sleep)) {
        runtimeMoveState.nightmareActive.erase(actor);
    }
}

void Battle::tickIngrainForActor(Pokemon* actor) {
    const auto it = runtimeMoveState.ingrainActive.find(actor);
    if (it == runtimeMoveState.ingrainActive.end()) {
        return;
    }

    if (!actor || actor->isFainted()) {
        runtimeMoveState.ingrainActive.erase(it);
        return;
    }

    const int healAmount = std::max(1, actor->getMaxHP() / 16);
    actor->setCurrentHP(actor->getCurrentHP() + healAmount);
}

void Battle::tickPerishSongForActor(Pokemon* actor) {
    auto it = runtimeMoveState.perishSongState.find(actor);
    if (it == runtimeMoveState.perishSongState.end()) {
        return;
    }

    if (!actor || actor->isFainted()) {
        runtimeMoveState.perishSongState.erase(it);
        return;
    }

    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0) {
        actor->setCurrentHP(0);
        runtimeMoveState.perishSongState.erase(it);
    }
}

void Battle::tickGhostCurseForActor(Pokemon* actor) {
    auto it = runtimeMoveState.ghostCurseActive.find(actor);
    if (it == runtimeMoveState.ghostCurseActive.end()) {
        return;
    }

    if (!actor || actor->isFainted()) {
        runtimeMoveState.ghostCurseActive.erase(it);
        return;
    }

    const int damage = std::max(1, actor->getMaxHP() / 4);
    actor->setCurrentHP(actor->getCurrentHP() - damage);
    if (actor->isFainted()) {
        runtimeMoveState.ghostCurseActive.erase(it);
    }
}

void Battle::tickWishForSide(Side& side) {
    auto it = runtimeMoveState.wishState.find(&side);
    if (it == runtimeMoveState.wishState.end()) {
        return;
    }

    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns > 0) {
        return;
    }

    Pokemon* active = side.getActivePokemon();
    if (active && !active->isFainted() && it->second.healAmount > 0) {
        active->setCurrentHP(active->getCurrentHP() + it->second.healAmount);
    }

    runtimeMoveState.wishState.erase(it);
}

void Battle::applyPendingSwitchInRecovery(Side& side, Pokemon* enteringPokemon) {
    if (!enteringPokemon || enteringPokemon->isFainted()) {
        return;
    }

    auto it = runtimeMoveState.switchInRecoveryState.find(&side);
    if (it == runtimeMoveState.switchInRecoveryState.end()) {
        return;
    }

    enteringPokemon->setCurrentHP(enteringPokemon->getMaxHP());
    enteringPokemon->clearStatuses();

    if (it->second.restorePP) {
        std::vector<Move> refreshedMoves = enteringPokemon->getMoves();
        for (Move& knownMove : refreshedMoves) {
            knownMove.setPP(knownMove.getMaxPP());
        }
        enteringPokemon->replaceMoves(refreshedMoves);
    }

    runtimeMoveState.switchInRecoveryState.erase(it);
}

void Battle::tickLockOnForActor(Pokemon* actor) {
    auto it = runtimeMoveState.lockOnState.find(actor);
    if (it == runtimeMoveState.lockOnState.end()) {
        return;
    }
    it->second.remainingTurns -= 1;
    if (it->second.remainingTurns <= 0 || it->second.target == nullptr || it->second.target->isFainted()) {
        runtimeMoveState.lockOnState.erase(it);
    }
}

void Battle::tickGravity() {
    if (runtimeMoveState.gravityTurns > 0) {
        runtimeMoveState.gravityTurns -= 1;
    }
}

bool Battle::isMoveDisabledForActor(Pokemon* actor, const Move& move) const {
    if (!actor) {
        return false;
    }

    const auto it = runtimeMoveState.disableState.find(actor);
    if (it == runtimeMoveState.disableState.end() || it->second.lockedMoveName.empty()) {
        return false;
    }

    return normalizeMoveName(move.getName()) == normalizeMoveName(it->second.lockedMoveName);
}

bool Battle::isMoveBlockedByTaunt(Pokemon* actor, const Move& move) const {
    if (!actor || move.getCategory() != Category::Status) {
        return false;
    }
    const auto it = runtimeMoveState.tauntState.find(actor);
    return it != runtimeMoveState.tauntState.end() && it->second.remainingTurns > 0;
}

bool Battle::isMoveBlockedByTorment(Pokemon* actor, const Move& move) const {
    if (!actor) {
        return false;
    }
    const auto it = runtimeMoveState.tormentState.find(actor);
    if (it == runtimeMoveState.tormentState.end() || it->second.remainingTurns <= 0) {
        return false;
    }

    const auto lastMoveIt = runtimeMoveState.lastUsedMoveName.find(actor);
    if (lastMoveIt == runtimeMoveState.lastUsedMoveName.end() || lastMoveIt->second.empty()) {
        return false;
    }
    return normalizeMoveName(lastMoveIt->second) == normalizeMoveName(move.getName());
}

bool Battle::isMoveBlockedByHealBlock(Pokemon* actor, const Move& move) const {
    if (!actor) {
        return false;
    }

    const auto it = runtimeMoveState.healBlockState.find(actor);
    if (it == runtimeMoveState.healBlockState.end() || it->second.remainingTurns <= 0) {
        return false;
    }

    return isHealingMove(move);
}

bool Battle::isItemUsageBlockedByEmbargo(Pokemon* actor) const {
    if (!actor) {
        return false;
    }

    const auto it = runtimeMoveState.embargoState.find(actor);
    return it != runtimeMoveState.embargoState.end() && it->second.remainingTurns > 0;
}

bool Battle::isMoveBlockedByQuickGuard(Pokemon* attacker, Pokemon* defender, const Move& move) const {
    if (!attacker || !defender) {
        return false;
    }

    if (move.getPriority() <= 0) {
        return false;
    }

    const Side* attackerSide = findSideForPokemon(*this, attacker);
    const Side* defenderSide = findSideForPokemon(*this, defender);
    if (!attackerSide || !defenderSide || attackerSide == defenderSide) {
        return false;
    }

    auto it = runtimeMoveState.quickGuardActive.find(defenderSide);
    return it != runtimeMoveState.quickGuardActive.end() && it->second;
}

bool Battle::isMoveBlockedByArmorTail(Pokemon* attacker, Pokemon* defender, const Move& move) const {
    if (!attacker || !defender) {
        return false;
    }

    if (move.getPriority() <= 0) {
        return false;
    }

    const Side* attackerSide = findSideForPokemon(*this, attacker);
    const Side* defenderSide = findSideForPokemon(*this, defender);
    if (!attackerSide || !defenderSide || attackerSide == defenderSide) {
        return false;
    }

    return abilityBlocksPriorityTargetedMoves(defender->getAbility());
}

bool Battle::isMoveBlockedByWideGuard(Pokemon* attacker, Pokemon* defender, const Move& move) const {
    if (!attacker || !defender) {
        return false;
    }

    if (move.getTarget() != Target::AllOpponents && move.getTarget() != Target::All) {
        return false;
    }

    const Side* attackerSide = findSideForPokemon(*this, attacker);
    const Side* defenderSide = findSideForPokemon(*this, defender);
    if (!attackerSide || !defenderSide || attackerSide == defenderSide) {
        return false;
    }

    auto it = runtimeMoveState.wideGuardActive.find(defenderSide);
    return it != runtimeMoveState.wideGuardActive.end() && it->second;
}

bool Battle::canBeForcedToSwitch(Pokemon* defender) const {
    if (!defender || defender->getSubstituteHP() > 0) {
        return false;
    }

    const auto ingrainIt = runtimeMoveState.ingrainActive.find(defender);
    if (ingrainIt != runtimeMoveState.ingrainActive.end() && ingrainIt->second) {
        return false;
    }

    const auto trappedIt = runtimeMoveState.trappedBySource.find(defender);
    if (trappedIt != runtimeMoveState.trappedBySource.end()) {
        Pokemon* trappingSource = trappedIt->second;
        const Side* defenderSide = findSideForPokemon(*this, defender);
        const Side* sourceSide = findSideForPokemon(*this, trappingSource);
        const bool stillTrapped = trappingSource && !trappingSource->isFainted()
            && defenderSide && sourceSide && defenderSide != sourceSide;
        if (stillTrapped) {
            return false;
        }
    }

    return true;
}

bool Battle::isMoveBlockedByImprison(Pokemon* actor, const Move& move) const {
    if (!actor) {
        return false;
    }

    const Side* actorSide = findSideForPokemon(*this, actor);
    if (!actorSide) {
        return false;
    }

    const Side& opponentSide = (actorSide == &sideA) ? sideB : sideA;
    Pokemon* opponentActive = opponentSide.getActivePokemon();
    if (!opponentActive || opponentActive->isFainted()) {
        return false;
    }

    const auto imprisonIt = runtimeMoveState.imprisonActive.find(opponentActive);
    if (imprisonIt == runtimeMoveState.imprisonActive.end() || !imprisonIt->second) {
        return false;
    }

    const std::string selectedMoveName = normalizeMoveName(move.getName());
    for (const Move& opponentMove : opponentActive->getMoves()) {
        if (normalizeMoveName(opponentMove.getName()) == selectedMoveName) {
            return true;
        }
    }
    return false;
}

void Battle::applyDisableToTarget(Pokemon* target) {
    if (!target) {
        return;
    }

    const auto it = runtimeMoveState.lastUsedMoveName.find(target);
    if (it == runtimeMoveState.lastUsedMoveName.end() || it->second.empty()) {
        return;
    }

    runtimeMoveState.disableState[target] = DisableState{it->second, 4};
}

Battle::SemiInvulnerableState Battle::getSemiInvulnerableState(const Pokemon* pokemon) const {
    auto it = runtimeMoveState.semiInvulnerableState.find(const_cast<Pokemon*>(pokemon));
    if (it == runtimeMoveState.semiInvulnerableState.end()) {
        return SemiInvulnerableState::None;
    }
    return it->second;
}

void Battle::beginTurn() {
    ++turnNumber;
    runtimeMoveState.roundUsedThisTurn = false;
    queue.setTrickRoom(field.isTrickRoom());

    for (Pokemon* actor : {sideA.getActivePokemon(), sideB.getActivePokemon()}) {
        if (actor) {
            runtimeMoveState.destinyBondActive.erase(actor);
            runtimeMoveState.grudgeActive.erase(actor);
        }
    }

    applyStatusEffects();

    tickWishForSide(sideA);
    tickWishForSide(sideB);

    for (Pokemon* actor : {sideA.getActivePokemon(), sideB.getActivePokemon()}) {
        tickEncoreForActor(actor);
        tickDisableForActor(actor);
        tickTauntForActor(actor);
        tickTormentForActor(actor);
        tickHealBlockForActor(actor);
        tickEmbargoForActor(actor);
        tickYawnForActor(actor);
        tickLockOnForActor(actor);
    }

    triggerAbilities(Trigger::OnTurnStart, nullptr);
    triggerItemEffects(ItemTrigger::OnTurnStart, nullptr);
}

void Battle::applyStatusEffects() {
    for (Side* side : {&sideA, &sideB}) {
        Pokemon* active = side->getActivePokemon();
        if (!active || active->isFainted()) {
            side->tickMistTurns();
            side->tickSafeguardTurns();
            side->tickMudSportTurns();
            side->tickWaterSportTurns();
            continue;
        }

        if (active->hasStatus(StatusType::Sleep)) {
            active->tickStatusDuration(StatusType::Sleep);
        }
        if (active->hasStatus(StatusType::Flinch)) {
            active->tickStatusDuration(StatusType::Flinch);
        }
    }
}

void Battle::resetActiveProtection() {
    Pokemon* activeA = sideA.getActivePokemon();
    if (activeA) {
        activeA->setIsProtected(false);
        runtimeMoveState.endureActive.erase(activeA);
        runtimeMoveState.protectionVariant.erase(activeA);
    }

    Pokemon* activeB = sideB.getActivePokemon();
    if (activeB) {
        activeB->setIsProtected(false);
        runtimeMoveState.endureActive.erase(activeB);
        runtimeMoveState.protectionVariant.erase(activeB);
    }

    runtimeMoveState.quickGuardActive.clear();
    runtimeMoveState.wideGuardActive.clear();
}

void Battle::endTurn() {
    triggerAbilities(Trigger::OnTurnEnd, nullptr);
    triggerItemEffects(ItemTrigger::OnTurnEnd, nullptr);

    for (auto it = runtimeMoveState.cudChewPending.begin(); it != runtimeMoveState.cudChewPending.end();) {
        Pokemon* owner = it->first;
        const CudChewState state = it->second;
        if (!owner || owner->isFainted() || owner->getAbility() != AbilityType::CudChew || owner->getItemType() != ItemType::None) {
            it = runtimeMoveState.cudChewPending.erase(it);
            continue;
        }
        if (state.dueTurn <= turnNumber) {
            const int hpBefore = owner->getCurrentHP();
            applyCudChewBerryEffect(owner, state.berry);
            appendSpecialEvent(*this, "item_replay", {
                {"pokemon", owner->getName()},
                {"reason", "cud_chew"},
                {"berry", BattleToJson::itemTypeToString(state.berry)},
                {"from", hpBefore},
                {"to", owner->getCurrentHP()}
            });
            it = runtimeMoveState.cudChewPending.erase(it);
            continue;
        }
        ++it;
    }

    for (Pokemon* actor : {sideA.getActivePokemon(), sideB.getActivePokemon()}) {
        tickNightmareForActor(actor);
        tickIngrainForActor(actor);
        tickPerishSongForActor(actor);
        tickGhostCurseForActor(actor);
    }

    applyWeatherEffects();
    applyFieldEffects();
    field.tick();
    weather.tick();
    tickGravity();

    resetActiveProtection();
}

Move Battle::applyEncoreOverride(Pokemon* actor, const Move& intendedMove) const {
    Move selectedMove = intendedMove;
    const auto encoreIt = runtimeMoveState.encoreState.find(actor);
    if (encoreIt == runtimeMoveState.encoreState.end() || encoreIt->second.lockedMoveName.empty()) {
        return selectedMove;
    }

    if (normalizeMoveName(selectedMove.getName()) == normalizeMoveName(encoreIt->second.lockedMoveName)) {
        return selectedMove;
    }

    const Move forcedMove = createMoveByName(encoreIt->second.lockedMoveName);
    if (!forcedMove.getName().empty()) {
        selectedMove = forcedMove;
    }
    return selectedMove;
}

bool Battle::handleTwoTurnChargeTurn(Pokemon* actor, const Move& selectedMove) {
    if (!isTwoTurnSemiInvulnerableMove(selectedMove)) {
        return false;
    }

    const std::string normalizedMove = normalizeMoveName(selectedMove.getName());
    const auto chargingIt = runtimeMoveState.chargingMoveName.find(actor);
    if (chargingIt != runtimeMoveState.chargingMoveName.end() && chargingIt->second == normalizedMove) {
        runtimeMoveState.chargingMoveName.erase(actor);
        runtimeMoveState.semiInvulnerableState.erase(actor);
        return false;
    }

    runtimeMoveState.chargingMoveName[actor] = normalizedMove;
    runtimeMoveState.semiInvulnerableState[actor] = stateForTwoTurnMove(selectedMove);
    recordExecutedMove(actor, selectedMove);
    return true;
}

void Battle::recordExecutedMove(Pokemon* actor, const Move& selectedMove) {
    runtimeMoveState.lastUsedMoveName[actor] = selectedMove.getName();
    if (moveNameEquals(selectedMove, "round")) {
        runtimeMoveState.roundUsedThisTurn = true;
    }
    tickEncoreForActor(actor);
}

void Battle::handlePursuitOnSwitch(Pokemon* switchingPokemon, Side* switchingSide) {
    if (!switchingPokemon || !switchingSide) {
        return;
    }

    Pokemon* opponentActive = (switchingSide == &sideA) ? sideB.getActivePokemon() : sideA.getActivePokemon();
    if (!opponentActive || switchingPokemon->isFainted() || opponentActive->isFainted()) {
        return;
    }

    for (const Move& candidate : opponentActive->getMoves()) {
        if (!moveNameEquals(candidate, "pursuit")) {
            continue;
        }

        runtimeMoveState.pursuitSwitchTarget = switchingPokemon;
        const int pursuitDamage = calculateDamage(opponentActive, switchingPokemon, candidate);
        runtimeMoveState.pursuitSwitchTarget = nullptr;
        switchingPokemon->setCurrentHP(switchingPokemon->getCurrentHP() - pursuitDamage);
        break;
    }
}

Side& Battle::getSideA() {
    return sideA;
}

const Side& Battle::getSideA() const {
    return sideA;
}

Side& Battle::getSideB() {
    return sideB;
}

const Side& Battle::getSideB() const {
    return sideB;
}

Side& Battle::getOpponentSide(const Side& side) {
    if (&side == &sideA) {
        return sideB;
    }
    return sideA;
}

Field& Battle::getField() {
    return field;
}

Weather& Battle::getWeather() {
    return weather;
}

EventSystem& Battle::getEventSystem() {
    return eventSystem;
}

const EventSystem& Battle::getEventSystem() const {
    return eventSystem;
}

void Battle::enqueueAction(const BattleAction& action) {
    BattleAction adjusted = action;
    if (adjusted.type == ActionType::Attack
        && adjusted.actor
        && adjusted.move.getCategory() == Category::Status) {
        adjusted.movePriority += abilityStatusMovePriorityBonus(adjusted.actor->getAbility());
    }
    if (adjusted.type == ActionType::Attack && adjusted.actor && adjusted.actor->getItemType() == ItemType::QuickClaw) {
        if (PRNG::nextFloat(0.0f, 1.0f) < 0.2f) {
            adjusted.priority = std::numeric_limits<int>::max() / 4;
        }
    }
    queue.push(adjusted, field.isTrickRoom());
}

void Battle::processTurn() {
    beginTurn();
    
    // 输出回合开始的Json
    json turnStartJson = BattleToJson::battleToJson(*this);
    turnStartJson["event"] = "turn_start";
    
    // 写入到cache文件
    BattleToJson::writeToCache(turnStartJson, "battle_turn_start_" + std::to_string(turnNumber) + ".json");
    
    while (!queue.empty()) {
        resolveNextAction();
    }

    endTurn();
    
    // 输出回合结束的Json
    json turnEndJson = BattleToJson::battleToJson(*this);
    turnEndJson["event"] = "turn_end";
    
    // 写入到cache文件
    BattleToJson::writeToCache(turnEndJson, "battle_turn_end_" + std::to_string(turnNumber) + ".json");
    
}

void Battle::resolveNextAction() {
    BattleAction action = queue.pop();
    const bool canEmitActionEvents = action.actor != nullptr;
    ScopedActionEventEmitter eventScope{&eventSystem, turnNumber, &action, canEmitActionEvents};
    if (canEmitActionEvents) {
        eventSystem.emitActionEvent(RuntimeActionEvent{
            RuntimeEventPhase::BeforeActionResolve,
            turnNumber,
            action,
        });
    }

    if (action.actor == nullptr) {
        return;
    }
    
    // 输出行动开始的Json
    json actionStartJson = BattleToJson::actionToJson(action);
    actionStartJson["event"] = "action_start";
    
    // 写入到cache文件
    BattleToJson::writeToCache(actionStartJson, "battle_action_start_" + std::to_string(turnNumber) + "_" + actionStartJson["type"].get<std::string>() + ".json");

    switch (action.type) {
        case ActionType::Attack: {
            if (!action.target || action.actor->isFainted() || action.target->isFainted()) {
                return;
            }

            const auto infatuationIt = runtimeMoveState.infatuationSource.find(action.actor);
            if (infatuationIt != runtimeMoveState.infatuationSource.end()) {
                Pokemon* source = infatuationIt->second;
                const Side* actorSide = findSideForPokemon(*this, action.actor);
                const Side* sourceSide = findSideForPokemon(*this, source);
                const bool infatuated = source && !source->isFainted() && actorSide && sourceSide && actorSide != sourceSide;
                if (infatuated && PRNG::nextFloat(0.0f, 1.0f) < 0.5f) {
                    break;
                }
                if (!infatuated) {
                    runtimeMoveState.infatuationSource.erase(infatuationIt);
                }
            }

            if (action.actor->hasStatus(StatusType::Sleep)) {
                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["missed"] = true;
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (action.actor->hasStatus(StatusType::Flinch)) {
                action.actor->tickStatusDuration(StatusType::Flinch);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["flinched"] = true;
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }
            
            // 触发攻击前的效果
            triggerAbilities(Trigger::OnAttack, action.actor);
            triggerItemEffects(ItemTrigger::OnAttack, action.actor);
            
            // 计算伤害
            Move selectedMove = applyEncoreOverride(action.actor, action.move);
            if (isMoveDisabledForActor(action.actor, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "disable";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (isMoveBlockedByTaunt(action.actor, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "taunt";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (isMoveBlockedByTorment(action.actor, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "torment";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (isMoveBlockedByImprison(action.actor, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "imprison";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            const Side* attackerSide = findSideForPokemon(*this, action.actor);
            const Side* defenderSide = findSideForPokemon(*this, action.target);
            const bool isOpponentTargetingStatusMove = selectedMove.getCategory() == Category::Status
                && attackerSide && defenderSide && attackerSide != defenderSide
                && (selectedMove.getTarget() == Target::Opponent
                    || selectedMove.getTarget() == Target::AllOpponents
                    || selectedMove.getTarget() == Target::All);
            if (isOpponentTargetingStatusMove
                && !abilityIgnoresTargetAbility(action.actor->getAbility())
                && abilityBlocksStatusMovesFromOpponents(action.target->getAbility())) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "good_as_gold";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (isMoveBlockedByHealBlock(action.actor, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "heal_block";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (isMoveBlockedByQuickGuard(action.actor, action.target, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "quick_guard";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (isMoveBlockedByArmorTail(action.actor, action.target, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "armor_tail";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (isMoveBlockedByWideGuard(action.actor, action.target, selectedMove)) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                attackResultJson["blocked"] = "wide_guard";
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (handleTwoTurnChargeTurn(action.actor, selectedMove)) {
                return;
            }

            const AbilityType attackerAbility = action.actor->getAbility();
            if (abilityCanTypeShift(attackerAbility)
                && !runtimeMoveState.typeShiftUsed[action.actor]) {
                action.actor->setTypes(selectedMove.getType(), Type::Count);
                runtimeMoveState.typeShiftUsed[action.actor] = true;
            }

            const bool ignoresSubstitute = abilityIgnoresSubstitute(attackerAbility);
            const bool blockedBySubstitute = !ignoresSubstitute
                && action.target->getSubstituteHP() > 0
                && selectedMove.getCategory() == Category::Status
                && (selectedMove.getTarget() == Target::Opponent
                    || selectedMove.getTarget() == Target::AllOpponents
                    || selectedMove.getTarget() == Target::All);
            bool forceHitByLockOn = false;
            const auto lockOnIt = runtimeMoveState.lockOnState.find(action.actor);
            if (lockOnIt != runtimeMoveState.lockOnState.end()) {
                const LockOnState& lockOnState = lockOnIt->second;
                if (lockOnState.target == action.target && lockOnState.remainingTurns > 0) {
                    forceHitByLockOn = true;
                    runtimeMoveState.lockOnState.erase(lockOnIt);
                }
            }

            const bool ignoreTargetEvasion = ignoresTargetEvasionForMove(selectedMove, action.target);
            const bool gravityActive = runtimeMoveState.gravityTurns > 0;
            if (!blockedBySubstitute && !forceHitByLockOn && !doesMoveHit(action.actor, action.target, selectedMove, ignoreTargetEvasion, gravityActive)) {
                applyCrashRecoil(action.actor, selectedMove);
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            if (blockedBySubstitute) {
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = action.move.getName();
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
                break;
            }

            const auto multiHitConfig = getMultiHitConfig(selectedMove);
            if (multiHitConfig.has_value()) {
                bool dealtDamage = false;
                int landedHits = 0;
                const int maxHits = rollHitCount(*multiHitConfig);

                for (int hitIndex = 0; hitIndex < maxHits; ++hitIndex) {
                    if (!forceHitByLockOn && !doesMoveHit(action.actor, action.target, selectedMove, ignoreTargetEvasion, gravityActive)) {
                        if (multiHitConfig->stopOnMiss) {
                            break;
                        }
                        break;
                    }

                    ++landedHits;
                    int damage = calculateDamage(action.actor, action.target, selectedMove);
                    ItemDamageContext damageContext;
                    damageContext.move = &selectedMove;
                    damageContext.damage = damage;
                    damageContext.hpBeforeDamage = action.target->getCurrentHP();
                    damageContext.wasSuperEffective = action.target->getTypeEffectiveness(selectedMove.getType()) > 1.0f;
                    damageContext.isDamagingMove = true;
                    damageContext.isContact = selectedMove.getCategory() == Category::Physical;

                    AbilityDamageContext abilityDamageContext;
                    abilityDamageContext.isDamagingMove = damageContext.isDamagingMove;
                    abilityDamageContext.isContact = damageContext.isContact;
                    abilityDamageContext.move = &selectedMove;
                    abilityDamageContext.battle = this;

                    if (!ignoresSubstitute && action.target->getSubstituteHP() > 0 && damage > 0) {
                        const int remainingSubstituteHp = action.target->getSubstituteHP() - damage;
                        if (remainingSubstituteHp > 0) {
                            action.target->setSubstituteHP(remainingSubstituteHp);
                        } else {
                            action.target->clearSubstitute();
                        }
                        damage = 0;
                    }

                    const auto endureIt = runtimeMoveState.endureActive.find(action.target);
                    const bool endureActive = endureIt != runtimeMoveState.endureActive.end() && endureIt->second;
                    if (endureActive && damage >= action.target->getCurrentHP() && action.target->getCurrentHP() > 1) {
                        damage = action.target->getCurrentHP() - 1;
                    }

                    action.target->setCurrentHP(action.target->getCurrentHP() - damage);

                    if (damage > 0) {
                        dealtDamage = true;
                        triggerAbility(action.target, Trigger::OnDamage, action.actor, &abilityDamageContext);
                        triggerItemEffect(action.target, ItemTrigger::OnDamage, action.actor, &damageContext);

                        triggerAbilities(Trigger::OnDealDamage, action.actor);
                        triggerItemEffect(action.actor, ItemTrigger::OnDealDamage, action.target, &damageContext);

                        if (action.target->isFainted()) {
                            const auto destinyIt = runtimeMoveState.destinyBondActive.find(action.target);
                            if (destinyIt != runtimeMoveState.destinyBondActive.end() && destinyIt->second) {
                                action.actor->setCurrentHP(0);
                                runtimeMoveState.destinyBondActive.erase(destinyIt);
                            }
                            const auto grudgeIt = runtimeMoveState.grudgeActive.find(action.target);
                            if (grudgeIt != runtimeMoveState.grudgeActive.end() && grudgeIt->second) {
                                action.actor->reduceMovePPByName(selectedMove.getName(), 999);
                                runtimeMoveState.grudgeActive.erase(grudgeIt);
                            }
                            triggerAbility(action.actor, Trigger::OnFaint, action.target);
                            Side* targetSide = findSideForPokemon(*this, action.target);
                            if (targetSide) {
                                targetSide->resetProtectCount();
                                if (targetSide->autoSwitchNext()) {
                                    applyEntryHazardsOnSwitchIn(targetSide, targetSide->getActivePokemon());
                                    applyPendingSwitchInRecovery(*targetSide, targetSide->getActivePokemon());
                                }
                            }
                            break;
                        }
                    }
                }

                if (landedHits > 0 && !abilitySuppressesSecondaryEffects(attackerAbility, selectedMove, isSheerForceBoostedMove(selectedMove))) {
                    processMoveEffects(action.actor, action.target, selectedMove);
                }
                recordExecutedMove(action.actor, selectedMove);

                json attackResultJson = BattleToJson::battleToJson(*this);
                attackResultJson["event"] = "attack_result";
                attackResultJson["actor"] = action.actor->getName();
                attackResultJson["target"] = action.target->getName();
                attackResultJson["move"] = selectedMove.getName();
                attackResultJson["hit_count"] = landedHits;
                attackResultJson["dealt_damage"] = dealtDamage;
                BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + selectedMove.getName() + ".json");
                break;
            }

            int damage = calculateDamage(action.actor, action.target, selectedMove);
            ItemDamageContext damageContext;
            damageContext.move = &selectedMove;
            damageContext.damage = damage;
            damageContext.hpBeforeDamage = action.target->getCurrentHP();
            damageContext.wasSuperEffective = action.target->getTypeEffectiveness(selectedMove.getType()) > 1.0f;
            damageContext.isDamagingMove = selectedMove.getCategory() != Category::Status;
            damageContext.isContact = selectedMove.getCategory() == Category::Physical;

            AbilityDamageContext abilityDamageContext;
            abilityDamageContext.isDamagingMove = damageContext.isDamagingMove;
            abilityDamageContext.isContact = damageContext.isContact;
            abilityDamageContext.move = &selectedMove;
            abilityDamageContext.battle = this;

            if (!ignoresSubstitute && action.target->getSubstituteHP() > 0 && damage > 0) {
                const int remainingSubstituteHp = action.target->getSubstituteHP() - damage;
                if (remainingSubstituteHp > 0) {
                    action.target->setSubstituteHP(remainingSubstituteHp);
                } else {
                    action.target->clearSubstitute();
                }
                damage = 0;
            }

            const auto endureIt = runtimeMoveState.endureActive.find(action.target);
            const bool endureActive = endureIt != runtimeMoveState.endureActive.end() && endureIt->second;
            if (endureActive && damage >= action.target->getCurrentHP() && action.target->getCurrentHP() > 1) {
                damage = action.target->getCurrentHP() - 1;
            }
            
            // 输出攻击信息
            // 应用伤害
            action.target->setCurrentHP(action.target->getCurrentHP() - damage);
            
            if (damage > 0) {
                // 触发受到伤害时的效果
                triggerAbility(action.target, Trigger::OnDamage, action.actor, &abilityDamageContext);
                triggerItemEffect(action.target, ItemTrigger::OnDamage, action.actor, &damageContext);

                // 检查目标是否濒死
                if (action.target->isFainted()) {
                    const auto destinyIt = runtimeMoveState.destinyBondActive.find(action.target);
                    if (destinyIt != runtimeMoveState.destinyBondActive.end() && destinyIt->second) {
                        action.actor->setCurrentHP(0);
                        runtimeMoveState.destinyBondActive.erase(destinyIt);
                    }
                    const auto grudgeIt = runtimeMoveState.grudgeActive.find(action.target);
                    if (grudgeIt != runtimeMoveState.grudgeActive.end() && grudgeIt->second) {
                        action.actor->reduceMovePPByName(selectedMove.getName(), 999);
                        runtimeMoveState.grudgeActive.erase(grudgeIt);
                    }
                    triggerAbility(action.actor, Trigger::OnFaint, action.target);
                    Side* targetSide = findSideForPokemon(*this, action.target);
                    if (targetSide) {
                        // 宝可梦死亡时重置保护计数
                        targetSide->resetProtectCount();
                        if (targetSide->autoSwitchNext()) {
                            applyEntryHazardsOnSwitchIn(targetSide, targetSide->getActivePokemon());
                            applyPendingSwitchInRecovery(*targetSide, targetSide->getActivePokemon());
                        }
                    }
                }
            }
            
            // 处理技能追加效果
            if (!abilitySuppressesSecondaryEffects(attackerAbility, selectedMove, isSheerForceBoostedMove(selectedMove))) {
                processMoveEffects(action.actor, action.target, selectedMove);
            }
            
            // 触发攻击后的效果
            if (damage > 0) {
                triggerAbilities(Trigger::OnDealDamage, action.actor);
                triggerItemEffect(action.actor, ItemTrigger::OnDealDamage, action.target, &damageContext);
            }
            recordExecutedMove(action.actor, selectedMove);
            
            // 输出攻击结果的Json
            json attackResultJson = BattleToJson::battleToJson(*this);
            attackResultJson["event"] = "attack_result";
            attackResultJson["actor"] = action.actor->getName();
            attackResultJson["target"] = action.target->getName();
            attackResultJson["move"] = action.move.getName();
            
            // 写入到cache文件
            BattleToJson::writeToCache(attackResultJson, "battle_attack_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + "_" + action.move.getName() + ".json");
            
            break;
        }
        case ActionType::Switch: {
            Side* side = findSideForPokemon(*this, action.actor);
            if (side) {
                const auto ingrainIt = runtimeMoveState.ingrainActive.find(action.actor);
                if (ingrainIt != runtimeMoveState.ingrainActive.end() && ingrainIt->second) {
                    break;
                }

                const auto trappedIt = runtimeMoveState.trappedBySource.find(action.actor);
                if (trappedIt != runtimeMoveState.trappedBySource.end()) {
                    Pokemon* trappingSource = trappedIt->second;
                    const Side* actorSide = findSideForPokemon(*this, action.actor);
                    const Side* sourceSide = findSideForPokemon(*this, trappingSource);
                    const bool stillTrapped = trappingSource && !trappingSource->isFainted()
                        && actorSide && sourceSide && actorSide != sourceSide;
                    if (stillTrapped) {
                        break;
                    }
                    runtimeMoveState.trappedBySource.erase(trappedIt);
                }

                Pokemon* oldPokemon = action.actor;
                handlePursuitOnSwitch(oldPokemon, side);
                triggerItemEffects(ItemTrigger::OnSwitchOut, action.actor);
                
                // 执行切换
                bool switched = switchPokemon(*side, action.switchIndex);
                
                if (switched) {
                    Pokemon* newPokemon = side->getActivePokemon();
                    
                    // 触发出场时的效果
                    // 获取对手的活跃宝可梦
                    Pokemon* opponentPokemon = (side == &sideA) ? sideB.getActivePokemon() : sideA.getActivePokemon();
                    triggerAbility(newPokemon, Trigger::OnEntry, opponentPokemon);
                    triggerItemEffect(newPokemon, ItemTrigger::OnEntry, opponentPokemon);
                    
                    // 输出切换结果的Json
                    json switchResultJson = BattleToJson::battleToJson(*this);
                    switchResultJson["event"] = "switch_result";
                    switchResultJson["old_pokemon"] = oldPokemon->getName();
                    switchResultJson["new_pokemon"] = newPokemon->getName();
                    
                    // 写入到cache文件
                    BattleToJson::writeToCache(switchResultJson, "battle_switch_result_" + std::to_string(turnNumber) + "_" + oldPokemon->getName() + "_" + newPokemon->getName() + ".json");
                }
            }
            break;
        }
        case ActionType::UseItem: {
            if (!action.actor) {
                break;
            }
            if (isItemUsageBlockedByEmbargo(action.actor)) {
                appendSpecialEvent(*this, "item_blocked", {
                    {"actor", action.actor->getName()},
                    {"item", BattleToJson::itemTypeToString(action.item)},
                    {"reason", "embargo"}
                });
                break;
            }
            Item held = action.actor->getHeldItem();
            if (held.type == action.item) {
                const Side* actorSide = findSideForPokemon(*this, action.actor);
                Pokemon* opponentActive = nullptr;
                if (actorSide == &sideA) {
                    opponentActive = sideB.getActivePokemon();
                } else if (actorSide == &sideB) {
                    opponentActive = sideA.getActivePokemon();
                }

                if (isBerryItem(action.item)
                    && opponentActive
                    && !opponentActive->isFainted()
                    && abilityBlocksBerryConsumption(opponentActive->getAbility())) {
                    appendSpecialEvent(*this, "item_blocked", {
                        {"actor", action.actor->getName()},
                        {"item", BattleToJson::itemTypeToString(action.item)},
                        {"reason", "unnerve"},
                        {"opponent", opponentActive->getName()}
                    });
                    break;
                }

                held.executeTrigger(ItemTrigger::OnEat, action.actor, action.target, *this);
                
                // 输出使用物品结果的Json
                json itemResultJson = BattleToJson::battleToJson(*this);
                itemResultJson["event"] = "item_result";
                itemResultJson["actor"] = action.actor->getName();
                itemResultJson["item"] = BattleToJson::itemTypeToString(action.item);
                
                // 写入到cache文件
                BattleToJson::writeToCache(itemResultJson, "battle_item_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + ".json");
            }
            break;
        }
        case ActionType::Pass: {
            
            // 输出跳过结果的Json
            json passResultJson = BattleToJson::battleToJson(*this);
            passResultJson["event"] = "pass_result";
            passResultJson["actor"] = action.actor->getName();
            
            // 写入到cache文件
            BattleToJson::writeToCache(passResultJson, "battle_pass_result_" + std::to_string(turnNumber) + "_" + action.actor->getName() + ".json");
            break;
        }
        default:
            break;
    }
}

int Battle::calculateDamage(Pokemon* attacker, Pokemon* defender, const Move& move) const {
    if (!attacker || !defender) {
        return 0;
    }

    // 幽灵突袭类可无视保护。
    if (defender->getIsProtected() && !moveNameEquals(move, "phantomforce") && !moveNameEquals(move, "shadowforce")) {
        if (move.getCategory() == Category::Physical) {
            const auto variantIt = runtimeMoveState.protectionVariant.find(defender);
            const std::string variant = (variantIt != runtimeMoveState.protectionVariant.end()) ? variantIt->second : std::string();
            if (variant == "spikyshield") {
                attacker->setCurrentHP(attacker->getCurrentHP() - std::max(1, attacker->getMaxHP() / 8));
            } else if (variant == "kingsshield" || variant == "obstruct") {
                attacker->changeStatStage(StatIndex::Attack, -2);
            } else if (variant == "banefulbunker") {
                attacker->addStatus(StatusType::Poison);
            } else if (variant == "burningbulwark") {
                attacker->addStatus(StatusType::Burn);
            }
        }
        return 0;
    }

    const AbilityType atkAbility = attacker->getAbility();
    const AbilityType defAbility = defender->getAbility();
    const bool ignoreDefenderAbility = abilityIgnoresTargetAbility(atkAbility);
    const AbilityType effectiveDefAbility = ignoreDefenderAbility ? AbilityType::None : defAbility;
    const Pokemon* activeA = sideA.getActivePokemon();
    const Pokemon* activeB = sideB.getActivePokemon();
    const bool beadsOfRuinActive = (activeA && !activeA->isFainted() && abilityLowersOpponentSpecialDefenseAura(activeA->getAbility()))
        || (activeB && !activeB->isFainted() && abilityLowersOpponentSpecialDefenseAura(activeB->getAbility()));
    const bool swordOfRuinActive = (activeA && !activeA->isFainted() && abilityLowersOpponentDefenseAura(activeA->getAbility()))
        || (activeB && !activeB->isFainted() && abilityLowersOpponentDefenseAura(activeB->getAbility()));
    const bool tabletsOfRuinActive = (activeA && !activeA->isFainted() && abilityLowersOpponentPhysicalAttackAura(activeA->getAbility()))
        || (activeB && !activeB->isFainted() && abilityLowersOpponentPhysicalAttackAura(activeB->getAbility()));
    const bool vesselOfRuinActive = (activeA && !activeA->isFainted() && abilityLowersOpponentSpecialAttackAura(activeA->getAbility()))
        || (activeB && !activeB->isFainted() && abilityLowersOpponentSpecialAttackAura(activeB->getAbility()));
    const bool weatherSuppressed = (activeA && !activeA->isFainted() && abilitySuppressesWeather(activeA->getAbility()))
        || (activeB && !activeB->isFainted() && abilitySuppressesWeather(activeB->getAbility()));

    const SemiInvulnerableState defenderState = getSemiInvulnerableState(defender);
    if (defenderState != SemiInvulnerableState::None) {
        bool canHitSemiInvulnerableTarget = false;
        switch (defenderState) {
            case SemiInvulnerableState::Underground:
                canHitSemiInvulnerableTarget = moveNameEquals(move, "earthquake") || moveNameEquals(move, "magnitude");
                break;
            case SemiInvulnerableState::Airborne:
                canHitSemiInvulnerableTarget = moveNameEquals(move, "gust") || moveNameEquals(move, "twister")
                    || moveNameEquals(move, "thunder") || moveNameEquals(move, "hurricane");
                break;
            case SemiInvulnerableState::Underwater:
                canHitSemiInvulnerableTarget = moveNameEquals(move, "surf") || moveNameEquals(move, "whirlpool");
                break;
            case SemiInvulnerableState::Phased:
                canHitSemiInvulnerableTarget = false;
                break;
            case SemiInvulnerableState::None:
                canHitSemiInvulnerableTarget = true;
                break;
        }

        if (!canHitSemiInvulnerableTarget) {
            return 0;
        }
    }

    bool healInstead = false;
    int healPercent = 0;
    if (abilityBlocksMoveDamage(effectiveDefAbility, move)) {
        applyTypeImmunityBonus(effectiveDefAbility, defender);
        return 0;
    }
    if (resolveTypeImmunity(effectiveDefAbility, move.getType(), healInstead, healPercent)) {
        if (healInstead && healPercent > 0) {
            const int beforeHp = defender->getCurrentHP();
            const int healAmount = std::max(1, defender->getMaxHP() * healPercent / 100);
            defender->setCurrentHP(defender->getCurrentHP() + healAmount);
            const int afterHp = defender->getCurrentHP();
            if (afterHp > beforeHp) {
                appendSpecialEvent(*this, "heal", {
                    {"pokemon", defender->getName()},
                    {"reason", abilityTypeImmunityEventReason(effectiveDefAbility)},
                    {"from", beforeHp},
                    {"to", afterHp},
                    {"delta", afterHp - beforeHp}
                });
            }
        }
        applyTypeImmunityBonus(effectiveDefAbility, defender);
        return 0;
    }

    auto stageMultiplier = [](int stage) -> float {
        if (stage >= 0) {
            return static_cast<float>(2 + stage) / 2.0f;
        }
        return 2.0f / static_cast<float>(2 - stage);
    };

    float attackStat = (move.getCategory() == Category::Physical) ? attacker->getAttack() : attacker->getSpecialAttack();
    float defenseStat = (move.getCategory() == Category::Physical) ? defender->getDefense() : defender->getSpecialDefense();

    if (move.getCategory() == Category::Physical) {
        const int atkStage = abilityIgnoresOpponentStatStages(effectiveDefAbility) ? 0 : attacker->getStatStage(StatIndex::Attack);
        const int defStage = abilityIgnoresOpponentStatStages(atkAbility) ? 0 : defender->getStatStage(StatIndex::Defense);
        attackStat *= stageMultiplier(atkStage);
        defenseStat *= stageMultiplier(defStage);
        if (tabletsOfRuinActive && !abilityLowersOpponentPhysicalAttackAura(atkAbility)) {
            attackStat *= 0.75f;
        }
        if (swordOfRuinActive && !abilityLowersOpponentDefenseAura(effectiveDefAbility)) {
            defenseStat *= 0.75f;
        }
    } else if (move.getCategory() == Category::Special) {
        const int spaStage = abilityIgnoresOpponentStatStages(effectiveDefAbility) ? 0 : attacker->getStatStage(StatIndex::SpecialAttack);
        const int spdStage = abilityIgnoresOpponentStatStages(atkAbility) ? 0 : defender->getStatStage(StatIndex::SpecialDefense);
        attackStat *= stageMultiplier(spaStage);
        defenseStat *= stageMultiplier(spdStage);
        if (vesselOfRuinActive && !abilityLowersOpponentSpecialAttackAura(atkAbility)) {
            attackStat *= 0.75f;
        }
        if (beadsOfRuinActive && !abilityLowersOpponentSpecialDefenseAura(effectiveDefAbility)) {
            defenseStat *= 0.75f;
        }
    }

    attackStat *= abilityAttackStatMultiplier(
        atkAbility,
        move,
        hasMajorStatus(attacker),
        field.isActive() && field.type == FieldType::Electric);
    defenseStat *= abilityDefenseStatMultiplier(effectiveDefAbility, move, hasMajorStatus(defender));

    attackStat *= abilityParadoxStatMultiplier(
        atkAbility,
        attacker,
        move.getCategory() == Category::Physical ? StatIndex::Attack : StatIndex::SpecialAttack,
        weather.type,
        field.type);
    defenseStat *= abilityParadoxStatMultiplier(
        effectiveDefAbility,
        defender,
        move.getCategory() == Category::Physical ? StatIndex::Defense : StatIndex::SpecialDefense,
        weather.type,
        field.type);

    if (defenseStat <= 0.0f) {
        defenseStat = 1.0f;
    }

    Type effectiveType = move.getType();
    int movePower = move.getPower();
    if (moveNameEquals(move, "heavyslam")) {
        movePower = heavySlamPowerByWeightRatio(attacker->getSpecies().weight, defender->getSpecies().weight);
    }
    if (moveNameEquals(move, "weatherball")) {
        effectiveType = weatherSuppressed ? Type::Normal : weatherBallType(weather);
        if (!weatherSuppressed && weather.isActive()) {
            movePower *= 2;
        }
    }
    if (moveNameEquals(move, "round") && runtimeMoveState.roundUsedThisTurn) {
        movePower *= 2;
    }

    movePower = applyAbilityPowerModifier(atkAbility, move, movePower, isSheerForceBoostedMove(move));

    float base = ((2.0f * attacker->getLevel() / 5.0f + 2.0f) * movePower * attackStat / defenseStat) / 50.0f + 2.0f;
    const float typeEffectiveness = adjustedTypeEffectivenessForMove(defender, effectiveType);
    float modifier = typeEffectiveness;
    
    // 应用天气加成并输出日志
    float weatherModifier = weatherSuppressed ? 1.0f : weather.applyDamageModifier(effectiveType);
    modifier *= weatherModifier;
    
    float damage = base * modifier;

    const bool isStab = effectiveType == attacker->getType1() || effectiveType == attacker->getType2();
    if (isStab) {
        damage *= abilityStabBonusMultiplier(atkAbility);
    }

    damage *= abilityIncomingDamageMultiplier(
        effectiveDefAbility,
        move,
        typeEffectiveness,
        defender->getCurrentHP(),
        defender->getMaxHP());
    const auto switchedInIt = runtimeMoveState.switchedInTurn.find(defender);
    const bool targetJustSwitchedIn = switchedInIt != runtimeMoveState.switchedInTurn.end()
        && switchedInIt->second == turnNumber;
    int faintedAllies = 0;
    if (const Side* attackerSide = findSideForPokemon(*this, attacker)) {
        for (Pokemon* teammate : attackerSide->getTeam()) {
            if (!teammate || teammate == attacker) {
                continue;
            }
            if (teammate->isFainted()) {
                ++faintedAllies;
            }
        }
    }
    damage *= abilityOutgoingDamageMultiplier(
        atkAbility,
        move,
        attacker->getCurrentHP(),
        attacker->getMaxHP(),
        targetJustSwitchedIn,
        faintedAllies);
    if (moveNameEquals(move, "knockoff") && defender->getItemType() != ItemType::None) {
        damage *= 1.5f;
    }
    if (moveNameEquals(move, "pursuit") && defender == runtimeMoveState.pursuitSwitchTarget) {
        damage *= 2.0f;
    }

    const auto critStageIt = runtimeMoveState.criticalHitStage.find(attacker);
    const int focusCritStage = (critStageIt != runtimeMoveState.criticalHitStage.end()) ? critStageIt->second : 0;
    const int totalCritStage = std::min(4, focusCritStage + (isHighCriticalMove(move) ? 1 : 0));
    if (PRNG::nextFloat() < criticalHitChanceForStage(totalCritStage)) {
        damage *= 1.5f;
    }

    if (defenderState == SemiInvulnerableState::Underground
        && (moveNameEquals(move, "earthquake") || moveNameEquals(move, "magnitude"))) {
        damage *= 2.0f;
    }
    if (defenderState == SemiInvulnerableState::Airborne
        && (moveNameEquals(move, "gust") || moveNameEquals(move, "twister"))) {
        damage *= 2.0f;
    }
    if (defenderState == SemiInvulnerableState::Underwater
        && (moveNameEquals(move, "surf") || moveNameEquals(move, "whirlpool"))) {
        damage *= 2.0f;
    }

    const FieldType fieldType = field.type;
    switch (fieldType) {
        case FieldType::Psychic:
            if (move.getCategory() == Category::Physical) {
                damage *= 0.5f;
            }
            break;
        case FieldType::Electric:
            if (move.getType() == Type::Electric) {
                damage *= 1.5f;
            }
            break;
        case FieldType::Misty:
            if (move.getType() == Type::Dragon) {
                damage *= 0.5f;
            }
            break;
        default:
            break;
    }

    const Side* defenderSide = findSideForPokemon(*this, defender);
    if (defenderSide) {
        if (defenderSide->hasReflect() && move.getCategory() == Category::Physical && !abilityIgnoresScreens(atkAbility)) {
            damage *= 0.5f;
        }
        if (defenderSide->hasLightScreen() && move.getCategory() == Category::Special && !abilityIgnoresScreens(atkAbility)) {
            damage *= 0.5f;
        }
    }

    const Side* attackerSide = findSideForPokemon(*this, attacker);
    if (attackerSide) {
        if (attackerSide->hasMudSport() && effectiveType == Type::Electric) {
            damage /= 3.0f;
        }
        if (attackerSide->hasWaterSport() && effectiveType == Type::Fire) {
            damage /= 3.0f;
        }
    }

    // 应用攻击方/防守方道具的伤害修正
    const Item attackerItem = attacker->getHeldItem();
    damage = attackerItem.applyDamageModifier(damage, attacker, defender, move, true);
    const Item defenderItem = defender->getHeldItem();
    damage = defenderItem.applyDamageModifier(damage, defender, attacker, move, false);

    // 添加随机数影响，伤害在85%到100%之间波动
    float randomFactor = PRNG::nextFloat(0.85f, 1.0f);
    int finalDamage = static_cast<int>(std::lround(damage * randomFactor));
    if (finalDamage < 0) {
        finalDamage = 0;
    }
    return finalDamage;
}

bool Battle::switchPokemon(Side& side, int newIndex) {
    Pokemon* oldActive = side.getActivePokemon();
    if (oldActive && !oldActive->isFainted()) {
        const int hpBeforeExit = oldActive->getCurrentHP();
        triggerAbility(oldActive, Trigger::OnExit, nullptr);
        const int hpAfterExit = oldActive->getCurrentHP();
        if (hpAfterExit > hpBeforeExit) {
            appendSpecialEvent(*this, "heal", {
                {"pokemon", oldActive->getName()},
                {"reason", "regenerator"},
                {"from", hpBeforeExit},
                {"to", hpAfterExit},
                {"delta", hpAfterExit - hpBeforeExit}
            });
        }
    }

    bool switched = side.switchActive(newIndex);
    if (switched) {
        appendSpecialEvent(*this, "switch", {
            {"side", side.getName()},
            {"from", oldActive ? oldActive->getName() : ""},
            {"to", side.getActivePokemon() ? side.getActivePokemon()->getName() : ""}
        });
        if (oldActive) {
            oldActive->resetTypesToSpecies();
        }

        Pokemon* newActive = side.getActivePokemon();
        if (newActive) {
            runtimeMoveState.switchedInTurn[newActive] = turnNumber;
        }
        applyEntryHazardsOnSwitchIn(&side, newActive);
        applyPendingSwitchInRecovery(side, newActive);
        clearPokemonRuntimeState(oldActive);
        clearVolatileSwitchState(oldActive);
        // 切换宝可梦时重置保护计数
        side.resetProtectCount();
    }
    return switched;
}

void Battle::applyEntryHazardsOnSwitchIn(Side* enteringSide, Pokemon* enteringPokemon) {
    if (!enteringSide || !enteringPokemon || enteringPokemon->isFainted()) {
        return;
    }

    if (enteringSide->hasStealthRock()) {
        const float effectiveness = enteringPokemon->getTypeEffectiveness(Type::Rock);
        const int stealthRockDamage = std::max(1, static_cast<int>(std::lround(enteringPokemon->getMaxHP() * 0.125f * effectiveness)));
        enteringPokemon->setCurrentHP(enteringPokemon->getCurrentHP() - stealthRockDamage);
    }

    const bool isFlyingType = enteringPokemon->getType1() == Type::Flying || enteringPokemon->getType2() == Type::Flying;
    const bool grounded = !isFlyingType && !abilityGrantsGroundHazardImmunity(enteringPokemon->getAbility());

    if (grounded && enteringSide->getSpikesLayers() > 0) {
        float spikesFactor = 0.125f;
        if (enteringSide->getSpikesLayers() == 2) {
            spikesFactor = 1.0f / 6.0f;
        } else if (enteringSide->getSpikesLayers() >= 3) {
            spikesFactor = 0.25f;
        }
        const int spikesDamage = std::max(1, static_cast<int>(std::lround(enteringPokemon->getMaxHP() * spikesFactor)));
        enteringPokemon->setCurrentHP(enteringPokemon->getCurrentHP() - spikesDamage);
    }

    if (grounded && enteringSide->getToxicSpikesLayers() > 0) {
        const bool poisonType = enteringPokemon->getType1() == Type::Poison || enteringPokemon->getType2() == Type::Poison;
        const bool steelType = enteringPokemon->getType1() == Type::Steel || enteringPokemon->getType2() == Type::Steel;
        if (!poisonType && !steelType) {
            const auto beforeStatuses = snapshotStatuses(enteringPokemon);
            if (enteringSide->getToxicSpikesLayers() >= 2) {
                enteringPokemon->addStatus(StatusType::ToxicPoison);
            } else {
                enteringPokemon->addStatus(StatusType::Poison);
            }
            const auto afterStatuses = snapshotStatuses(enteringPokemon);
            for (StatusType status : afterStatuses) {
                if (!hasStatusInSnapshot(beforeStatuses, status)) {
                    appendSpecialEvent(*this, "status_apply", {
                        {"pokemon", enteringPokemon->getName()},
                        {"status", BattleToJson::statusTypeToString(status)},
                        {"reason", "toxic_spikes"}
                    });
                }
            }
        }
    }
}

void Battle::processMoveEffects(Pokemon* attacker, Pokemon* defender, const Move& move) {
    if (!attacker || !defender) return;

    auto applyBatonPassSwitch = [&](Pokemon* source) {
        Side* actorSide = findSideForPokemon(*this, source);
        if (!actorSide || !actorSide->canSwitch()) {
            return;
        }

        const int atkStage = source->getStatStage(StatIndex::Attack);
        const int defStage = source->getStatStage(StatIndex::Defense);
        const int spaStage = source->getStatStage(StatIndex::SpecialAttack);
        const int spdStage = source->getStatStage(StatIndex::SpecialDefense);
        const int speStage = source->getStatStage(StatIndex::Speed);

        if (!actorSide->autoSwitchNext()) {
            return;
        }

        Pokemon* newActive = actorSide->getActivePokemon();
        if (!newActive || newActive == source) {
            return;
        }
        runtimeMoveState.switchedInTurn[newActive] = turnNumber;

        const int newAtk = newActive->getStatStage(StatIndex::Attack);
        const int newDef = newActive->getStatStage(StatIndex::Defense);
        const int newSpa = newActive->getStatStage(StatIndex::SpecialAttack);
        const int newSpd = newActive->getStatStage(StatIndex::SpecialDefense);
        const int newSpe = newActive->getStatStage(StatIndex::Speed);

        newActive->changeStatStage(StatIndex::Attack, atkStage - newAtk);
        newActive->changeStatStage(StatIndex::Defense, defStage - newDef);
        newActive->changeStatStage(StatIndex::SpecialAttack, spaStage - newSpa);
        newActive->changeStatStage(StatIndex::SpecialDefense, spdStage - newSpd);
        newActive->changeStatStage(StatIndex::Speed, speStage - newSpe);

        Pokemon* opponentPokemon = (actorSide == &sideA) ? sideB.getActivePokemon() : sideA.getActivePokemon();
        applyEntryHazardsOnSwitchIn(actorSide, newActive);
        applyPendingSwitchInRecovery(*actorSide, newActive);
        triggerAbility(newActive, Trigger::OnEntry, opponentPokemon);
        triggerItemEffect(newActive, ItemTrigger::OnEntry, opponentPokemon);
    };

    if (moveRuleRegistry.apply(*this, attacker, defender, move)) {
        return;
    }

    if (moveNameEquals(move, "conversion")) {
        for (const Move& knownMove : attacker->getMoves()) {
            if (!knownMove.getName().empty() && knownMove.getType() != Type::Count) {
                attacker->setTypes(knownMove.getType(), Type::Count);
                return;
            }
        }
        return;
    }

    if (moveNameEquals(move, "conversion2")) {
        const auto lastMoveIt = runtimeMoveState.lastUsedMoveName.find(defender);
        if (lastMoveIt == runtimeMoveState.lastUsedMoveName.end() || lastMoveIt->second.empty()) {
            return;
        }

        const Move lastMove = createMoveByName(lastMoveIt->second);
        if (lastMove.getName().empty() || lastMove.getType() == Type::Count) {
            return;
        }

        for (int candidate = 0; candidate < static_cast<int>(Type::Count); ++candidate) {
            const Type candidateType = static_cast<Type>(candidate);
            if (candidateType == Type::Count) {
                continue;
            }
            const float effectiveness = Utils::TypeEffectiveness[static_cast<int>(lastMove.getType())][candidate];
            if (effectiveness <= 0.5f) {
                attacker->setTypes(candidateType, Type::Count);
                return;
            }
        }

        attacker->setTypes(Type::Normal, Type::Count);
        return;
    }

    if (moveNameEquals(move, "encore")) {
        const auto it = runtimeMoveState.lastUsedMoveName.find(defender);
        if (it != runtimeMoveState.lastUsedMoveName.end() && !it->second.empty()) {
            runtimeMoveState.encoreState[defender] = Battle::EncoreState{it->second, 3};
        }
        return;
    }

    if (moveNameEquals(move, "knockoff")) {
        if (defender->getItemType() != ItemType::None) {
            defender->removeItem();
        }
        return;
    }

    if (moveNameEquals(move, "batonpass")) {
        applyBatonPassSwitch(attacker);
        return;
    }

    if (moveNameEquals(move, "uturn")) {
        if (!attacker->isFainted()) {
            Side* actorSide = findSideForPokemon(*this, attacker);
            if (actorSide && actorSide->canSwitch()) {
                actorSide->autoSwitchNext();
                Pokemon* newActive = actorSide->getActivePokemon();
                if (newActive) {
                    runtimeMoveState.switchedInTurn[newActive] = turnNumber;
                }
                Pokemon* opponentPokemon = (actorSide == &sideA) ? sideB.getActivePokemon() : sideA.getActivePokemon();
                if (newActive && newActive != attacker) {
                    applyEntryHazardsOnSwitchIn(actorSide, newActive);
                    applyPendingSwitchInRecovery(*actorSide, newActive);
                    triggerAbility(newActive, Trigger::OnEntry, opponentPokemon);
                    triggerItemEffect(newActive, ItemTrigger::OnEntry, opponentPokemon);
                }
            }
        }
        return;
    }

    if (field.isActive() && field.type == FieldType::Misty && isStatusInflictingEffect(move.getEffect())) {
        return;
    }
    Side* defenderSide = findSideForPokemon(*this, defender);
    if (defenderSide && defenderSide->hasSafeguard() && isStatusInflictingEffect(move.getEffect())) {
        return;
    }
    const int attackerHpBefore = attacker->getCurrentHP();
    const int defenderHpBefore = defender->getCurrentHP();
    const auto attackerStatusesBefore = snapshotStatuses(attacker);
    const auto defenderStatusesBefore = snapshotStatuses(defender);

    MoveEffectHandlers::applyStandardMoveEffect(*this, attacker, defender, move);

    const int attackerHpAfter = attacker->getCurrentHP();
    const int defenderHpAfter = defender->getCurrentHP();
    const auto attackerStatusesAfter = snapshotStatuses(attacker);
    const auto defenderStatusesAfter = snapshotStatuses(defender);

    if (attackerHpAfter > attackerHpBefore) {
        appendSpecialEvent(*this, "heal", {
            {"pokemon", attacker->getName()},
            {"reason", "move_effect"},
            {"move", move.getName()},
            {"from", attackerHpBefore},
            {"to", attackerHpAfter},
            {"delta", attackerHpAfter - attackerHpBefore}
        });
    }
    if (defenderHpAfter > defenderHpBefore) {
        appendSpecialEvent(*this, "heal", {
            {"pokemon", defender->getName()},
            {"reason", "move_effect"},
            {"move", move.getName()},
            {"from", defenderHpBefore},
            {"to", defenderHpAfter},
            {"delta", defenderHpAfter - defenderHpBefore}
        });
    }

    for (StatusType status : attackerStatusesAfter) {
        if (!hasStatusInSnapshot(attackerStatusesBefore, status)) {
            appendSpecialEvent(*this, "status_apply", {
                {"pokemon", attacker->getName()},
                {"status", BattleToJson::statusTypeToString(status)},
                {"reason", "move_effect"},
                {"move", move.getName()}
            });
        }
    }
    for (StatusType status : defenderStatusesAfter) {
        if (!hasStatusInSnapshot(defenderStatusesBefore, status)) {
            appendSpecialEvent(*this, "status_apply", {
                {"pokemon", defender->getName()},
                {"status", BattleToJson::statusTypeToString(status)},
                {"reason", "move_effect"},
                {"move", move.getName()}
            });
        }
    }
}

void Battle::applyWeatherEffects() {
    if (!weather.isActive()) {
        return;
    }

    Pokemon* activeA = sideA.getActivePokemon();
    Pokemon* activeB = sideB.getActivePokemon();
    const bool weatherSuppressed = (activeA && !activeA->isFainted() && abilitySuppressesWeather(activeA->getAbility()))
        || (activeB && !activeB->isFainted() && abilitySuppressesWeather(activeB->getAbility()));
    if (weatherSuppressed) {
        return;
    }

    for (Side* side : {&sideA, &sideB}) {
        Pokemon* active = side->getActivePokemon();
        if (!active || active->isFainted()) {
            continue;
        }

        switch (weather.type) {
            case WeatherType::Sandstorm:
                // 沙尘暴：对非岩石、地面、钢属性的宝可梦造成伤害
                if (abilityIgnoresIndirectDamage(active->getAbility())) {
                    break;
                }
                if (active->getType1() != Type::Rock && active->getType1() != Type::Ground && active->getType1() != Type::Steel && 
                    active->getType2() != Type::Rock && active->getType2() != Type::Ground && active->getType2() != Type::Steel) {
                    int damage = std::max(1, active->getMaxHP() / 16);
                    active->setCurrentHP(active->getCurrentHP() - damage);
                }
                break;
            case WeatherType::Hail:
                // 冰雹：对非冰属性的宝可梦造成伤害
                if (abilityIgnoresIndirectDamage(active->getAbility())) {
                    break;
                }
                if (active->getType1() != Type::Ice && active->getType2() != Type::Ice) {
                    int damage = std::max(1, active->getMaxHP() / 16);
                    active->setCurrentHP(active->getCurrentHP() - damage);
                }
                break;
            case WeatherType::Snow:
                // 雪：对冰属性以外的宝可梦造成少量伤害
                if (abilityIgnoresIndirectDamage(active->getAbility())) {
                    break;
                }
                if (active->getType1() != Type::Ice && active->getType2() != Type::Ice) {
                    int damage = std::max(1, active->getMaxHP() / 32);
                    active->setCurrentHP(active->getCurrentHP() - damage);
                }
                break;
            case WeatherType::Rain:
                // 雨天：无直接伤害效果
                break;
            case WeatherType::Sun:
                // 晴天：无直接伤害效果
                break;
            default:
                break;
        }
    }
}

void Battle::applyFieldEffects() {
    for (Side* side : {&sideA, &sideB}) {
        Pokemon* active = side->getActivePokemon();
        if (!active || active->isFainted()) {
            side->tickScreenEffects();
            side->tickMudSportTurns();
            side->tickWaterSportTurns();
            continue;
        }

        Pokemon* seedSource = active->getLeechSeedSource();
        if (seedSource && !seedSource->isFainted()) {
            const int drain = std::max(1, active->getMaxHP() / 8);
            active->setCurrentHP(active->getCurrentHP() - drain);
            seedSource->setCurrentHP(seedSource->getCurrentHP() + drain);
        }

        side->tickScreenEffects();
        side->tickMistTurns();
        side->tickSafeguardTurns();
        side->tickMudSportTurns();
        side->tickWaterSportTurns();
    }

    if (!field.isActive()) {
        return;
    }

    for (Side* side : {&sideA, &sideB}) {
        Pokemon* active = side->getActivePokemon();
        if (!active || active->isFainted()) {
            continue;
        }

        switch (field.type) {
            case FieldType::Psychic: {
                // 精神场地：降低地面上宝可梦受到的物理伤害
                break;
            }
            case FieldType::Electric: {
                // 电气场地：提高地面上宝可梦的电属性技能威力
                break;
            }
            case FieldType::Grassy: {
                // 青草场地：地面上的宝可梦每回合恢复少量HP
                const int beforeHp = active->getCurrentHP();
                int grassHeal = std::max(1, active->getMaxHP() / 16);
                active->setCurrentHP(active->getCurrentHP() + grassHeal);
                const int afterHp = active->getCurrentHP();
                if (afterHp > beforeHp) {
                    appendSpecialEvent(*this, "heal", {
                        {"pokemon", active->getName()},
                        {"reason", "grassy_terrain"},
                        {"from", beforeHp},
                        {"to", afterHp},
                        {"delta", afterHp - beforeHp}
                    });
                }
                break;
            }
            case FieldType::Misty: {
                // 薄雾场地：降低地面上宝可梦受到的龙属性伤害，防止状态异常
                break;
            }
            case FieldType::TrickRoom: {
                // 戏法空间：速度慢的宝可梦先行动
                break;
            }
            default: {
                break;
            }
        }
    }
}

void Battle::triggerAbility(Pokemon* pokemon, Trigger trigger, Pokemon* opponent, void* context) {
    if (!pokemon) return;

    auto snapshotStages = [](Pokemon* target) {
        std::array<int, 5> stages = {0, 0, 0, 0, 0};
        if (!target) {
            return stages;
        }
        stages[0] = target->getStatStage(StatIndex::Attack);
        stages[1] = target->getStatStage(StatIndex::Defense);
        stages[2] = target->getStatStage(StatIndex::SpecialAttack);
        stages[3] = target->getStatStage(StatIndex::SpecialDefense);
        stages[4] = target->getStatStage(StatIndex::Speed);
        return stages;
    };

    auto statChanged = [](Pokemon* target, const std::array<int, 5>& before) {
        if (!target) {
            return false;
        }
        return target->getStatStage(StatIndex::Attack) != before[0]
            || target->getStatStage(StatIndex::Defense) != before[1]
            || target->getStatStage(StatIndex::SpecialAttack) != before[2]
            || target->getStatStage(StatIndex::SpecialDefense) != before[3]
            || target->getStatStage(StatIndex::Speed) != before[4];
    };

    const std::array<int, 5> selfBefore = snapshotStages(pokemon);
    const std::array<int, 5> opponentBefore = snapshotStages(opponent);
    
    AbilityType abilityType = pokemon->getAbility();
    Ability ability = getAbility(abilityType);

    if (ability.hasTrigger(trigger)) {
        void* triggerContext = context;
        if (!triggerContext && (trigger == Trigger::OnEntry || trigger == Trigger::OnExit)) {
            triggerContext = this;
        }
        appendSpecialEvent(*this, "ability_trigger", {
            {"pokemon", pokemon->getName()},
            {"ability", BattleToJson::abilityTypeToString(abilityType)},
            {"trigger", triggerToString(trigger)},
            {"opponent", opponent ? opponent->getName() : ""}
        });
        ability.executeTrigger(trigger, pokemon, opponent, triggerContext);
    }

    if (statChanged(pokemon, selfBefore)) {
        triggerItemEffect(pokemon, ItemTrigger::OnStatChange, opponent);
    }
    if (statChanged(opponent, opponentBefore)) {
        triggerItemEffect(opponent, ItemTrigger::OnStatChange, pokemon);
    }
}

void Battle::triggerAbilities(Trigger trigger, Pokemon* target, void* context) {
    // 触发所有宝可梦的特性
    for (Side* side : {&sideA, &sideB}) {
        Pokemon* active = side->getActivePokemon();
        if (active && !active->isFainted()) {
            triggerAbility(active, trigger, target, context);
        }
    }
}

void Battle::triggerItemEffects(ItemTrigger trigger, Pokemon* target) {
    // 触发所有宝可梦的物品效果
    for (Side* side : {&sideA, &sideB}) {
        Pokemon* active = side->getActivePokemon();
        if (active && !active->isFainted()) {
            triggerItemEffect(active, trigger, target);
        }
    }
}

void Battle::triggerItemEffect(Pokemon* pokemon, ItemTrigger trigger, Pokemon* opponent, void* context) {
    if (!pokemon || pokemon->isFainted()) {
        return;
    }

    Item item = pokemon->getHeldItem();
    if (!item.hasTrigger(trigger)) {
        return;
    }

    appendSpecialEvent(*this, "item_trigger", {
        {"pokemon", pokemon->getName()},
        {"item", BattleToJson::itemTypeToString(item.type)},
        {"trigger", itemTriggerToString(trigger)},
        {"opponent", opponent ? opponent->getName() : ""}
    });
    const ItemType beforeItem = pokemon->getItemType();
    item.executeTrigger(trigger, pokemon, opponent, *this, context);
    const ItemType afterItem = pokemon->getItemType();

    if (pokemon->getAbility() == AbilityType::CudChew
        && isBerryItem(beforeItem)
        && afterItem == ItemType::None) {
        runtimeMoveState.cudChewPending[pokemon] = CudChewState{beforeItem, turnNumber + 1};
    }
}
