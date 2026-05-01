#include "Abilities.h"
#include "Battle/Battle.h"
#include "Pokemon.h" // For Pokemon class
#include "Battle/PRNG.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
std::string normalizeToken(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        if (ch == ' ' || ch == '-' || ch == '_' || ch == '\'') {
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

bool isSlicingMove(const Move& move) {
    const std::string key = normalizeToken(move.getName());
    return key == "aerialace"
        || key == "airslash"
        || key == "aquacutter"
        || key == "bitterblade"
        || key == "ceaselessedge"
        || key == "crosspoison"
        || key == "cut"
        || key == "furycutter"
        || key == "leafblade"
        || key == "nightslash"
        || key == "populationbomb"
        || key == "psyblade"
        || key == "razorleaf"
        || key == "razorwind"
        || key == "sacredsword"
        || key == "slash"
        || key == "solarblade"
        || key == "xscissor";
}

    bool isWindMove(const Move& move) {
        const std::string key = normalizeToken(move.getName());
        return key == "gust"
        || key == "hurricane"
        || key == "heatwave"
        || key == "icywind"
        || key == "twister"
        || key == "fairywind"
        || key == "bleakwindstorm"
        || key == "wildboltstorm";
    }

    StatIndex strongestParadoxStat(const Pokemon* self) {
        if (!self) {
            return StatIndex::Attack;
        }

        const int attack = self->getAttack();
        const int defense = self->getDefense();
        const int specialAttack = self->getSpecialAttack();
        const int specialDefense = self->getSpecialDefense();
        const int speed = self->getSpeed();

        int maxValue = attack;
        StatIndex strongest = StatIndex::Attack;
        if (defense > maxValue) {
            maxValue = defense;
            strongest = StatIndex::Defense;
        }
        if (specialAttack > maxValue) {
            maxValue = specialAttack;
            strongest = StatIndex::SpecialAttack;
        }
        if (specialDefense > maxValue) {
            maxValue = specialDefense;
            strongest = StatIndex::SpecialDefense;
        }
        if (speed > maxValue) {
            strongest = StatIndex::Speed;
        }
        return strongest;
    }

    bool paradoxBoostActive(AbilityType abilityType, WeatherType weatherType, FieldType fieldType) {
        if (abilityType == AbilityType::Protosynthesis) {
            return weatherType == WeatherType::Sun;
        }
        if (abilityType == AbilityType::QuarkDrive) {
            return fieldType == FieldType::Electric;
        }
        return false;
    }
}

std::string getAbilityName(AbilityType type) {
    switch (type) {
        case AbilityType::Intimidate: return "Intimidate";
        case AbilityType::Overgrow: return "Overgrow";
        case AbilityType::Blaze: return "Blaze";
        case AbilityType::Torrent: return "Torrent";
        case AbilityType::Multiscale: return "Multiscale";
        case AbilityType::Levitate: return "Levitate";
        case AbilityType::WaterAbsorb: return "Water Absorb";
        case AbilityType::VoltAbsorb: return "Volt Absorb";
        case AbilityType::FlashFire: return "Flash Fire";
        case AbilityType::Static: return "Static";
        case AbilityType::PoisonPoint: return "Poison Point";
        case AbilityType::Aftermath: return "Aftermath";
        case AbilityType::Mummy: return "Mummy";
        case AbilityType::RoughSkin: return "Rough Skin";
        case AbilityType::FlameBody: return "Flame Body";
        case AbilityType::Insomnia: return "Insomnia";
        case AbilityType::VitalSpirit: return "Vital Spirit";
        case AbilityType::Guts: return "Guts";
        case AbilityType::HugePower: return "Huge Power";
        case AbilityType::ThickFat: return "Thick Fat";
        case AbilityType::MarvelScale: return "Marvel Scale";
        case AbilityType::SapSipper: return "Sap Sipper";
        case AbilityType::IronBarbs: return "Iron Barbs";
        case AbilityType::StormDrain: return "Storm Drain";
        case AbilityType::MotorDrive: return "Motor Drive";
        case AbilityType::Immunity: return "Immunity";
        case AbilityType::Technician: return "Technician";
        case AbilityType::Filter: return "Filter";
        case AbilityType::SolidRock: return "Solid Rock";
        case AbilityType::Moxie: return "Moxie";
        case AbilityType::InnerFocus: return "Inner Focus";
        case AbilityType::Regenerator: return "Regenerator";
        case AbilityType::NaturalCure: return "Natural Cure";
        case AbilityType::MagicGuard: return "Magic Guard";
        case AbilityType::Unaware: return "Unaware";
        case AbilityType::Prankster: return "Prankster";
        case AbilityType::ClearBody: return "Clear Body";
        case AbilityType::Defiant: return "Defiant";
        case AbilityType::Competitive: return "Competitive";
        case AbilityType::WhiteSmoke: return "White Smoke";
        case AbilityType::MirrorArmor: return "Mirror Armor";
        case AbilityType::HyperCutter: return "Hyper Cutter";
        case AbilityType::KeenEye: return "Keen Eye";
        case AbilityType::Drizzle: return "Drizzle";
        case AbilityType::Drought: return "Drought";
        case AbilityType::SandStream: return "Sand Stream";
        case AbilityType::SnowWarning: return "Snow Warning";
        case AbilityType::CloudNine: return "Cloud Nine";
        case AbilityType::GrassySurge: return "Grassy Surge";
        case AbilityType::ElectricSurge: return "Electric Surge";
        case AbilityType::PsychicSurge: return "Psychic Surge";
        case AbilityType::MistySurge: return "Misty Surge";
        case AbilityType::HadronEngine: return "Hadron Engine";
        case AbilityType::Protean: return "Protean";
        case AbilityType::Libero: return "Libero";
        case AbilityType::Adaptability: return "Adaptability";
        case AbilityType::SheerForce: return "Sheer Force";
        case AbilityType::Infiltrator: return "Infiltrator";
        case AbilityType::BeadsOfRuin: return "Beads of Ruin";
        case AbilityType::SwordOfRuin: return "Sword of Ruin";
        case AbilityType::TabletsOfRuin: return "Tablets of Ruin";
        case AbilityType::VesselOfRuin: return "Vessel of Ruin";
        case AbilityType::Unnerve: return "Unnerve";
        case AbilityType::EarthEater: return "Earth Eater";
        case AbilityType::Sharpness: return "Sharpness";
        case AbilityType::PurifyingSalt: return "Purifying Salt";
        case AbilityType::WellBakedBody: return "Well-Baked Body";
        case AbilityType::WindRider: return "Wind Rider";
        case AbilityType::ToxicDebris: return "Toxic Debris";
        case AbilityType::LingeringAroma: return "Lingering Aroma";
        case AbilityType::ArmorTail: return "Armor Tail";
        case AbilityType::GoodAsGold: return "Good as Gold";
        case AbilityType::Stakeout: return "Stakeout";
        case AbilityType::CudChew: return "Cud Chew";
        case AbilityType::MoldBreaker: return "Mold Breaker";
        case AbilityType::Protosynthesis: return "Protosynthesis";
        case AbilityType::QuarkDrive: return "Quark Drive";
        case AbilityType::SupremeOverlord: return "Supreme Overlord";
        default: return "None";
    }
}

AbilityType getAbilityTypeByName(const std::string& name) {
    std::string key;
    key.reserve(name.size());
    for (char ch : name) {
        if (ch == ' ' || ch == '-' || ch == '_' || ch == '\'') {
            continue;
        }
        key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (key == "intimidate") return AbilityType::Intimidate;
    if (key == "overgrow") return AbilityType::Overgrow;
    if (key == "blaze") return AbilityType::Blaze;
    if (key == "torrent") return AbilityType::Torrent;
    if (key == "multiscale") return AbilityType::Multiscale;
    if (key == "levitate") return AbilityType::Levitate;
    if (key == "waterabsorb") return AbilityType::WaterAbsorb;
    if (key == "voltabsorb") return AbilityType::VoltAbsorb;
    if (key == "flashfire") return AbilityType::FlashFire;
    if (key == "static") return AbilityType::Static;
    if (key == "poisonpoint") return AbilityType::PoisonPoint;
    if (key == "aftermath") return AbilityType::Aftermath;
    if (key == "mummy") return AbilityType::Mummy;
    if (key == "roughskin") return AbilityType::RoughSkin;
    if (key == "flamebody") return AbilityType::FlameBody;
    if (key == "insomnia") return AbilityType::Insomnia;
    if (key == "vitalspirit") return AbilityType::VitalSpirit;
    if (key == "guts") return AbilityType::Guts;
    if (key == "hugepower") return AbilityType::HugePower;
    if (key == "thickfat") return AbilityType::ThickFat;
    if (key == "marvelscale") return AbilityType::MarvelScale;
    if (key == "sapsipper") return AbilityType::SapSipper;
    if (key == "ironbarbs") return AbilityType::IronBarbs;
    if (key == "stormdrain") return AbilityType::StormDrain;
    if (key == "motordrive") return AbilityType::MotorDrive;
    if (key == "immunity") return AbilityType::Immunity;
    if (key == "technician") return AbilityType::Technician;
    if (key == "filter") return AbilityType::Filter;
    if (key == "solidrock") return AbilityType::SolidRock;
    if (key == "moxie") return AbilityType::Moxie;
    if (key == "innerfocus") return AbilityType::InnerFocus;
    if (key == "regenerator") return AbilityType::Regenerator;
    if (key == "naturalcure") return AbilityType::NaturalCure;
    if (key == "magicguard") return AbilityType::MagicGuard;
    if (key == "unaware") return AbilityType::Unaware;
    if (key == "prankster") return AbilityType::Prankster;
    if (key == "clearbody") return AbilityType::ClearBody;
    if (key == "defiant") return AbilityType::Defiant;
    if (key == "competitive") return AbilityType::Competitive;
    if (key == "whitesmoke") return AbilityType::WhiteSmoke;
    if (key == "mirrorarmor") return AbilityType::MirrorArmor;
    if (key == "hypercutter") return AbilityType::HyperCutter;
    if (key == "keeneye") return AbilityType::KeenEye;
    if (key == "drizzle") return AbilityType::Drizzle;
    if (key == "drought") return AbilityType::Drought;
    if (key == "sandstream") return AbilityType::SandStream;
    if (key == "snowwarning") return AbilityType::SnowWarning;
    if (key == "cloudnine") return AbilityType::CloudNine;
    if (key == "grassysurge") return AbilityType::GrassySurge;
    if (key == "electricsurge") return AbilityType::ElectricSurge;
    if (key == "psychicsurge") return AbilityType::PsychicSurge;
    if (key == "mistysurge") return AbilityType::MistySurge;
    if (key == "hadronengine") return AbilityType::HadronEngine;
    if (key == "protean") return AbilityType::Protean;
    if (key == "libero") return AbilityType::Libero;
    if (key == "adaptability") return AbilityType::Adaptability;
    if (key == "sheerforce") return AbilityType::SheerForce;
    if (key == "infiltrator") return AbilityType::Infiltrator;
    if (key == "beadsofruin") return AbilityType::BeadsOfRuin;
    if (key == "swordofruin") return AbilityType::SwordOfRuin;
    if (key == "tabletsofruin") return AbilityType::TabletsOfRuin;
    if (key == "vesselofruin") return AbilityType::VesselOfRuin;
    if (key == "unnerve") return AbilityType::Unnerve;
    if (key == "eartheater") return AbilityType::EarthEater;
    if (key == "sharpness") return AbilityType::Sharpness;
    if (key == "purifyingsalt") return AbilityType::PurifyingSalt;
    if (key == "wellbakedbody") return AbilityType::WellBakedBody;
    if (key == "windrider") return AbilityType::WindRider;
    if (key == "toxicdebris") return AbilityType::ToxicDebris;
    if (key == "lingeringaroma") return AbilityType::LingeringAroma;
    if (key == "armortail") return AbilityType::ArmorTail;
    if (key == "goodasgold") return AbilityType::GoodAsGold;
    if (key == "stakeout") return AbilityType::Stakeout;
    if (key == "cudchew") return AbilityType::CudChew;
    if (key == "moldbreaker") return AbilityType::MoldBreaker;
    if (key == "protosynthesis") return AbilityType::Protosynthesis;
    if (key == "quarkdrive") return AbilityType::QuarkDrive;
    if (key == "supremeoverlord") return AbilityType::SupremeOverlord;
    return AbilityType::None;
}

Ability getAbility(AbilityType type) {
    AbilityData abilityData = getAbilityData(type);
    if (abilityData.type == AbilityType::None && type != AbilityType::None) {
        abilityData.type = type;
    }
    if (abilityData.name.empty() || abilityData.name == "None") {
        abilityData.name = getAbilityName(type);
    }

    Ability ability(abilityData);
    ability.damageModifier = {1.0f, true};

    auto addTypeImmunity = [&ability](Type moveType, bool healInstead = false, int healPercent = 0) {
        ability.typeImmunities.push_back({static_cast<int>(moveType), healInstead, healPercent});
    };
    auto addStatusImmunity = [&ability](StatusType status) {
        ability.statusImmunities.push_back({static_cast<int>(status)});
    };

    switch (type) {
        case AbilityType::Intimidate:
            ability.effects[Trigger::OnEntry] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!opponent || !self) return;
                if (!opponent->isFainted()) {
                    const AbilityType opponentAbility = opponent->getAbility();
                    if (abilityBlocksAttackDrops(opponentAbility)) {
                        return;
                    }
                    if (abilityReflectsStatDrops(opponentAbility)) {
                        self->changeStatStage(StatIndex::Attack, -1);
                        return;
                    }
                    const int beforeAttack = opponent->getStatStage(StatIndex::Attack);
                    opponent->changeStatStage(StatIndex::Attack, -1);
                    const int afterAttack = opponent->getStatStage(StatIndex::Attack);
                    if (afterAttack < beforeAttack) {
                        applyStatLoweredReaction(opponentAbility, opponent);
                    }
                }
            };
            break;
        case AbilityType::Overgrow:
            ability.damageModifier = {1.5f, true};
            break;
        case AbilityType::Blaze:
            ability.damageModifier = {1.5f, true};
            break;
        case AbilityType::Torrent:
            ability.damageModifier = {1.5f, true};
            break;
        case AbilityType::Multiscale:
            ability.damageModifier = {0.5f, false};
            break;
        case AbilityType::Levitate:
            addTypeImmunity(Type::Ground, false, 0);
            break;
        case AbilityType::WaterAbsorb:
            addTypeImmunity(Type::Water, true, 25);
            break;
        case AbilityType::VoltAbsorb:
            addTypeImmunity(Type::Electric, true, 25);
            break;
        case AbilityType::FlashFire:
            addTypeImmunity(Type::Fire, false, 0);
            ability.damageModifier = {1.5f, true};
            break;
        case AbilityType::Static:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) return;
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (PRNG::nextInt(0, 100) >= 30) {
                    return;
                }
                if (resolveStatusImmunity(opponent->getAbility(), StatusType::Paralysis)) {
                    return;
                }
                opponent->addStatus(StatusType::Paralysis);
            };
            break;
        case AbilityType::PoisonPoint:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) return;
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (PRNG::nextInt(0, 100) >= 30) {
                    return;
                }
                if (resolveStatusImmunity(opponent->getAbility(), StatusType::Poison)) {
                    return;
                }
                opponent->addStatus(StatusType::Poison);
            };
            break;
        case AbilityType::Aftermath:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) return;
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (!self->isFainted() || opponent->isFainted()) {
                    return;
                }
                const int chip = std::max(1, opponent->getMaxHP() / 4);
                opponent->setCurrentHP(opponent->getCurrentHP() - chip);
            };
            break;
        case AbilityType::Mummy:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) return;
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (opponent->isFainted()) {
                    return;
                }
                if (opponent->getAbility() != AbilityType::Mummy) {
                    opponent->setAbility(AbilityType::Mummy);
                }
            };
            break;
        case AbilityType::RoughSkin:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) return;
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (opponent->isFainted()) {
                    return;
                }
                const int chip = std::max(1, opponent->getMaxHP() / 8);
                opponent->setCurrentHP(opponent->getCurrentHP() - chip);
            };
            break;
        case AbilityType::FlameBody:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) return;
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (PRNG::nextInt(0, 100) >= 30) {
                    return;
                }
                if (resolveStatusImmunity(opponent->getAbility(), StatusType::Burn)) {
                    return;
                }
                opponent->addStatus(StatusType::Burn);
            };
            break;
        case AbilityType::Insomnia:
        case AbilityType::VitalSpirit:
            addStatusImmunity(StatusType::Sleep);
            break;
        case AbilityType::Immunity:
            addStatusImmunity(StatusType::Poison);
            addStatusImmunity(StatusType::ToxicPoison);
            break;
        case AbilityType::Guts:
            ability.damageModifier = {1.5f, true};
            break;
        case AbilityType::HugePower:
            ability.statModifiers.push_back({StatModifier::Attack, 2.0f, 0});
            break;
        case AbilityType::ThickFat:
            ability.damageModifier = {0.5f, false};
            break;
        case AbilityType::MarvelScale:
            ability.statModifiers.push_back({StatModifier::Defense, 1.5f, 0});
            break;
        case AbilityType::SapSipper:
            addTypeImmunity(Type::Grass, false, 0);
            break;
        case AbilityType::IronBarbs:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) return;
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (opponent->isFainted()) {
                    return;
                }
                const int chip = std::max(1, opponent->getMaxHP() / 8);
                opponent->setCurrentHP(opponent->getCurrentHP() - chip);
            };
            break;
        case AbilityType::StormDrain:
            addTypeImmunity(Type::Water, false, 0);
            break;
        case AbilityType::MotorDrive:
            addTypeImmunity(Type::Electric, false, 0);
            break;
        case AbilityType::Technician:
            ability.damageModifier = {1.5f, true};
            break;
        case AbilityType::Filter:
        case AbilityType::SolidRock:
            ability.damageModifier = {0.75f, false};
            break;
        case AbilityType::Moxie:
            ability.effects[Trigger::OnFaint] = [](Pokemon* self, Pokemon*, void*) {
                if (!self) {
                    return;
                }
                self->changeStatStage(StatIndex::Attack, 1);
            };
            break;
        case AbilityType::InnerFocus:
            addStatusImmunity(StatusType::Flinch);
            break;
        case AbilityType::Regenerator:
            ability.effects[Trigger::OnExit] = [](Pokemon* self, Pokemon*, void*) {
                if (!self || self->isFainted()) {
                    return;
                }
                const int healAmount = std::max(1, self->getMaxHP() / 3);
                self->setCurrentHP(self->getCurrentHP() + healAmount);
            };
            break;
        case AbilityType::NaturalCure:
            ability.effects[Trigger::OnExit] = [](Pokemon* self, Pokemon*, void*) {
                if (!self || self->isFainted()) {
                    return;
                }
                self->clearStatuses();
            };
            break;
        case AbilityType::MagicGuard:
        case AbilityType::Unaware:
        case AbilityType::Prankster:
        case AbilityType::ClearBody:
        case AbilityType::Defiant:
        case AbilityType::Competitive:
        case AbilityType::WhiteSmoke:
        case AbilityType::MirrorArmor:
        case AbilityType::HyperCutter:
        case AbilityType::KeenEye:
            break;
        case AbilityType::Drizzle:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getWeather().setWeather(WeatherType::Rain, 5);
            };
            break;
        case AbilityType::Drought:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getWeather().setWeather(WeatherType::Sun, 5);
            };
            break;
        case AbilityType::SandStream:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getWeather().setWeather(WeatherType::Sandstorm, 5);
            };
            break;
        case AbilityType::SnowWarning:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getWeather().setWeather(WeatherType::Snow, 5);
            };
            break;
        case AbilityType::CloudNine:
            break;
        case AbilityType::GrassySurge:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getField().setField(FieldType::Grassy, 5);
            };
            break;
        case AbilityType::ElectricSurge:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getField().setField(FieldType::Electric, 5);
            };
            break;
        case AbilityType::PsychicSurge:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getField().setField(FieldType::Psychic, 5);
            };
            break;
        case AbilityType::MistySurge:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getField().setField(FieldType::Misty, 5);
            };
            break;
        case AbilityType::HadronEngine:
            ability.effects[Trigger::OnEntry] = [](Pokemon*, Pokemon*, void* context) {
                Battle* battle = static_cast<Battle*>(context);
                if (!battle) {
                    return;
                }
                battle->getField().setField(FieldType::Electric, 5);
            };
            break;
        case AbilityType::Protean:
        case AbilityType::Libero:
        case AbilityType::Adaptability:
            break;
        case AbilityType::SheerForce:
            ability.damageModifier = {1.3f, true};
            break;
        case AbilityType::Infiltrator:
        case AbilityType::BeadsOfRuin:
        case AbilityType::SwordOfRuin:
        case AbilityType::TabletsOfRuin:
        case AbilityType::VesselOfRuin:
        case AbilityType::Unnerve:
            break;
        case AbilityType::EarthEater:
            addTypeImmunity(Type::Ground, true, 25);
            break;
        case AbilityType::Sharpness:
            break;
        case AbilityType::PurifyingSalt:
            addStatusImmunity(StatusType::Burn);
            addStatusImmunity(StatusType::Freeze);
            addStatusImmunity(StatusType::Paralysis);
            addStatusImmunity(StatusType::Poison);
            addStatusImmunity(StatusType::Sleep);
            addStatusImmunity(StatusType::ToxicPoison);
            break;
        case AbilityType::WellBakedBody:
            addTypeImmunity(Type::Fire, false, 0);
            break;
        case AbilityType::WindRider:
            break;
        case AbilityType::ToxicDebris:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) {
                    return;
                }
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->move || !damageContext->battle) {
                    return;
                }
                if (damageContext->move->getCategory() != Category::Physical) {
                    return;
                }
                Side* opponentSide = Battle::findSideForPokemon(*damageContext->battle, opponent);
                if (!opponentSide) {
                    return;
                }
                opponentSide->addToxicSpikesLayer();
            };
            break;
        case AbilityType::LingeringAroma:
            ability.effects[Trigger::OnDamage] = [](Pokemon* self, Pokemon* opponent, void* context) {
                if (!self || !opponent) {
                    return;
                }
                const AbilityDamageContext* damageContext = static_cast<const AbilityDamageContext*>(context);
                if (!damageContext || !damageContext->isDamagingMove || !damageContext->isContact) {
                    return;
                }
                if (opponent->isFainted()) {
                    return;
                }
                if (opponent->getAbility() != AbilityType::LingeringAroma) {
                    opponent->setAbility(AbilityType::LingeringAroma);
                }
            };
            break;
        case AbilityType::ArmorTail:
            break;
        case AbilityType::GoodAsGold:
        case AbilityType::Stakeout:
        case AbilityType::CudChew:
        case AbilityType::MoldBreaker:
        case AbilityType::Protosynthesis:
        case AbilityType::QuarkDrive:
        case AbilityType::SupremeOverlord:
            break;
        default:
            break;
    }
    return ability;
}

