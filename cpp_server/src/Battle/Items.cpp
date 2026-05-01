#include "Battle/Items.h"
#include "Battle/Pokemon.h"
#include "Battle/Battle.h"
#include "Battle/PRNG.h"
#include "Battle/Types.h"
#include "Battle/Status.h"
#include <algorithm>

namespace {
const ItemDamageContext* toDamageContext(void* context) {
    return static_cast<const ItemDamageContext*>(context);
}

bool tookDamageFromDamagingMove(const ItemDamageContext* ctx) {
    return ctx && ctx->isDamagingMove && ctx->damage > 0;
}
}

Item getItem(ItemType type) {
    Item item;
    switch (type) {
        case ItemType::OranBerry:
            item = createOranBerry();
            break;

        case ItemType::SitrusBerry:
            item = createSitrusBerry();
            break;

        case ItemType::LumBerry:
            item = createLumBerry();
            break;

        case ItemType::ChestoBerry:
            item = createChestoBerry();
            break;

        case ItemType::PechaBerry:
            item = createPechaBerry();
            break;

        case ItemType::RawstBerry:
            item = createRawstBerry();
            break;

        case ItemType::AspearBerry:
            item = createAspearBerry();
            break;

        case ItemType::PersimBerry:
            item = createPersimBerry();
            break;

        case ItemType::CheriBerry:
            item = createCheriBerry();
            break;

        case ItemType::FigyBerry:
            item = createFigyBerry();
            break;

        case ItemType::WikiBerry:
            item = createWikiBerry();
            break;

        case ItemType::MagoBerry:
            item = createMagoBerry();
            break;

        case ItemType::AguavBerry:
            item = createAguavBerry();
            break;

        case ItemType::IapapaBerry:
            item = createIapapaBerry();
            break;

        case ItemType::OccaBerry:
            item = createOccaBerry();
            break;

        case ItemType::PasshoBerry:
            item = createPasshoBerry();
            break;

        case ItemType::WacanBerry:
            item = createWacanBerry();
            break;

        case ItemType::RindoBerry:
            item = createRindoBerry();
            break;

        case ItemType::YacheBerry:
            item = createYacheBerry();
            break;

        case ItemType::ChopleBerry:
            item = createChopleBerry();
            break;

        case ItemType::KebiaBerry:
            item = createKebiaBerry();
            break;

        case ItemType::ShucaBerry:
            item = createShucaBerry();
            break;

        case ItemType::CobaBerry:
            item = createCobaBerry();
            break;

        case ItemType::PayapaBerry:
            item = createPayapaBerry();
            break;

        case ItemType::TangaBerry:
            item = createTangaBerry();
            break;

        case ItemType::ChartiBerry:
            item = createChartiBerry();
            break;

        case ItemType::KasibBerry:
            item = createKasibBerry();
            break;

        case ItemType::HabanBerry:
            item = createHabanBerry();
            break;

        case ItemType::ColburBerry:
            item = createColburBerry();
            break;

        case ItemType::BabiriBerry:
            item = createBabiriBerry();
            break;

        case ItemType::ChilanBerry:
            item = createChilanBerry();
            break;

        case ItemType::LiechiBerry:
            item = createLiechiBerry();
            break;

        case ItemType::GanlonBerry:
            item = createGanlonBerry();
            break;

        case ItemType::SalacBerry:
            item = createSalacBerry();
            break;

        case ItemType::PetayaBerry:
            item = createPetayaBerry();
            break;

        case ItemType::ApicotBerry:
            item = createApicotBerry();
            break;

        case ItemType::JabocaBerry:
            item = createJabocaBerry();
            break;

        case ItemType::RowapBerry:
            item = createRowapBerry();
            break;

        case ItemType::Leftovers:
            item = createLeftovers();
            break;

        case ItemType::BlackSludge:
            item = createBlackSludge();
            break;

        case ItemType::ShellBell:
            item = createShellBell();
            break;

        case ItemType::ChoiceBand:
            item = createChoiceBand();
            break;

        case ItemType::ChoiceSpecs:
            item = createChoiceSpecs();
            break;

        case ItemType::ChoiceScarf:
            item = createChoiceScarf();
            break;

        case ItemType::QuickClaw:
            item = createQuickClaw();
            break;

        case ItemType::LifeOrb:
            item = createLifeOrb();
            break;

        case ItemType::ExpertBelt:
            item = createExpertBelt();
            break;

        case ItemType::MuscleBand:
            item = createMuscleBand();
            break;

        case ItemType::WiseGlasses:
            item = createWiseGlasses();
            break;

        case ItemType::LightBall:
            item = createLightBall();
            break;
        case ItemType::QuickPowder:
            item = createQuickPowder();
            break;
        case ItemType::ThickClub:
            item = createThickClub();
            break;
        case ItemType::MetalPowder:
            item = createMetalPowder();
            break;
        case ItemType::DeepSeaTooth:
            item = createDeepSeaTooth();
            break;
        case ItemType::DeepSeaScale:
            item = createDeepSeaScale();
            break;
        case ItemType::PowerHerb:
            item = createPowerHerb();
            break;
        case ItemType::StickyBarb:
            item = createStickyBarb();
            break;
        case ItemType::BigRoot:
            item = createBigRoot();
            break;
        case ItemType::KingsRock:
            item = createKingsRock();
            break;
        case ItemType::WideLens:
            item = createWideLens();
            break;
        case ItemType::ZoomLens:
            item = createZoomLens();
            break;
        case ItemType::ScopeLens:
            item = createScopeLens();
            break;

        case ItemType::SilverPowder:
            item = createSilverPowder();
            break;
        case ItemType::MetalCoat:
            item = createMetalCoat();
            break;
        case ItemType::HardStone:
            item = createHardStone();
            break;
        case ItemType::MiracleSeed:
            item = createMiracleSeed();
            break;
        case ItemType::BlackGlasses:
            item = createBlackGlasses();
            break;
        case ItemType::BlackBelt:
            item = createBlackBelt();
            break;
        case ItemType::Magnet:
            item = createMagnet();
            break;
        case ItemType::MysticWater:
            item = createMysticWater();
            break;
        case ItemType::SharpBeak:
            item = createSharpBeak();
            break;
        case ItemType::PoisonBarb:
            item = createPoisonBarb();
            break;
        case ItemType::NeverMeltIce:
            item = createNeverMeltIce();
            break;
        case ItemType::SpellTag:
            item = createSpellTag();
            break;
        case ItemType::TwistedSpoon:
            item = createTwistedSpoon();
            break;
        case ItemType::Charcoal:
            item = createCharcoal();
            break;
        case ItemType::DragonFang:
            item = createDragonFang();
            break;
        case ItemType::SilkScarf:
            item = createSilkScarf();
            break;
        case ItemType::SeaIncense:
            item = createSeaIncense();
            break;
        case ItemType::FlamePlate:
            item = createFlamePlate();
            break;
        case ItemType::SplashPlate:
            item = createSplashPlate();
            break;
        case ItemType::ZapPlate:
            item = createZapPlate();
            break;
        case ItemType::MeadowPlate:
            item = createMeadowPlate();
            break;
        case ItemType::IciclePlate:
            item = createIciclePlate();
            break;
        case ItemType::FistPlate:
            item = createFistPlate();
            break;
        case ItemType::ToxicPlate:
            item = createToxicPlate();
            break;
        case ItemType::EarthPlate:
            item = createEarthPlate();
            break;
        case ItemType::SkyPlate:
            item = createSkyPlate();
            break;
        case ItemType::MindPlate:
            item = createMindPlate();
            break;
        case ItemType::InsectPlate:
            item = createInsectPlate();
            break;
        case ItemType::StonePlate:
            item = createStonePlate();
            break;
        case ItemType::SpookyPlate:
            item = createSpookyPlate();
            break;
        case ItemType::IronPlate:
            item = createIronPlate();
            break;
        case ItemType::FlameOrb:
            item = createFlameOrb();
            break;
        case ItemType::ToxicOrb:
            item = createToxicOrb();
            break;

        case ItemType::FocusSash:
            item = createFocusSash();
            break;

        case ItemType::RockyHelmet:
            item = createRockyHelmet();
            break;

        case ItemType::AirBalloon:
            item = createAirBalloon();
            break;

        case ItemType::Eviolite:
            item = createEviolite();
            break;

        case ItemType::AssaultVest:
            item = createAssaultVest();
            break;

        case ItemType::RedCard:
            item = createRedCard();
            break;

        case ItemType::EjectButton:
            item = createEjectButton();
            break;

        case ItemType::WhiteHerb:
            item = createWhiteHerb();
            break;

        case ItemType::WeaknessPolicy:
            item = createWeaknessPolicy();
            break;

        case ItemType::BerryJuice:
            item = createBerryJuice();
            break;

        default:
            item = Item(ItemType::None, "None");
            break;
    }
    return item;
}

