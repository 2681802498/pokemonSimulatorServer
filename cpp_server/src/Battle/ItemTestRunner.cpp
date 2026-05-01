#include "Battle/ItemTestRunner.h"

#include "Battle/Battle.h"
#include "Battle/Pokemon.h"
#include "Battle/Status.h"

#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>

namespace {
std::string toItemSlug(ItemType type) {
    switch (type) {
        case ItemType::OranBerry: return "oran-berry";
        case ItemType::SitrusBerry: return "sitrus-berry";
        case ItemType::LumBerry: return "lum-berry";
        case ItemType::ChestoBerry: return "chesto-berry";
        case ItemType::PechaBerry: return "pecha-berry";
        case ItemType::RawstBerry: return "rawst-berry";
        case ItemType::AspearBerry: return "aspear-berry";
        case ItemType::PersimBerry: return "persim-berry";
        case ItemType::CheriBerry: return "cheri-berry";
        case ItemType::FigyBerry: return "figy-berry";
        case ItemType::WikiBerry: return "wiki-berry";
        case ItemType::MagoBerry: return "mago-berry";
        case ItemType::AguavBerry: return "aguav-berry";
        case ItemType::IapapaBerry: return "iapapa-berry";
        case ItemType::OccaBerry: return "occa-berry";
        case ItemType::PasshoBerry: return "passho-berry";
        case ItemType::WacanBerry: return "wacan-berry";
        case ItemType::RindoBerry: return "rindo-berry";
        case ItemType::YacheBerry: return "yache-berry";
        case ItemType::ChopleBerry: return "chople-berry";
        case ItemType::KebiaBerry: return "kebia-berry";
        case ItemType::ShucaBerry: return "shuca-berry";
        case ItemType::CobaBerry: return "coba-berry";
        case ItemType::PayapaBerry: return "payapa-berry";
        case ItemType::TangaBerry: return "tanga-berry";
        case ItemType::ChartiBerry: return "charti-berry";
        case ItemType::KasibBerry: return "kasib-berry";
        case ItemType::HabanBerry: return "haban-berry";
        case ItemType::ColburBerry: return "colbur-berry";
        case ItemType::BabiriBerry: return "babiri-berry";
        case ItemType::ChilanBerry: return "chilan-berry";
        case ItemType::LiechiBerry: return "liechi-berry";
        case ItemType::GanlonBerry: return "ganlon-berry";
        case ItemType::SalacBerry: return "salac-berry";
        case ItemType::PetayaBerry: return "petaya-berry";
        case ItemType::ApicotBerry: return "apicot-berry";
        case ItemType::JabocaBerry: return "jaboca-berry";
        case ItemType::RowapBerry: return "rowap-berry";
        case ItemType::Leftovers: return "leftovers";
        case ItemType::BlackSludge: return "black-sludge";
        case ItemType::ShellBell: return "shell-bell";
        case ItemType::ChoiceBand: return "choice-band";
        case ItemType::ChoiceSpecs: return "choice-specs";
        case ItemType::ChoiceScarf: return "choice-scarf";
        case ItemType::QuickClaw: return "quick-claw";
        case ItemType::LifeOrb: return "life-orb";
        case ItemType::ExpertBelt: return "expert-belt";
        case ItemType::MuscleBand: return "muscle-band";
        case ItemType::WiseGlasses: return "wise-glasses";
        case ItemType::LightBall: return "light-ball";
        case ItemType::QuickPowder: return "quick-powder";
        case ItemType::ThickClub: return "thick-club";
        case ItemType::MetalPowder: return "metal-powder";
        case ItemType::DeepSeaTooth: return "deep-sea-tooth";
        case ItemType::DeepSeaScale: return "deep-sea-scale";
        case ItemType::PowerHerb: return "power-herb";
        case ItemType::StickyBarb: return "sticky-barb";
        case ItemType::BigRoot: return "big-root";
        case ItemType::KingsRock: return "kings-rock";
        case ItemType::WideLens: return "wide-lens";
        case ItemType::ZoomLens: return "zoom-lens";
        case ItemType::ScopeLens: return "scope-lens";
        case ItemType::SilverPowder: return "silver-powder";
        case ItemType::MetalCoat: return "metal-coat";
        case ItemType::HardStone: return "hard-stone";
        case ItemType::MiracleSeed: return "miracle-seed";
        case ItemType::BlackGlasses: return "black-glasses";
        case ItemType::BlackBelt: return "black-belt";
        case ItemType::Magnet: return "magnet";
        case ItemType::MysticWater: return "mystic-water";
        case ItemType::SharpBeak: return "sharp-beak";
        case ItemType::PoisonBarb: return "poison-barb";
        case ItemType::NeverMeltIce: return "never-melt-ice";
        case ItemType::SpellTag: return "spell-tag";
        case ItemType::TwistedSpoon: return "twisted-spoon";
        case ItemType::Charcoal: return "charcoal";
        case ItemType::DragonFang: return "dragon-fang";
        case ItemType::SilkScarf: return "silk-scarf";
        case ItemType::SeaIncense: return "sea-incense";
        case ItemType::FlamePlate: return "flame-plate";
        case ItemType::SplashPlate: return "splash-plate";
        case ItemType::ZapPlate: return "zap-plate";
        case ItemType::MeadowPlate: return "meadow-plate";
        case ItemType::IciclePlate: return "icicle-plate";
        case ItemType::FistPlate: return "fist-plate";
        case ItemType::ToxicPlate: return "toxic-plate";
        case ItemType::EarthPlate: return "earth-plate";
        case ItemType::SkyPlate: return "sky-plate";
        case ItemType::MindPlate: return "mind-plate";
        case ItemType::InsectPlate: return "insect-plate";
        case ItemType::StonePlate: return "stone-plate";
        case ItemType::SpookyPlate: return "spooky-plate";
        case ItemType::IronPlate: return "iron-plate";
        case ItemType::FlameOrb: return "flame-orb";
        case ItemType::ToxicOrb: return "toxic-orb";
        case ItemType::FocusSash: return "focus-sash";
        case ItemType::RockyHelmet: return "rocky-helmet";
        case ItemType::AirBalloon: return "air-balloon";
        case ItemType::Eviolite: return "eviolite";
        case ItemType::AssaultVest: return "assault-vest";
        case ItemType::RedCard: return "red-card";
        case ItemType::EjectButton: return "eject-button";
        case ItemType::WhiteHerb: return "white-herb";
        case ItemType::WeaknessPolicy: return "weakness-policy";
        case ItemType::BerryJuice: return "berry-juice";
        default: return "none";
    }
}

bool isBattleApplicableDescription(const std::string& desc) {
    std::string lower;
    lower.reserve(desc.size());
    for (char ch : desc) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return lower.find("held:") != std::string::npos
        || lower.find("an item to be held by a pokemon") != std::string::npos
        || lower.find("an item to be held by the holder") != std::string::npos;
}

void resetPokemon(Pokemon* self, Pokemon* other) {
    if (self) {
        self->setCurrentHP(self->getMaxHP());
        self->clearStatuses();
        self->removeItem();
    }
    if (other) {
        other->setCurrentHP(other->getMaxHP());
        other->clearStatuses();
        other->removeItem();
    }
}

bool runSingleItemTestWithContext(ItemType itemType, Pokemon* self, Pokemon* other, Battle& battle, std::ostream& out, std::ostream& err) {
    Item item = getItem(itemType);
    bool pass = true;
    auto check = [&](bool condition, const std::string& message) {
        if (!condition) {
            pass = false;
            err << "  [FAIL] " << message << std::endl;
        } else {
            out << "  [PASS] " << message << std::endl;
        }
    };

    Move groundMove("Earthquake", Type::Ground, Category::Physical, 100, 100, 10);
    Move electricMove("Thunderbolt", Type::Electric, Category::Special, 90, 100, 15);
    Move grassMove("Energy Ball", Type::Grass, Category::Special, 90, 100, 10);
    Move waterMove("Surf", Type::Water, Category::Special, 90, 100, 15);
    Move fireMove("Flamethrower", Type::Fire, Category::Special, 90, 100, 15);
    Move bugMove("Bug Bite", Type::Bug, Category::Physical, 60, 100, 20);
    Move steelMove("Iron Head", Type::Steel, Category::Physical, 80, 100, 15);
    Move rockMove("Rock Slide", Type::Rock, Category::Physical, 75, 90, 10);
    Move darkMove("Knock Off", Type::Dark, Category::Physical, 65, 100, 20);
    Move fightingMove("Close Combat", Type::Fighting, Category::Physical, 120, 100, 5);
    Move flyingMove("Air Slash", Type::Flying, Category::Special, 75, 95, 15);
    Move poisonMove("Sludge Bomb", Type::Poison, Category::Special, 90, 100, 10);
    Move iceMove("Ice Beam", Type::Ice, Category::Special, 90, 100, 10);
    Move ghostMove("Shadow Ball", Type::Ghost, Category::Special, 80, 100, 15);
    Move psychicMove("Psychic", Type::Psychic, Category::Special, 90, 100, 10);
    Move dragonMove("Dragon Claw", Type::Dragon, Category::Physical, 80, 100, 15);
    Move normalMove("Tackle", Type::Normal, Category::Physical, 40, 100, 35);

    switch (itemType) {
        case ItemType::OranBerry:
        case ItemType::SitrusBerry:
        case ItemType::BerryJuice: {
            self->holdItem(itemType);
            self->setCurrentHP(self->getMaxHP() / 2);
            int before = self->getCurrentHP();
            ItemDamageContext ctx{&electricMove, 30, before, false, true, false};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(self->getCurrentHP() > before, "HP should be recovered at or below half HP");
            check(self->getItemType() == ItemType::None, "Berry should be consumed after activation");
            break;
        }
        case ItemType::LumBerry: {
            self->holdItem(itemType);
            self->addStatus(StatusType::Sleep);
            self->addStatus(StatusType::Poison);
            item.executeTrigger(ItemTrigger::OnStatus, self, other, battle);
            check(self->getStatuses().empty(), "Lum Berry should cure all status");
            break;
        }
        case ItemType::ChestoBerry: {
            self->holdItem(itemType);
            self->addStatus(StatusType::Sleep);
            item.executeTrigger(ItemTrigger::OnStatus, self, other, battle);
            check(!self->hasStatus(StatusType::Sleep), "Chesto Berry should cure sleep");
            break;
        }
        case ItemType::PechaBerry: {
            self->holdItem(itemType);
            self->addStatus(StatusType::Poison);
            item.executeTrigger(ItemTrigger::OnStatus, self, other, battle);
            check(!self->hasStatus(StatusType::Poison), "Pecha Berry should cure poison");
            break;
        }
        case ItemType::RawstBerry: {
            self->holdItem(itemType);
            self->addStatus(StatusType::Burn);
            item.executeTrigger(ItemTrigger::OnStatus, self, other, battle);
            check(!self->hasStatus(StatusType::Burn), "Rawst Berry should cure burn");
            break;
        }
        case ItemType::AspearBerry: {
            self->holdItem(itemType);
            self->addStatus(StatusType::Freeze);
            item.executeTrigger(ItemTrigger::OnStatus, self, other, battle);
            check(!self->hasStatus(StatusType::Freeze), "Aspear Berry should cure freeze");
            break;
        }
        case ItemType::PersimBerry: {
            self->holdItem(itemType);
            self->addStatus(StatusType::Confusion);
            item.executeTrigger(ItemTrigger::OnStatus, self, other, battle);
            check(!self->hasStatus(StatusType::Confusion), "Persim Berry should cure confusion");
            break;
        }
        case ItemType::CheriBerry: {
            self->holdItem(itemType);
            self->addStatus(StatusType::Paralysis);
            item.executeTrigger(ItemTrigger::OnStatus, self, other, battle);
            check(!self->hasStatus(StatusType::Paralysis), "Cheri Berry should cure paralysis");
            check(self->getItemType() == ItemType::None, "Cheri Berry should be consumed after activation");
            break;
        }
        case ItemType::FigyBerry:
        case ItemType::WikiBerry:
        case ItemType::MagoBerry:
        case ItemType::AguavBerry:
        case ItemType::IapapaBerry: {
            self->holdItem(itemType);
            self->setCurrentHP(self->getMaxHP() / 2);
            int before = self->getCurrentHP();
            ItemDamageContext ctx{&electricMove, 20, before, false, true, false};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(self->getCurrentHP() > before, "Pinch heal berry should recover HP at or below half HP");
            check(self->getItemType() == ItemType::None, "Pinch heal berry should be consumed after activation");
            break;
        }
        case ItemType::OccaBerry:
        case ItemType::PasshoBerry:
        case ItemType::WacanBerry:
        case ItemType::RindoBerry:
        case ItemType::YacheBerry:
        case ItemType::ChopleBerry:
        case ItemType::KebiaBerry:
        case ItemType::ShucaBerry:
        case ItemType::CobaBerry:
        case ItemType::PayapaBerry:
        case ItemType::TangaBerry:
        case ItemType::ChartiBerry:
        case ItemType::KasibBerry:
        case ItemType::HabanBerry:
        case ItemType::ColburBerry:
        case ItemType::BabiriBerry:
        case ItemType::ChilanBerry: {
            self->holdItem(itemType);
            const Move* resistedMove = &normalMove;
            switch (itemType) {
                case ItemType::OccaBerry: resistedMove = &fireMove; break;
                case ItemType::PasshoBerry: resistedMove = &waterMove; break;
                case ItemType::WacanBerry: resistedMove = &electricMove; break;
                case ItemType::RindoBerry: resistedMove = &grassMove; break;
                case ItemType::YacheBerry: resistedMove = &iceMove; break;
                case ItemType::ChopleBerry: resistedMove = &fightingMove; break;
                case ItemType::KebiaBerry: resistedMove = &poisonMove; break;
                case ItemType::ShucaBerry: resistedMove = &groundMove; break;
                case ItemType::CobaBerry: resistedMove = &flyingMove; break;
                case ItemType::PayapaBerry: resistedMove = &psychicMove; break;
                case ItemType::TangaBerry: resistedMove = &bugMove; break;
                case ItemType::ChartiBerry: resistedMove = &rockMove; break;
                case ItemType::KasibBerry: resistedMove = &ghostMove; break;
                case ItemType::HabanBerry: resistedMove = &dragonMove; break;
                case ItemType::ColburBerry: resistedMove = &darkMove; break;
                case ItemType::BabiriBerry: resistedMove = &steelMove; break;
                case ItemType::ChilanBerry: resistedMove = &normalMove; break;
                default: break;
            }

            float reduced = item.applyDamageModifier(100.0f, self, other, *resistedMove, false);
            const bool isSuperEffectiveInFixture = (itemType == ItemType::ChopleBerry);
            if (isSuperEffectiveInFixture) {
                check(reduced < 100.0f, "Resist berry should reduce qualifying super-effective damage");
            } else {
                check(reduced == 100.0f, "Resist berry should only reduce damage on super-effective matchups");
            }

            ItemDamageContext ctx{resistedMove, 40, self->getCurrentHP(), true, true, false};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(self->getItemType() == ItemType::None, "Resist berry should be consumed when it activates");
            break;
        }
        case ItemType::LiechiBerry:
        case ItemType::GanlonBerry:
        case ItemType::SalacBerry:
        case ItemType::PetayaBerry:
        case ItemType::ApicotBerry: {
            self->holdItem(itemType);
            self->setCurrentHP(self->getMaxHP() / 4);
            int beforeStage = 0;
            switch (itemType) {
                case ItemType::LiechiBerry:
                    beforeStage = self->getStatStage(StatIndex::Attack);
                    break;
                case ItemType::GanlonBerry:
                    beforeStage = self->getStatStage(StatIndex::Defense);
                    break;
                case ItemType::SalacBerry:
                    beforeStage = self->getStatStage(StatIndex::Speed);
                    break;
                case ItemType::PetayaBerry:
                    beforeStage = self->getStatStage(StatIndex::SpecialAttack);
                    break;
                case ItemType::ApicotBerry:
                    beforeStage = self->getStatStage(StatIndex::SpecialDefense);
                    break;
                default:
                    break;
            }

            ItemDamageContext ctx{&electricMove, 30, self->getCurrentHP(), false, true, false};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);

            int afterStage = 0;
            switch (itemType) {
                case ItemType::LiechiBerry:
                    afterStage = self->getStatStage(StatIndex::Attack);
                    break;
                case ItemType::GanlonBerry:
                    afterStage = self->getStatStage(StatIndex::Defense);
                    break;
                case ItemType::SalacBerry:
                    afterStage = self->getStatStage(StatIndex::Speed);
                    break;
                case ItemType::PetayaBerry:
                    afterStage = self->getStatStage(StatIndex::SpecialAttack);
                    break;
                case ItemType::ApicotBerry:
                    afterStage = self->getStatStage(StatIndex::SpecialDefense);
                    break;
                default:
                    break;
            }

            check(afterStage > beforeStage, "Pinch stat berry should raise its target stat at low HP");
            check(self->getItemType() == ItemType::None, "Pinch stat berry should be consumed after activation");
            break;
        }
        case ItemType::JabocaBerry:
        case ItemType::RowapBerry: {
            self->holdItem(itemType);
            other->setCurrentHP(other->getMaxHP());
            int before = other->getCurrentHP();
            const Move* triggeringMove = (itemType == ItemType::JabocaBerry) ? &groundMove : &electricMove;
            ItemDamageContext ctx{triggeringMove, 40, self->getCurrentHP(), false, true, triggeringMove->getCategory() == Category::Physical};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(other->getCurrentHP() < before, "Retaliation berry should damage attacker on qualifying category hit");
            check(self->getItemType() == ItemType::None, "Retaliation berry should be consumed after activation");
            break;
        }
        case ItemType::Leftovers:
        case ItemType::BlackSludge: {
            self->holdItem(itemType);
            self->setCurrentHP(self->getMaxHP() - 10);
            int before = self->getCurrentHP();
            item.executeTrigger(ItemTrigger::OnTurnEnd, self, other, battle);
            if (itemType == ItemType::Leftovers) {
                check(self->getCurrentHP() > before, "Leftovers should recover HP each turn");
            } else {
                check(self->getCurrentHP() < before, "Black Sludge should damage non-Poison holder");
            }
            break;
        }
        case ItemType::ShellBell: {
            self->holdItem(itemType);
            self->setCurrentHP(self->getMaxHP() - 20);
            int before = self->getCurrentHP();
            ItemDamageContext ctx{&electricMove, 40, before, false, true, false};
            item.executeTrigger(ItemTrigger::OnDealDamage, self, other, battle, &ctx);
            check(self->getCurrentHP() > before, "Shell Bell should heal from dealt damage");
            break;
        }
        case ItemType::FocusSash: {
            self->holdItem(itemType);
            self->setCurrentHP(0);
            ItemDamageContext ctx{&electricMove, 999, self->getMaxHP(), false, true, true};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(self->getCurrentHP() == 1, "Focus Sash should leave holder at 1 HP");
            break;
        }
        case ItemType::RockyHelmet: {
            self->holdItem(itemType);
            int before = other->getCurrentHP();
            ItemDamageContext ctx{&groundMove, 50, self->getCurrentHP(), false, true, true};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(other->getCurrentHP() < before, "Rocky Helmet should damage contact attacker");
            break;
        }
        case ItemType::AirBalloon: {
            self->holdItem(itemType);
            float blocked = item.applyDamageModifier(100.0f, self, other, groundMove, false);
            check(blocked == 0.0f, "Air Balloon should grant Ground immunity before popping");
            ItemDamageContext ctx{&electricMove, 30, self->getCurrentHP(), false, true, false};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(self->getItemType() == ItemType::None, "Air Balloon should pop after taking move damage");
            break;
        }
        case ItemType::LifeOrb: {
            self->holdItem(itemType);
            int beforeHP = self->getCurrentHP();
            ItemDamageContext ctx{&electricMove, 40, beforeHP, false, true, false};
            item.executeTrigger(ItemTrigger::OnDealDamage, self, other, battle, &ctx);
            check(self->getCurrentHP() < beforeHP, "Life Orb should recoil after dealing damage");
            break;
        }
        case ItemType::WeaknessPolicy: {
            self->holdItem(itemType);
            ItemDamageContext ctx{&electricMove, 40, self->getCurrentHP(), true, true, false};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(self->getStatStage(StatIndex::Attack) >= 2, "Weakness Policy should raise Attack by 2 stages");
            check(self->getStatStage(StatIndex::SpecialAttack) >= 2, "Weakness Policy should raise Sp. Attack by 2 stages");
            break;
        }
        case ItemType::RedCard:
        case ItemType::EjectButton: {
            self->holdItem(itemType);
            ItemDamageContext ctx{&electricMove, 20, self->getCurrentHP(), false, true, false};
            item.executeTrigger(ItemTrigger::OnDamage, self, other, battle, &ctx);
            check(self->getItemType() == ItemType::None, "Reactive switch item should be consumed on hit");
            break;
        }
        case ItemType::ExpertBelt: {
            float boosted = item.applyDamageModifier(100.0f, self, other, electricMove, true);
            check(boosted == 100.0f, "Expert Belt should not boost neutral hits");
            break;
        }
        case ItemType::Magnet: {
            float boosted = item.applyDamageModifier(100.0f, self, other, electricMove, true);
            check(boosted > 100.0f, "Magnet should boost Electric moves");
            break;
        }
        case ItemType::MysticWater: {
            float boosted = item.applyDamageModifier(100.0f, self, other, waterMove, true);
            check(boosted > 100.0f, "Mystic Water should boost Water moves");
            break;
        }
        case ItemType::Charcoal: {
            float boosted = item.applyDamageModifier(100.0f, self, other, fireMove, true);
            check(boosted > 100.0f, "Charcoal should boost Fire moves");
            break;
        }
        case ItemType::SilverPowder: {
            float boosted = item.applyDamageModifier(100.0f, self, other, bugMove, true);
            check(boosted > 100.0f, "Silver Powder should boost Bug moves");
            break;
        }
        case ItemType::MetalCoat: {
            float boosted = item.applyDamageModifier(100.0f, self, other, steelMove, true);
            check(boosted > 100.0f, "Metal Coat should boost Steel moves");
            break;
        }
        case ItemType::HardStone: {
            float boosted = item.applyDamageModifier(100.0f, self, other, rockMove, true);
            check(boosted > 100.0f, "Hard Stone should boost Rock moves");
            break;
        }
        case ItemType::MiracleSeed: {
            float boosted = item.applyDamageModifier(100.0f, self, other, grassMove, true);
            check(boosted > 100.0f, "Miracle Seed should boost Grass moves");
            break;
        }
        case ItemType::BlackGlasses: {
            float boosted = item.applyDamageModifier(100.0f, self, other, darkMove, true);
            check(boosted > 100.0f, "Black Glasses should boost Dark moves");
            break;
        }
        case ItemType::BlackBelt: {
            float boosted = item.applyDamageModifier(100.0f, self, other, fightingMove, true);
            check(boosted > 100.0f, "Black Belt should boost Fighting moves");
            break;
        }
        case ItemType::SharpBeak: {
            float boosted = item.applyDamageModifier(100.0f, self, other, flyingMove, true);
            check(boosted > 100.0f, "Sharp Beak should boost Flying moves");
            break;
        }
        case ItemType::PoisonBarb: {
            float boosted = item.applyDamageModifier(100.0f, self, other, poisonMove, true);
            check(boosted > 100.0f, "Poison Barb should boost Poison moves");
            break;
        }
        case ItemType::NeverMeltIce: {
            float boosted = item.applyDamageModifier(100.0f, self, other, iceMove, true);
            check(boosted > 100.0f, "Never-Melt Ice should boost Ice moves");
            break;
        }
        case ItemType::SpellTag: {
            float boosted = item.applyDamageModifier(100.0f, self, other, ghostMove, true);
            check(boosted > 100.0f, "Spell Tag should boost Ghost moves");
            break;
        }
        case ItemType::TwistedSpoon: {
            float boosted = item.applyDamageModifier(100.0f, self, other, psychicMove, true);
            check(boosted > 100.0f, "Twisted Spoon should boost Psychic moves");
            break;
        }
        case ItemType::DragonFang: {
            float boosted = item.applyDamageModifier(100.0f, self, other, dragonMove, true);
            check(boosted > 100.0f, "Dragon Fang should boost Dragon moves");
            break;
        }
        case ItemType::SilkScarf: {
            float boosted = item.applyDamageModifier(100.0f, self, other, normalMove, true);
            check(boosted > 100.0f, "Silk Scarf should boost Normal moves");
            break;
        }
        case ItemType::SeaIncense:
        case ItemType::SplashPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, waterMove, true);
            check(boosted > 100.0f, "Water boosting item should boost Water moves");
            break;
        }
        case ItemType::FlamePlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, fireMove, true);
            check(boosted > 100.0f, "Flame Plate should boost Fire moves");
            break;
        }
        case ItemType::ZapPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, electricMove, true);
            check(boosted > 100.0f, "Zap Plate should boost Electric moves");
            break;
        }
        case ItemType::MeadowPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, grassMove, true);
            check(boosted > 100.0f, "Meadow Plate should boost Grass moves");
            break;
        }
        case ItemType::IciclePlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, iceMove, true);
            check(boosted > 100.0f, "Icicle Plate should boost Ice moves");
            break;
        }
        case ItemType::FistPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, fightingMove, true);
            check(boosted > 100.0f, "Fist Plate should boost Fighting moves");
            break;
        }
        case ItemType::ToxicPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, poisonMove, true);
            check(boosted > 100.0f, "Toxic Plate should boost Poison moves");
            break;
        }
        case ItemType::EarthPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, groundMove, true);
            check(boosted > 100.0f, "Earth Plate should boost Ground moves");
            break;
        }
        case ItemType::SkyPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, flyingMove, true);
            check(boosted > 100.0f, "Sky Plate should boost Flying moves");
            break;
        }
        case ItemType::MindPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, psychicMove, true);
            check(boosted > 100.0f, "Mind Plate should boost Psychic moves");
            break;
        }
        case ItemType::InsectPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, bugMove, true);
            check(boosted > 100.0f, "Insect Plate should boost Bug moves");
            break;
        }
        case ItemType::StonePlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, rockMove, true);
            check(boosted > 100.0f, "Stone Plate should boost Rock moves");
            break;
        }
        case ItemType::SpookyPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, ghostMove, true);
            check(boosted > 100.0f, "Spooky Plate should boost Ghost moves");
            break;
        }
        case ItemType::IronPlate: {
            float boosted = item.applyDamageModifier(100.0f, self, other, steelMove, true);
            check(boosted > 100.0f, "Iron Plate should boost Steel moves");
            break;
        }
        case ItemType::LightBall: {
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::Attack) > 100.0f, "Light Ball should boost Attack");
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::SpAttack) > 100.0f, "Light Ball should boost Sp. Attack");
            break;
        }
        case ItemType::QuickPowder: {
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::Speed) > 100.0f, "Quick Powder should boost Speed");
            break;
        }
        case ItemType::ThickClub: {
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::Attack) > 100.0f, "Thick Club should boost Attack");
            break;
        }
        case ItemType::MetalPowder: {
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::Defense) > 100.0f, "Metal Powder should boost Defense");
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::SpDefense) > 100.0f, "Metal Powder should boost Sp. Defense");
            break;
        }
        case ItemType::DeepSeaTooth: {
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::SpAttack) > 100.0f, "Deep Sea Tooth should boost Sp. Attack");
            break;
        }
        case ItemType::DeepSeaScale: {
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::SpDefense) > 100.0f, "Deep Sea Scale should boost Sp. Defense");
            break;
        }
        case ItemType::PowerHerb: {
            self->holdItem(itemType);
            item.executeTrigger(ItemTrigger::OnAttack, self, other, battle);
            check(self->getItemType() == ItemType::None, "Power Herb should consume on activation");
            break;
        }
        case ItemType::StickyBarb: {
            self->holdItem(itemType);
            self->setCurrentHP(self->getMaxHP());
            int before = self->getCurrentHP();
            item.executeTrigger(ItemTrigger::OnTurnEnd, self, other, battle);
            check(self->getCurrentHP() < before, "Sticky Barb should hurt holder at end of turn");
            break;
        }
        case ItemType::BigRoot: {
            self->holdItem(itemType);
            self->setCurrentHP(self->getMaxHP() - 20);
            int before = self->getCurrentHP();
            ItemDamageContext ctx{&grassMove, 40, before, false, true, false};
            item.executeTrigger(ItemTrigger::OnDealDamage, self, other, battle, &ctx);
            check(self->getCurrentHP() > before, "Big Root should amplify draining recovery");
            break;
        }
        case ItemType::KingsRock: {
            self->holdItem(itemType);
            other->clearStatuses();
            ItemDamageContext ctx{&groundMove, 30, other->getCurrentHP(), false, true, true};
            bool triggered = false;
            for (int i = 0; i < 200 && !triggered; ++i) {
                other->removeStatus(StatusType::Flinch);
                item.executeTrigger(ItemTrigger::OnDealDamage, self, other, battle, &ctx);
                triggered = other->hasStatus(StatusType::Flinch);
            }
            check(triggered, "King's Rock should occasionally cause flinch on damaging hits");
            break;
        }
        case ItemType::WideLens:
        case ItemType::ZoomLens:
        case ItemType::ScopeLens: {
            check(item.applyStatModifier(100.0f, ItemStatModifier::Stat::Accuracy) > 100.0f, "Accuracy item should boost accuracy");
            break;
        }
        case ItemType::QuickClaw: {
            self->holdItem(itemType);
            bool triggered = false;
            for (int i = 0; i < 120 && !triggered; ++i) {
                self->setCurrentHP(1);
                other->setCurrentHP(1);

                BattleAction slowerAction = BattleAction::makeAttack(self, other, normalMove);
                BattleAction fasterAction = BattleAction::makeAttack(other, self, normalMove);
                slowerAction.priority = 10;
                fasterAction.priority = 100;

                battle.enqueueAction(slowerAction);
                battle.enqueueAction(fasterAction);
                battle.processTurn();

                if (!self->isFainted() && other->isFainted()) {
                    triggered = true;
                }
            }

            check(triggered, "Quick Claw should occasionally let the slower holder act first");
            break;
        }
        case ItemType::ChoiceBand:
        case ItemType::ChoiceSpecs:
        case ItemType::ChoiceScarf:
        case ItemType::MuscleBand:
        case ItemType::WiseGlasses:
        case ItemType::FlameOrb:
        case ItemType::ToxicOrb:
        case ItemType::Eviolite:
        case ItemType::AssaultVest: {
            check(!item.statModifiers.empty() || item.hasTrigger(ItemTrigger::OnTurnEnd), "Item should expose passive stat or trigger behavior");
            break;
        }
        case ItemType::WhiteHerb: {
            self->holdItem(itemType);
            self->changeStatStage(StatIndex::Attack, -2);
            item.executeTrigger(ItemTrigger::OnStatChange, self, other, battle);
            check(self->getStatStage(StatIndex::Attack) == 0, "White Herb should clear negative Attack stage");
            check(self->getItemType() == ItemType::None, "White Herb should be consumed after restoring stats");

            self->holdItem(itemType);
            self->changeStatStage(StatIndex::Defense, 1);
            item.executeTrigger(ItemTrigger::OnStatChange, self, other, battle);
            check(self->getItemType() == ItemType::WhiteHerb, "White Herb should not consume without negative stat stages");
            break;
        }
        default: {
            break;
        }
    }

    out << (pass ? "测试通过" : "测试失败") << std::endl;
    return pass;
}
}