bool resolveTypeImmunity(AbilityType abilityType, Type moveType, bool& healInstead, int& healPercent) {
    healInstead = false;
    healPercent = 0;

    const Ability ability = getAbility(abilityType);
    for (const TypeImmunity& immunity : ability.typeImmunities) {
        if (immunity.typeId != static_cast<int>(moveType)) {
            continue;
        }
        healInstead = immunity.healInstead;
        healPercent = immunity.healPercent;
        return true;
    }

    return false;
}

bool resolveStatusImmunity(AbilityType abilityType, StatusType status) {
    // Preserve existing simulator behavior: Vital Spirit currently blocks paralysis.
    if (abilityType == AbilityType::VitalSpirit && status == StatusType::Paralysis) {
        return true;
    }

    const Ability ability = getAbility(abilityType);
    for (const StatusImmunity& immunity : ability.statusImmunities) {
        if (immunity.statusId == static_cast<int>(status)) {
            return true;
        }
    }

    return false;
}

bool abilitySuppressesWeather(AbilityType abilityType) {
    return abilityType == AbilityType::CloudNine;
}

bool abilityIgnoresSubstitute(AbilityType abilityType) {
    return abilityType == AbilityType::Infiltrator;
}

bool abilityIgnoresScreens(AbilityType abilityType) {
    return abilityType == AbilityType::Infiltrator;
}