std::string getItemName(ItemType type) {
    switch (type) {
        case ItemType::OranBerry: return "Oran Berry";
        case ItemType::SitrusBerry: return "Sitrus Berry";
        case ItemType::LumBerry: return "Lum Berry";
        case ItemType::ChestoBerry: return "Chesto Berry";
        case ItemType::PechaBerry: return "Pecha Berry";
        case ItemType::RawstBerry: return "Rawst Berry";
        case ItemType::AspearBerry: return "Aspear Berry";
        case ItemType::PersimBerry: return "Persim Berry";
        case ItemType::CheriBerry: return "Cheri Berry";
        case ItemType::FigyBerry: return "Figy Berry";
        case ItemType::WikiBerry: return "Wiki Berry";
        case ItemType::MagoBerry: return "Mago Berry";
        case ItemType::AguavBerry: return "Aguav Berry";
        case ItemType::IapapaBerry: return "Iapapa Berry";
        case ItemType::OccaBerry: return "Occa Berry";
        case ItemType::PasshoBerry: return "Passho Berry";
        case ItemType::WacanBerry: return "Wacan Berry";
        case ItemType::RindoBerry: return "Rindo Berry";
        case ItemType::YacheBerry: return "Yache Berry";
        case ItemType::ChopleBerry: return "Chople Berry";
        case ItemType::KebiaBerry: return "Kebia Berry";
        case ItemType::ShucaBerry: return "Shuca Berry";
        case ItemType::CobaBerry: return "Coba Berry";
        case ItemType::PayapaBerry: return "Payapa Berry";
        case ItemType::TangaBerry: return "Tanga Berry";
        case ItemType::ChartiBerry: return "Charti Berry";
        case ItemType::KasibBerry: return "Kasib Berry";
        case ItemType::HabanBerry: return "Haban Berry";
        case ItemType::ColburBerry: return "Colbur Berry";
        case ItemType::BabiriBerry: return "Babiri Berry";
        case ItemType::ChilanBerry: return "Chilan Berry";
        case ItemType::LiechiBerry: return "Liechi Berry";
        case ItemType::GanlonBerry: return "Ganlon Berry";
        case ItemType::SalacBerry: return "Salac Berry";
        case ItemType::PetayaBerry: return "Petaya Berry";
        case ItemType::ApicotBerry: return "Apicot Berry";
        case ItemType::JabocaBerry: return "Jaboca Berry";
        case ItemType::RowapBerry: return "Rowap Berry";
        case ItemType::Leftovers: return "Leftovers";
        case ItemType::BlackSludge: return "Black Sludge";
        case ItemType::ShellBell: return "Shell Bell";
        case ItemType::ChoiceBand: return "Choice Band";
        case ItemType::ChoiceSpecs: return "Choice Specs";
        case ItemType::ChoiceScarf: return "Choice Scarf";
        case ItemType::QuickClaw: return "Quick Claw";
        case ItemType::LifeOrb: return "Life Orb";
        case ItemType::ExpertBelt: return "Expert Belt";
        case ItemType::MuscleBand: return "Muscle Band";
        case ItemType::WiseGlasses: return "Wise Glasses";
        case ItemType::LightBall: return "Light Ball";
        case ItemType::QuickPowder: return "Quick Powder";
        case ItemType::ThickClub: return "Thick Club";
        case ItemType::MetalPowder: return "Metal Powder";
        case ItemType::DeepSeaTooth: return "Deep Sea Tooth";
        case ItemType::DeepSeaScale: return "Deep Sea Scale";
        case ItemType::PowerHerb: return "Power Herb";
        case ItemType::StickyBarb: return "Sticky Barb";
        case ItemType::BigRoot: return "Big Root";
        case ItemType::KingsRock: return "King's Rock";
        case ItemType::WideLens: return "Wide Lens";
        case ItemType::ZoomLens: return "Zoom Lens";
        case ItemType::ScopeLens: return "Scope Lens";
        case ItemType::SilverPowder: return "Silver Powder";
        case ItemType::MetalCoat: return "Metal Coat";
        case ItemType::HardStone: return "Hard Stone";
        case ItemType::MiracleSeed: return "Miracle Seed";
        case ItemType::BlackGlasses: return "Black Glasses";
        case ItemType::BlackBelt: return "Black Belt";
        case ItemType::Magnet: return "Magnet";
        case ItemType::MysticWater: return "Mystic Water";
        case ItemType::SharpBeak: return "Sharp Beak";
        case ItemType::PoisonBarb: return "Poison Barb";
        case ItemType::NeverMeltIce: return "Never-Melt Ice";
        case ItemType::SpellTag: return "Spell Tag";
        case ItemType::TwistedSpoon: return "Twisted Spoon";
        case ItemType::Charcoal: return "Charcoal";
        case ItemType::DragonFang: return "Dragon Fang";
        case ItemType::SilkScarf: return "Silk Scarf";
        case ItemType::SeaIncense: return "Sea Incense";
        case ItemType::FlamePlate: return "Flame Plate";
        case ItemType::SplashPlate: return "Splash Plate";
        case ItemType::ZapPlate: return "Zap Plate";
        case ItemType::MeadowPlate: return "Meadow Plate";
        case ItemType::IciclePlate: return "Icicle Plate";
        case ItemType::FistPlate: return "Fist Plate";
        case ItemType::ToxicPlate: return "Toxic Plate";
        case ItemType::EarthPlate: return "Earth Plate";
        case ItemType::SkyPlate: return "Sky Plate";
        case ItemType::MindPlate: return "Mind Plate";
        case ItemType::InsectPlate: return "Insect Plate";
        case ItemType::StonePlate: return "Stone Plate";
        case ItemType::SpookyPlate: return "Spooky Plate";
        case ItemType::IronPlate: return "Iron Plate";
        case ItemType::FlameOrb: return "Flame Orb";
        case ItemType::ToxicOrb: return "Toxic Orb";
        case ItemType::FocusSash: return "Focus Sash";
        case ItemType::RockyHelmet: return "Rocky Helmet";
        case ItemType::AirBalloon: return "Air Balloon";
        case ItemType::Eviolite: return "Eviolite";
        case ItemType::AssaultVest: return "Assault Vest";
        case ItemType::RedCard: return "Red Card";
        case ItemType::EjectButton: return "Eject Button";
        case ItemType::WhiteHerb: return "White Herb";
        case ItemType::WeaknessPolicy: return "Weakness Policy";
        case ItemType::BerryJuice: return "Berry Juice";
        default: return "None";
    }
}