std::vector<ItemType> getAllTestItemTypes() {
    return {
        ItemType::OranBerry,
        ItemType::SitrusBerry,
        ItemType::LumBerry,
        ItemType::ChestoBerry,
        ItemType::PechaBerry,
        ItemType::RawstBerry,
        ItemType::AspearBerry,
        ItemType::PersimBerry,
        ItemType::CheriBerry,
        ItemType::FigyBerry,
        ItemType::WikiBerry,
        ItemType::MagoBerry,
        ItemType::AguavBerry,
        ItemType::IapapaBerry,
        ItemType::OccaBerry,
        ItemType::PasshoBerry,
        ItemType::WacanBerry,
        ItemType::RindoBerry,
        ItemType::YacheBerry,
        ItemType::ChopleBerry,
        ItemType::KebiaBerry,
        ItemType::ShucaBerry,
        ItemType::CobaBerry,
        ItemType::PayapaBerry,
        ItemType::TangaBerry,
        ItemType::ChartiBerry,
        ItemType::KasibBerry,
        ItemType::HabanBerry,
        ItemType::ColburBerry,
        ItemType::BabiriBerry,
        ItemType::ChilanBerry,
        ItemType::LiechiBerry,
        ItemType::GanlonBerry,
        ItemType::SalacBerry,
        ItemType::PetayaBerry,
        ItemType::ApicotBerry,
        ItemType::JabocaBerry,
        ItemType::RowapBerry,
        ItemType::Leftovers,
        ItemType::BlackSludge,
        ItemType::ShellBell,
        ItemType::ChoiceBand,
        ItemType::ChoiceSpecs,
        ItemType::ChoiceScarf,
        ItemType::QuickClaw,
        ItemType::LifeOrb,
        ItemType::ExpertBelt,
        ItemType::MuscleBand,
        ItemType::WiseGlasses,
        ItemType::LightBall,
        ItemType::QuickPowder,
        ItemType::ThickClub,
        ItemType::MetalPowder,
        ItemType::DeepSeaTooth,
        ItemType::DeepSeaScale,
        ItemType::PowerHerb,
        ItemType::StickyBarb,
        ItemType::BigRoot,
        ItemType::KingsRock,
        ItemType::WideLens,
        ItemType::ZoomLens,
        ItemType::ScopeLens,
        ItemType::SilverPowder,
        ItemType::MetalCoat,
        ItemType::HardStone,
        ItemType::MiracleSeed,
        ItemType::BlackGlasses,
        ItemType::BlackBelt,
        ItemType::Magnet,
        ItemType::MysticWater,
        ItemType::SharpBeak,
        ItemType::PoisonBarb,
        ItemType::NeverMeltIce,
        ItemType::SpellTag,
        ItemType::TwistedSpoon,
        ItemType::Charcoal,
        ItemType::DragonFang,
        ItemType::SilkScarf,
        ItemType::SeaIncense,
        ItemType::FlamePlate,
        ItemType::SplashPlate,
        ItemType::ZapPlate,
        ItemType::MeadowPlate,
        ItemType::IciclePlate,
        ItemType::FistPlate,
        ItemType::ToxicPlate,
        ItemType::EarthPlate,
        ItemType::SkyPlate,
        ItemType::MindPlate,
        ItemType::InsectPlate,
        ItemType::StonePlate,
        ItemType::SpookyPlate,
        ItemType::IronPlate,
        ItemType::FlameOrb,
        ItemType::ToxicOrb,
        ItemType::FocusSash,
        ItemType::RockyHelmet,
        ItemType::AirBalloon,
        ItemType::Eviolite,
        ItemType::AssaultVest,
        ItemType::RedCard,
        ItemType::EjectButton,
        ItemType::WhiteHerb,
        ItemType::WeaknessPolicy,
        ItemType::BerryJuice
    };
}