bool abilityBlocksBerryConsumption(AbilityType abilityType) {
    return abilityType == AbilityType::Unnerve;
}

bool abilityIgnoresIndirectDamage(AbilityType abilityType) {
    return abilityType == AbilityType::MagicGuard;
}

bool abilityCanTypeShift(AbilityType abilityType) {
    return abilityType == AbilityType::Protean || abilityType == AbilityType::Libero;
}

bool abilityIgnoresOpponentStatStages(AbilityType abilityType) {
    return abilityType == AbilityType::Unaware;
}

float abilityStabBonusMultiplier(AbilityType abilityType) {
    return abilityType == AbilityType::Adaptability ? 2.0f : 1.5f;
}

bool abilitySuppressesSecondaryEffects(AbilityType abilityType, const Move& move, bool sheerForceBoostedMove) {
    return abilityType == AbilityType::SheerForce && sheerForceBoostedMove && move.getCategory() != Category::Status;
}

int abilityStatusMovePriorityBonus(AbilityType abilityType) {
    return abilityType == AbilityType::Prankster ? 1 : 0;
}

bool abilityBlocksGenericStatDrops(AbilityType abilityType) {
    return abilityType == AbilityType::ClearBody || abilityType == AbilityType::WhiteSmoke;
}

bool abilityBlocksAttackDrops(AbilityType abilityType) {
    return abilityBlocksGenericStatDrops(abilityType) || abilityType == AbilityType::HyperCutter;
}