Item createOranBerry() {
    Item item(ItemType::OranBerry, "Oran Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        if (self->getCurrentHP() <= self->getMaxHP() / 2) {
            self->setCurrentHP(self->getCurrentHP() + 10);
            self->removeItem();
        }
    });
    return item;
}

Item createSitrusBerry() {
    Item item(ItemType::SitrusBerry, "Sitrus Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        if (self->getCurrentHP() <= self->getMaxHP() / 2) {
            int heal = std::max(1, self->getMaxHP() / 4);
            self->setCurrentHP(self->getCurrentHP() + heal);
            self->removeItem();
        }
    });
    return item;
}

Item createLumBerry() {
    Item item(ItemType::LumBerry, "Lum Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatus, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        // 清除所有异常状态
        self->clearStatuses();
        self->removeItem();
    });
    return item;
}

Item createLeftovers() {
    Item item(ItemType::Leftovers, "Leftovers");
    item.addEffect(ItemTrigger::OnTurnEnd, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        int heal = std::max(1, self->getMaxHP() / 16);
        self->setCurrentHP(self->getCurrentHP() + heal);
    });
    return item;
}

Item createChoiceBand() {
    Item item(ItemType::ChoiceBand, "Choice Band");
    item.addEffect(ItemTrigger::OnEntry, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
    });
    item.addStatModifier(ItemStatModifier::Stat::Attack, 1.5f);
    return item;
}