bool runSingleItemTest(ItemType itemType, std::ostream& out, std::ostream& err) {
    Species testSpecies{
        1,
        "Test Pokemon",
        100,
        100,
        Type::Normal,
        Type::Count,
        {100, 100, 100, 100, 100, 100},
        {EggGroup::Monster, EggGroup::Monster},
        {},
        1.0f,
        -1,
        0,
        {AbilityType::None},
        AbilityType::None
    };

    std::array<int, 6> ivs = {31, 31, 31, 31, 31, 31};
    std::array<int, 6> evs = {0, 0, 0, 0, 0, 0};

    Pokemon* self = new Pokemon(testSpecies, Nature::Hardy, AbilityType::None, false, 50, ivs, evs);
    Pokemon* other = new Pokemon(testSpecies, Nature::Hardy, AbilityType::None, false, 50, ivs, evs);
    Side sideA("Player");
    Side sideB("Opponent");
    sideA.addPokemon(self);
    sideB.addPokemon(other);
    Battle battle(sideA, sideB);

    const bool pass = runSingleItemTestWithContext(itemType, self, other, battle, out, err);

    delete self;
    delete other;
    return pass;
}

ItemTestSummary runAllItemTests(std::ostream& out, std::ostream& err) {
    ItemTestSummary summary;
    const std::vector<ItemType> itemTypes = getAllTestItemTypes();
    summary.total = static_cast<int>(itemTypes.size());

    out << "=== 开始测试所有物品 ===" << std::endl;
    out << "总共有 " << itemTypes.size() << " 个物品需要测试" << std::endl;

    auto toSlugFromDisplayName = [](const std::string& name) {
        std::string slug;
        slug.reserve(name.size());
        for (char ch : name) {
            if (ch == ' ') {
                slug.push_back('-');
            } else if (ch == '\'') {
                continue;
            } else {
                slug.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
        }
        return slug;
    };

    std::set<std::string> implemented;
    for (int i = 0; i < static_cast<int>(ItemType::Count); ++i) {
        ItemType itemType = static_cast<ItemType>(i);
        if (itemType == ItemType::None) {
            continue;
        }
        std::string slug = toSlugFromDisplayName(getItemName(itemType));
        if (!slug.empty() && slug != "none") {
            implemented.insert(slug);
        }
    }

    std::set<std::string> jsonBattleItems;
    const std::vector<std::string> jsonCandidates = {
        "data/items.json",
        "../data/items.json",
        "../../data/items.json"
    };
    for (const auto& path : jsonCandidates) {
        std::ifstream file(path);
        if (!file.is_open()) {
            continue;
        }
        nlohmann::json itemsJson;
        file >> itemsJson;
        for (const auto& entry : itemsJson["items"]) {
            const std::string name = entry.value("name", "");
            const std::string description = entry.value("description", "");
            if (!name.empty() && isBattleApplicableDescription(description)) {
                jsonBattleItems.insert(name);
            }
        }
        break;
    }

    summary.jsonBattleItemCount = static_cast<int>(jsonBattleItems.size());
    for (const auto& jsonItem : jsonBattleItems) {
        const ItemType mapped = getItemTypeByName(jsonItem);
        if (mapped == ItemType::None) {
            summary.unsupportedJsonBattleItemCount++;
        }
    }

    out << "JSON 对战道具总数: " << summary.jsonBattleItemCount << std::endl;
    out << "当前系统已实现对战道具: " << implemented.size() << std::endl;
    out << "当前系统未覆盖的 JSON 对战道具: " << summary.unsupportedJsonBattleItemCount << std::endl;

    Species testSpecies{
        1,
        "Test Pokemon",
        100,
        100,
        Type::Normal,
        Type::Count,
        {100, 100, 100, 100, 100, 100},
        {EggGroup::Monster, EggGroup::Monster},
        {},
        1.0f,
        -1,
        0,
        {AbilityType::None},
        AbilityType::None
    };

    std::array<int, 6> ivs = {31, 31, 31, 31, 31, 31};
    std::array<int, 6> evs = {0, 0, 0, 0, 0, 0};

    Pokemon* self = new Pokemon(testSpecies, Nature::Hardy, AbilityType::None, false, 50, ivs, evs);
    Pokemon* other = new Pokemon(testSpecies, Nature::Hardy, AbilityType::None, false, 50, ivs, evs);
    Side sideA("Player");
    Side sideB("Opponent");
    sideA.addPokemon(self);
    sideB.addPokemon(other);
    Battle battle(sideA, sideB);

    int testCount = 1;
    for (ItemType itemType : itemTypes) {
        out << "\n=== 测试物品 " << testCount << "/" << itemTypes.size() << ": " << getItemName(itemType) << " ===" << std::endl;
        const bool pass = runSingleItemTestWithContext(itemType, self, other, battle, out, err);
        if (pass) {
            summary.passed++;
        } else {
            summary.failedItemNames.push_back(getItemName(itemType));
        }
        resetPokemon(self, other);
        ++testCount;
    }

    delete self;
    delete other;

    summary.failed = summary.total - summary.passed;

    out << "\n=== 所有物品测试完成 ===" << std::endl;
    out << "总共测试了 " << summary.total << " 个物品" << std::endl;
    out << "通过: " << summary.passed << "，失败: " << summary.failed << std::endl;

    return summary;
}