bool abilityBlocksAccuracyDrops(AbilityType abilityType) {
    return abilityBlocksGenericStatDrops(abilityType);
}

bool abilityBlocksEvasionDrops(AbilityType abilityType) {
    return abilityBlocksGenericStatDrops(abilityType) || abilityType == AbilityType::KeenEye;
}

bool abilityReflectsStatDrops(AbilityType abilityType) {
    return abilityType == AbilityType::MirrorArmor;
}

void applyStatLoweredReaction(AbilityType abilityType, Pokemon* self) {
    if (!self) {
        return;
    }

    if (abilityType == AbilityType::Defiant) {
        self->changeStatStage(StatIndex::Attack, 2);
    } else if (abilityType == AbilityType::Competitive) {
        self->changeStatStage(StatIndex::SpecialAttack, 2);
    }
}

bool abilityLowersOpponentPhysicalAttackAura(AbilityType abilityType) {
    return abilityType == AbilityType::TabletsOfRuin;
}

bool abilityLowersOpponentSpecialAttackAura(AbilityType abilityType) {
    return abilityType == AbilityType::VesselOfRuin;
}

bool abilityLowersOpponentDefenseAura(AbilityType abilityType) {
    return abilityType == AbilityType::SwordOfRuin;
}

bool abilityLowersOpponentSpecialDefenseAura(AbilityType abilityType) {
    return abilityType == AbilityType::BeadsOfRuin;
}