Item createChoiceSpecs() {
    Item item(ItemType::ChoiceSpecs, "Choice Specs");
    item.addEffect(ItemTrigger::OnEntry, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
    });
    item.addStatModifier(ItemStatModifier::Stat::SpAttack, 1.5f);
    return item;
}

Item createChoiceScarf() {
    Item item(ItemType::ChoiceScarf, "Choice Scarf");
    item.addEffect(ItemTrigger::OnEntry, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
    });
    item.addStatModifier(ItemStatModifier::Stat::Speed, 1.5f);
    return item;
}

Item createQuickClaw() {
    Item item(ItemType::QuickClaw, "Quick Claw");
    return item;
}

Item createLifeOrb() {
    Item item(ItemType::LifeOrb, "Life Orb");
    item.addEffect(ItemTrigger::OnDealDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!tookDamageFromDamagingMove(damageContext)) {
            return;
        }
        int recoil = std::max(1, self->getMaxHP() / 10);
        self->setCurrentHP(self->getCurrentHP() - recoil);
    });
    item.setDamageModifier(1.3f, true);
    return item;
}

Item createFocusSash() {
    Item item(ItemType::FocusSash, "Focus Sash");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->isDamagingMove) {
            return;
        }
        if (damageContext->hpBeforeDamage >= self->getMaxHP() && self->getCurrentHP() <= 0) {
            self->setCurrentHP(1);
            self->removeItem();
        }
    });
    return item;
}

Item createRockyHelmet() {
    Item item(ItemType::RockyHelmet, "Rocky Helmet");
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self || !opponent) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->isContact || damageContext->damage <= 0) {
            return;
        }
        int reflectedDamage = std::max(1, opponent->getMaxHP() / 6);
        opponent->setCurrentHP(opponent->getCurrentHP() - reflectedDamage);
    });
    return item;
}

Item createAirBalloon() {
    Item item(ItemType::AirBalloon, "Air Balloon");
    item.isConsumable = true;
    item.setDamageModifier(0.0f, false, [](Pokemon* self, Pokemon* opponent, const Move& move) {
        return self && move.getType() == Type::Ground;
    });
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (tookDamageFromDamagingMove(damageContext)) {
            self->removeItem();
        }
    });
    return item;
}

Item createEviolite() {
    Item item(ItemType::Eviolite, "Eviolite");
    item.addEffect(ItemTrigger::OnEntry, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
    });
    item.addStatModifier(ItemStatModifier::Stat::Defense, 1.5f);
    item.addStatModifier(ItemStatModifier::Stat::SpDefense, 1.5f);
    return item;
}

Item createAssaultVest() {
    Item item(ItemType::AssaultVest, "Assault Vest");
    item.addEffect(ItemTrigger::OnEntry, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
    });
    item.addStatModifier(ItemStatModifier::Stat::SpDefense, 1.5f);
    return item;
}

Item createRedCard() {
    Item item(ItemType::RedCard, "Red Card");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self || !opponent) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!tookDamageFromDamagingMove(damageContext) || self->isFainted()) {
            return;
        }
        Side* opponentSide = Battle::findSideForPokemon(battle, opponent);
        if (opponentSide) {
            opponentSide->autoSwitchNext();
            self->removeItem();
        }
    });
    return item;
}

Item createWeaknessPolicy() {
    Item item(ItemType::WeaknessPolicy, "Weakness Policy");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->wasSuperEffective || damageContext->damage <= 0) {
            return;
        }
        self->changeStatStage(StatIndex::Attack, 2);
        self->changeStatStage(StatIndex::SpecialAttack, 2);
        self->removeItem();
    });
    return item;
}

Item createBlackSludge() {
    Item item(ItemType::BlackSludge, "Black Sludge");
    item.addEffect(ItemTrigger::OnTurnEnd, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        bool isPoisonType = false;
        // 检查宝可梦是否为毒属性
        if (self->getType1() == Type::Poison || self->getType2() == Type::Poison) {
            isPoisonType = true;
        }
        if (isPoisonType) {
            // 毒属性宝可梦回复1/16最大HP
            int heal = std::max(1, self->getMaxHP() / 16);
            self->setCurrentHP(self->getCurrentHP() + heal);
        } else {
            // 非毒属性宝可梦受到1/8最大HP伤害
            int damage = self->getMaxHP() / 8;
            self->setCurrentHP(self->getCurrentHP() - damage);
        }
    });
    return item;
}

Item createShellBell() {
    Item item(ItemType::ShellBell, "Shell Bell");
    item.addEffect(ItemTrigger::OnDealDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self || !opponent) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!tookDamageFromDamagingMove(damageContext)) {
            return;
        }
        int heal = damageContext->damage / 8;
        if (heal > 0) {
            self->setCurrentHP(self->getCurrentHP() + heal);
        }
    });
    return item;
}

Item createExpertBelt() {
    Item item(ItemType::ExpertBelt, "Expert Belt");
    item.setDamageModifier(1.2f, true, [](Pokemon* self, Pokemon* opponent, const Move& move) {
        if (!self || !opponent) return false;
        return opponent->getTypeEffectiveness(move.getType()) > 1.0f;
    });
    return item;
}

Item createMuscleBand() {
    Item item(ItemType::MuscleBand, "Muscle Band");
    item.addStatModifier(ItemStatModifier::Stat::Attack, 1.1f);
    return item;
}

Item createWiseGlasses() {
    Item item(ItemType::WiseGlasses, "Wise Glasses");
    item.addStatModifier(ItemStatModifier::Stat::SpAttack, 1.1f);
    return item;
}

Item createLightBall() {
    Item item(ItemType::LightBall, "Light Ball");
    item.addStatModifier(ItemStatModifier::Stat::Attack, 2.0f);
    item.addStatModifier(ItemStatModifier::Stat::SpAttack, 2.0f);
    return item;
}

Item createQuickPowder() {
    Item item(ItemType::QuickPowder, "Quick Powder");
    item.addStatModifier(ItemStatModifier::Stat::Speed, 2.0f);
    return item;
}

Item createThickClub() {
    Item item(ItemType::ThickClub, "Thick Club");
    item.addStatModifier(ItemStatModifier::Stat::Attack, 2.0f);
    return item;
}

Item createMetalPowder() {
    Item item(ItemType::MetalPowder, "Metal Powder");
    item.addStatModifier(ItemStatModifier::Stat::Defense, 1.5f);
    item.addStatModifier(ItemStatModifier::Stat::SpDefense, 1.5f);
    return item;
}

Item createDeepSeaTooth() {
    Item item(ItemType::DeepSeaTooth, "Deep Sea Tooth");
    item.addStatModifier(ItemStatModifier::Stat::SpAttack, 2.0f);
    return item;
}

Item createDeepSeaScale() {
    Item item(ItemType::DeepSeaScale, "Deep Sea Scale");
    item.addStatModifier(ItemStatModifier::Stat::SpDefense, 2.0f);
    return item;
}

Item createPowerHerb() {
    Item item(ItemType::PowerHerb, "Power Herb");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnAttack, [](Pokemon* self, Pokemon*, Battle&, void*) {
        if (self) {
            self->removeItem();
        }
    });
    item.addEffect(ItemTrigger::OnDealDamage, [](Pokemon* self, Pokemon* opponent, Battle&, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->move) return;
        if (damageContext->move->getName() == "Fly" || damageContext->move->getName() == "Protect") {
            self->removeItem();
        }
    });
    return item;
}

Item createStickyBarb() {
    Item item(ItemType::StickyBarb, "Sticky Barb");
    item.addEffect(ItemTrigger::OnTurnEnd, [](Pokemon* self, Pokemon*, Battle&, void*) {
        if (!self) return;
        int damage = std::max(1, self->getMaxHP() / 8);
        self->setCurrentHP(self->getCurrentHP() - damage);
    });
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle&, void* context) {
        if (!self || !opponent) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->isContact || damageContext->damage <= 0) return;
        int damage = std::max(1, opponent->getMaxHP() / 8);
        opponent->setCurrentHP(opponent->getCurrentHP() - damage);
    });
    return item;
}

Item createBigRoot() {
    Item item(ItemType::BigRoot, "Big Root");
    item.addEffect(ItemTrigger::OnDealDamage, [](Pokemon* self, Pokemon*, Battle&, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || damageContext->damage <= 0) return;
        int heal = std::max(1, damageContext->damage * 3 / 10);
        self->setCurrentHP(self->getCurrentHP() + heal);
    });
    return item;
}

Item createKingsRock() {
    Item item(ItemType::KingsRock, "King's Rock");
    item.addEffect(ItemTrigger::OnDealDamage, [](Pokemon* self, Pokemon* opponent, Battle&, void* context) {
        if (!self || !opponent) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->isDamagingMove || damageContext->damage <= 0) return;
        if (!damageContext->move || damageContext->move->getEffect() == MoveEffect::Flinch) {
            return;
        }
        if (opponent->getAbility() == AbilityType::InnerFocus) {
            return;
        }
        if (PRNG::nextInt(0, 100) < 10) {
            opponent->addStatus(StatusType::Flinch, 1);
        }
    });
    return item;
}

Item createWideLens() {
    Item item(ItemType::WideLens, "Wide Lens");
    item.addStatModifier(ItemStatModifier::Stat::Accuracy, 1.1f);
    return item;
}

Item createZoomLens() {
    Item item(ItemType::ZoomLens, "Zoom Lens");
    item.addStatModifier(ItemStatModifier::Stat::Accuracy, 1.2f);
    return item;
}

Item createScopeLens() {
    Item item(ItemType::ScopeLens, "Scope Lens");
    item.addStatModifier(ItemStatModifier::Stat::Accuracy, 1.1f);
    return item;
}