bool abilityGrantsGroundHazardImmunity(AbilityType abilityType) {
    bool healInstead = false;
    int healPercent = 0;
    return resolveTypeImmunity(abilityType, Type::Ground, healInstead, healPercent);
}

std::string abilityTypeImmunityEventReason(AbilityType abilityType) {
    if (abilityType == AbilityType::WaterAbsorb) {
        return "water_absorb";
    }
    if (abilityType == AbilityType::VoltAbsorb) {
        return "volt_absorb";
    }
    return "ability_immunity";
}

void applyTypeImmunityBonus(AbilityType abilityType, Pokemon* self) {
    if (!self) {
        return;
    }

    switch (abilityType) {
        case AbilityType::SapSipper:
            self->changeStatStage(StatIndex::Attack, 1);
            break;
        case AbilityType::StormDrain:
            self->changeStatStage(StatIndex::SpecialAttack, 1);
            break;
        case AbilityType::MotorDrive:
            self->changeStatStage(StatIndex::Speed, 1);
            break;
        case AbilityType::WellBakedBody:
            self->changeStatStage(StatIndex::Defense, 2);
            break;
        case AbilityType::WindRider:
            self->changeStatStage(StatIndex::Attack, 1);
            break;
        default:
            break;
    }
}

float abilityAttackStatMultiplier(AbilityType abilityType, const Move& move, bool hasMajorStatus, bool electricTerrainActive) {
    if (move.getCategory() == Category::Physical) {
        if (abilityType == AbilityType::HugePower) {
            return 2.0f;
        }
        if (abilityType == AbilityType::Guts && hasMajorStatus) {
            return 1.5f;
        }
    }

    if (move.getCategory() == Category::Special
        && abilityType == AbilityType::HadronEngine
        && electricTerrainActive) {
        return 1.3f;
    }

    return 1.0f;
}

float abilityDefenseStatMultiplier(AbilityType abilityType, const Move& move, bool hasMajorStatus) {
    (void)move;
    if (abilityType == AbilityType::MarvelScale && hasMajorStatus) {
        return 1.5f;
    }
    return 1.0f;
}

int applyAbilityPowerModifier(AbilityType abilityType, const Move& move, int basePower, bool sheerForceBoostedMove) {
    int modifiedPower = basePower;
    if (abilityType == AbilityType::Technician
        && move.getCategory() != Category::Status
        && modifiedPower <= 60) {
        modifiedPower = static_cast<int>(std::lround(modifiedPower * 1.5f));
    }

    if (abilityType == AbilityType::Sharpness
        && move.getCategory() != Category::Status
        && isSlicingMove(move)) {
        modifiedPower = static_cast<int>(std::lround(modifiedPower * 1.5f));
    }

    if (abilityType == AbilityType::SheerForce && sheerForceBoostedMove) {
        modifiedPower = static_cast<int>(std::lround(modifiedPower * 1.3f));
    }

    return modifiedPower;
}

float abilityOutgoingDamageMultiplier(AbilityType abilityType, const Move& move, int currentHp, int maxHp, bool targetJustSwitchedIn, int faintedAllies) {
    if (maxHp <= 0) {
        return 1.0f;
    }

    const bool lowHpBoostRange = currentHp * 3 <= maxHp;
    if (abilityType == AbilityType::Blaze && move.getType() == Type::Fire && lowHpBoostRange) {
        return 1.5f;
    }
    if (abilityType == AbilityType::Torrent && move.getType() == Type::Water && lowHpBoostRange) {
        return 1.5f;
    }
    if (abilityType == AbilityType::Overgrow && move.getType() == Type::Grass && lowHpBoostRange) {
        return 1.5f;
    }
    if (abilityType == AbilityType::FlashFire && move.getType() == Type::Fire) {
        return 1.5f;
    }
    if (abilityType == AbilityType::Stakeout && targetJustSwitchedIn) {
        return 2.0f;
    }
    if (abilityType == AbilityType::SupremeOverlord && faintedAllies > 0) {
        const int clamped = std::min(5, faintedAllies);
        return 1.0f + 0.1f * static_cast<float>(clamped);
    }

    return 1.0f;
}