namespace {
Item createTypeBoostItem(ItemType type, const std::string& name, Type boostedType) {
    Item item(type, name);
    item.setDamageModifier(1.2f, true, [boostedType](Pokemon*, Pokemon*, const Move& move) {
        return move.getType() == boostedType;
    });
    return item;
}
}

Item createSilverPowder() { return createTypeBoostItem(ItemType::SilverPowder, "Silver Powder", Type::Bug); }
Item createMetalCoat() { return createTypeBoostItem(ItemType::MetalCoat, "Metal Coat", Type::Steel); }
Item createHardStone() { return createTypeBoostItem(ItemType::HardStone, "Hard Stone", Type::Rock); }
Item createMiracleSeed() { return createTypeBoostItem(ItemType::MiracleSeed, "Miracle Seed", Type::Grass); }
Item createBlackGlasses() { return createTypeBoostItem(ItemType::BlackGlasses, "Black Glasses", Type::Dark); }
Item createBlackBelt() { return createTypeBoostItem(ItemType::BlackBelt, "Black Belt", Type::Fighting); }
Item createMagnet() { return createTypeBoostItem(ItemType::Magnet, "Magnet", Type::Electric); }
Item createMysticWater() { return createTypeBoostItem(ItemType::MysticWater, "Mystic Water", Type::Water); }
Item createSharpBeak() { return createTypeBoostItem(ItemType::SharpBeak, "Sharp Beak", Type::Flying); }
Item createPoisonBarb() { return createTypeBoostItem(ItemType::PoisonBarb, "Poison Barb", Type::Poison); }
Item createNeverMeltIce() { return createTypeBoostItem(ItemType::NeverMeltIce, "Never-Melt Ice", Type::Ice); }
Item createSpellTag() { return createTypeBoostItem(ItemType::SpellTag, "Spell Tag", Type::Ghost); }
Item createTwistedSpoon() { return createTypeBoostItem(ItemType::TwistedSpoon, "Twisted Spoon", Type::Psychic); }
Item createCharcoal() { return createTypeBoostItem(ItemType::Charcoal, "Charcoal", Type::Fire); }
Item createDragonFang() { return createTypeBoostItem(ItemType::DragonFang, "Dragon Fang", Type::Dragon); }
Item createSilkScarf() { return createTypeBoostItem(ItemType::SilkScarf, "Silk Scarf", Type::Normal); }
Item createSeaIncense() { return createTypeBoostItem(ItemType::SeaIncense, "Sea Incense", Type::Water); }
Item createFlamePlate() { return createTypeBoostItem(ItemType::FlamePlate, "Flame Plate", Type::Fire); }
Item createSplashPlate() { return createTypeBoostItem(ItemType::SplashPlate, "Splash Plate", Type::Water); }
Item createZapPlate() { return createTypeBoostItem(ItemType::ZapPlate, "Zap Plate", Type::Electric); }
Item createMeadowPlate() { return createTypeBoostItem(ItemType::MeadowPlate, "Meadow Plate", Type::Grass); }
Item createIciclePlate() { return createTypeBoostItem(ItemType::IciclePlate, "Icicle Plate", Type::Ice); }
Item createFistPlate() { return createTypeBoostItem(ItemType::FistPlate, "Fist Plate", Type::Fighting); }
Item createToxicPlate() { return createTypeBoostItem(ItemType::ToxicPlate, "Toxic Plate", Type::Poison); }
Item createEarthPlate() { return createTypeBoostItem(ItemType::EarthPlate, "Earth Plate", Type::Ground); }
Item createSkyPlate() { return createTypeBoostItem(ItemType::SkyPlate, "Sky Plate", Type::Flying); }
Item createMindPlate() { return createTypeBoostItem(ItemType::MindPlate, "Mind Plate", Type::Psychic); }
Item createInsectPlate() { return createTypeBoostItem(ItemType::InsectPlate, "Insect Plate", Type::Bug); }
Item createStonePlate() { return createTypeBoostItem(ItemType::StonePlate, "Stone Plate", Type::Rock); }
Item createSpookyPlate() { return createTypeBoostItem(ItemType::SpookyPlate, "Spooky Plate", Type::Ghost); }
Item createIronPlate() { return createTypeBoostItem(ItemType::IronPlate, "Iron Plate", Type::Steel); }

Item createFlameOrb() {
    Item item(ItemType::FlameOrb, "Flame Orb");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnTurnEnd, [](Pokemon* self, Pokemon*, Battle&, void*) {
        if (self && !self->hasStatus(StatusType::Burn)) {
            self->addStatus(StatusType::Burn);
            self->removeItem();
        }
    });
    return item;
}

Item createToxicOrb() {
    Item item(ItemType::ToxicOrb, "Toxic Orb");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnTurnEnd, [](Pokemon* self, Pokemon*, Battle&, void*) {
        if (self && !self->hasStatus(StatusType::ToxicPoison)) {
            self->addStatus(StatusType::ToxicPoison);
            self->removeItem();
        }
    });
    return item;
}

Item createEjectButton() {
    Item item(ItemType::EjectButton, "Eject Button");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!tookDamageFromDamagingMove(damageContext) || self->isFainted()) {
            return;
        }
        Side* selfSide = Battle::findSideForPokemon(battle, self);
        if (selfSide) {
            selfSide->autoSwitchNext();
            self->removeItem();
        }
    });
    return item;
}

Item createWhiteHerb() {
    Item item(ItemType::WhiteHerb, "White Herb");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatChange, [](Pokemon* self, Pokemon*, Battle&, void*) {
        if (!self) return;

        const StatIndex stats[] = {
            StatIndex::Attack,
            StatIndex::Defense,
            StatIndex::SpecialAttack,
            StatIndex::SpecialDefense,
            StatIndex::Speed
        };

        bool restored = false;
        for (StatIndex stat : stats) {
            const int stage = self->getStatStage(stat);
            if (stage < 0) {
                self->changeStatStage(stat, -stage);
                restored = true;
            }
        }

        if (restored) {
            self->removeItem();
        }
    });
    return item;
}

Item createBerryJuice() {
    Item item(ItemType::BerryJuice, "Berry Juice");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        if (self->getCurrentHP() <= self->getMaxHP() / 2) {
            self->setCurrentHP(self->getCurrentHP() + 20);
            self->removeItem();
        }
    });
    return item;
}

Item createChestoBerry() {
    Item item(ItemType::ChestoBerry, "Chesto Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatus, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        // 当睡眠时唤醒
        if (self->hasStatus(StatusType::Sleep)) {
            self->removeStatus(StatusType::Sleep);
            self->removeItem();
        }
    });
    return item;
}

Item createPechaBerry() {
    Item item(ItemType::PechaBerry, "Pecha Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatus, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        // 当中毒时解毒
        if (self->hasStatus(StatusType::Poison) || self->hasStatus(StatusType::ToxicPoison)) {
            self->removeStatus(StatusType::Poison);
            self->removeStatus(StatusType::ToxicPoison);
            self->removeItem();
        }
    });
    return item;
}

Item createRawstBerry() {
    Item item(ItemType::RawstBerry, "Rawst Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatus, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        // 当烧伤时解除烧伤
        if (self->hasStatus(StatusType::Burn)) {
            self->removeStatus(StatusType::Burn);
            self->removeItem();
        }
    });
    return item;
}

Item createAspearBerry() {
    Item item(ItemType::AspearBerry, "Aspear Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatus, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        // 当冰冻时解冻
        if (self->hasStatus(StatusType::Freeze)) {
            self->removeStatus(StatusType::Freeze);
            self->removeItem();
        }
    });
    return item;
}

Item createPersimBerry() {
    Item item(ItemType::PersimBerry, "Persim Berry");
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatus, [](Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
        if (!self) return;
        // 当混乱时解除混乱
        if (self->hasStatus(StatusType::Confusion)) {
            self->removeStatus(StatusType::Confusion);
            self->removeItem();
        }
    });
    return item;
}

namespace {
Item createStatusCureBerry(ItemType type, const std::string& name, StatusType status) {
    Item item(type, name);
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnStatus, [status](Pokemon* self, Pokemon*, Battle&, void*) {
        if (!self || !self->hasStatus(status)) return;
        self->removeStatus(status);
        self->removeItem();
    });
    return item;
}

Item createHalfHpHealBerry(ItemType type, const std::string& name, int numerator, int denominator) {
    Item item(type, name);
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [numerator, denominator](Pokemon* self, Pokemon*, Battle&, void*) {
        if (!self) return;
        if (self->getCurrentHP() > self->getMaxHP() / 2) return;
        int heal = std::max(1, self->getMaxHP() * numerator / denominator);
        self->setCurrentHP(self->getCurrentHP() + heal);
        self->removeItem();
    });
    return item;
}

Item createResistBerry(ItemType type, const std::string& name, Type resistedType) {
    Item item(type, name);
    item.isConsumable = true;
    item.setDamageModifier(0.5f, false, [resistedType](Pokemon* self, Pokemon*, const Move& move) {
        if (!self || move.getType() != resistedType) return false;
        return self->getTypeEffectiveness(move.getType()) > 1.0f;
    });
    item.addEffect(ItemTrigger::OnDamage, [resistedType](Pokemon* self, Pokemon*, Battle&, void* context) {
        if (!self) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->move || damageContext->damage <= 0) return;
        if (damageContext->move->getType() != resistedType) return;
        if (!damageContext->wasSuperEffective) return;
        self->removeItem();
    });
    return item;
}

Item createPinchStatBerry(ItemType type, const std::string& name, StatIndex stat) {
    Item item(type, name);
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [stat](Pokemon* self, Pokemon*, Battle&, void*) {
        if (!self) return;
        if (self->getCurrentHP() > self->getMaxHP() / 4) return;
        self->changeStatStage(stat, 1);
        self->removeItem();
    });
    return item;
}

Item createRetaliationBerry(ItemType type, const std::string& name, Category triggerCategory) {
    Item item(type, name);
    item.isConsumable = true;
    item.addEffect(ItemTrigger::OnDamage, [triggerCategory](Pokemon* self, Pokemon* opponent, Battle&, void* context) {
        if (!self || !opponent) return;
        const ItemDamageContext* damageContext = toDamageContext(context);
        if (!damageContext || !damageContext->move || damageContext->damage <= 0) return;
        if (damageContext->move->getCategory() != triggerCategory) return;
        int reflectedDamage = std::max(1, opponent->getMaxHP() / 8);
        opponent->setCurrentHP(opponent->getCurrentHP() - reflectedDamage);
        self->removeItem();
    });
    return item;
}
} // namespace