float abilityIncomingDamageMultiplier(AbilityType abilityType, const Move& move, float typeEffectiveness, int currentHp, int maxHp) {
    float multiplier = 1.0f;

    if (abilityType == AbilityType::ThickFat && (move.getType() == Type::Fire || move.getType() == Type::Ice)) {
        multiplier *= 0.5f;
    }
    if ((abilityType == AbilityType::Filter || abilityType == AbilityType::SolidRock) && typeEffectiveness > 1.0f) {
        multiplier *= 0.75f;
    }
    if (abilityType == AbilityType::Multiscale && maxHp > 0 && currentHp == maxHp) {
        multiplier *= 0.5f;
    }
    if (abilityType == AbilityType::PurifyingSalt && move.getType() == Type::Ghost) {
        multiplier *= 0.5f;
    }

    return multiplier;
}

bool abilityBlocksMoveDamage(AbilityType abilityType, const Move& move) {
    if (abilityType == AbilityType::WindRider && isWindMove(move)) {
        return true;
    }
    return false;
}

bool abilityBlocksPriorityTargetedMoves(AbilityType abilityType) {
    return abilityType == AbilityType::ArmorTail;
}

bool abilityBlocksStatusMovesFromOpponents(AbilityType abilityType) {
    return abilityType == AbilityType::GoodAsGold;
}

bool abilityIgnoresTargetAbility(AbilityType abilityType) {
    return abilityType == AbilityType::MoldBreaker;
}

float abilityParadoxStatMultiplier(AbilityType abilityType, const Pokemon* self, StatIndex stat, WeatherType weatherType, FieldType fieldType) {
    if (!paradoxBoostActive(abilityType, weatherType, fieldType)) {
        return 1.0f;
    }

    const StatIndex strongest = strongestParadoxStat(self);
    if (strongest != stat) {
        return 1.0f;
    }

    return strongest == StatIndex::Speed ? 1.5f : 1.3f;
}

std::vector<Ability> getAbilitiesForPokemon(AbilityType type) {
    // For now, each Pokemon has one ability
    std::vector<Ability> abilities;
    abilities.push_back(getAbility(type));
    return abilities;
}