Item createCheriBerry() { return createStatusCureBerry(ItemType::CheriBerry, "Cheri Berry", StatusType::Paralysis); }
Item createFigyBerry() { return createHalfHpHealBerry(ItemType::FigyBerry, "Figy Berry", 1, 8); }
Item createWikiBerry() { return createHalfHpHealBerry(ItemType::WikiBerry, "Wiki Berry", 1, 8); }
Item createMagoBerry() { return createHalfHpHealBerry(ItemType::MagoBerry, "Mago Berry", 1, 8); }
Item createAguavBerry() { return createHalfHpHealBerry(ItemType::AguavBerry, "Aguav Berry", 1, 8); }
Item createIapapaBerry() { return createHalfHpHealBerry(ItemType::IapapaBerry, "Iapapa Berry", 1, 8); }

Item createOccaBerry() { return createResistBerry(ItemType::OccaBerry, "Occa Berry", Type::Fire); }
Item createPasshoBerry() { return createResistBerry(ItemType::PasshoBerry, "Passho Berry", Type::Water); }
Item createWacanBerry() { return createResistBerry(ItemType::WacanBerry, "Wacan Berry", Type::Electric); }
Item createRindoBerry() { return createResistBerry(ItemType::RindoBerry, "Rindo Berry", Type::Grass); }
Item createYacheBerry() { return createResistBerry(ItemType::YacheBerry, "Yache Berry", Type::Ice); }
Item createChopleBerry() { return createResistBerry(ItemType::ChopleBerry, "Chople Berry", Type::Fighting); }
Item createKebiaBerry() { return createResistBerry(ItemType::KebiaBerry, "Kebia Berry", Type::Poison); }
Item createShucaBerry() { return createResistBerry(ItemType::ShucaBerry, "Shuca Berry", Type::Ground); }
Item createCobaBerry() { return createResistBerry(ItemType::CobaBerry, "Coba Berry", Type::Flying); }
Item createPayapaBerry() { return createResistBerry(ItemType::PayapaBerry, "Payapa Berry", Type::Psychic); }
Item createTangaBerry() { return createResistBerry(ItemType::TangaBerry, "Tanga Berry", Type::Bug); }
Item createChartiBerry() { return createResistBerry(ItemType::ChartiBerry, "Charti Berry", Type::Rock); }
Item createKasibBerry() { return createResistBerry(ItemType::KasibBerry, "Kasib Berry", Type::Ghost); }
Item createHabanBerry() { return createResistBerry(ItemType::HabanBerry, "Haban Berry", Type::Dragon); }
Item createColburBerry() { return createResistBerry(ItemType::ColburBerry, "Colbur Berry", Type::Dark); }
Item createBabiriBerry() { return createResistBerry(ItemType::BabiriBerry, "Babiri Berry", Type::Steel); }
Item createChilanBerry() { return createResistBerry(ItemType::ChilanBerry, "Chilan Berry", Type::Normal); }

Item createLiechiBerry() { return createPinchStatBerry(ItemType::LiechiBerry, "Liechi Berry", StatIndex::Attack); }
Item createGanlonBerry() { return createPinchStatBerry(ItemType::GanlonBerry, "Ganlon Berry", StatIndex::Defense); }
Item createSalacBerry() { return createPinchStatBerry(ItemType::SalacBerry, "Salac Berry", StatIndex::Speed); }
Item createPetayaBerry() { return createPinchStatBerry(ItemType::PetayaBerry, "Petaya Berry", StatIndex::SpecialAttack); }
Item createApicotBerry() { return createPinchStatBerry(ItemType::ApicotBerry, "Apicot Berry", StatIndex::SpecialDefense); }

Item createJabocaBerry() { return createRetaliationBerry(ItemType::JabocaBerry, "Jaboca Berry", Category::Physical); }
Item createRowapBerry() { return createRetaliationBerry(ItemType::RowapBerry, "Rowap Berry", Category::Special); }

bool isBerry(ItemType type) {
    switch (type) {
        case ItemType::OranBerry:
        case ItemType::SitrusBerry:
        case ItemType::LumBerry:
        case ItemType::ChestoBerry:
        case ItemType::PechaBerry:
        case ItemType::RawstBerry:
        case ItemType::AspearBerry:
        case ItemType::PersimBerry:
        case ItemType::CheriBerry:
        case ItemType::FigyBerry:
        case ItemType::WikiBerry:
        case ItemType::MagoBerry:
        case ItemType::AguavBerry:
        case ItemType::IapapaBerry:
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
        case ItemType::ChilanBerry:
        case ItemType::LiechiBerry:
        case ItemType::GanlonBerry:
        case ItemType::SalacBerry:
        case ItemType::PetayaBerry:
        case ItemType::ApicotBerry:
        case ItemType::JabocaBerry:
        case ItemType::RowapBerry:
            return true;
        default:
            return false;
    }
}

void Item::executeTrigger(ItemTrigger trigger, Pokemon* self, Pokemon* opponent, Battle& battle, void* context) {
    if (isUsed && isConsumable) return;  // 已使用的一次性物品不再触发
    
    auto it = effects.find(trigger);
    if (it != effects.end() && it->second) {
        it->second(self, opponent, battle, context);
    }
}
