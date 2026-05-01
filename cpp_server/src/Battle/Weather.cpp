#include "Battle/Weather.h"

#include "Battle/Pokemon.h"

#include <algorithm>

Type weatherBallType(const Weather& weather) {
    if (!weather.isActive()) {
        return Type::Normal;
    }

    switch (weather.type) {
        case WeatherType::Rain: return Type::Water;
        case WeatherType::Sun: return Type::Fire;
        case WeatherType::Sandstorm: return Type::Rock;
        case WeatherType::Hail: return Type::Ice;
        case WeatherType::Snow: return Type::Ice;
        default: return Type::Normal;
    }
}

int weatherRecoveryAmount(const Weather& weather, const Pokemon* attacker) {
    if (!attacker) {
        return 0;
    }

    if (weather.type == WeatherType::Sun) {
        return std::max(1, (attacker->getMaxHP() * 2) / 3);
    }
    if (weather.type == WeatherType::Rain || weather.type == WeatherType::Sandstorm || weather.type == WeatherType::Hail) {
        return std::max(1, attacker->getMaxHP() / 4);
    }
    return std::max(1, attacker->getMaxHP() / 2);
}

void Weather::setWeather(WeatherType newType, int turns) {
    type = newType;
    duration = turns > 0 ? turns : 0;
}

void Weather::tick() {
    if (duration > 0) {
        --duration;
        if (duration == 0) {
            type = WeatherType::Clear;
        }
    }
}

bool Weather::isActive() const {
    return type != WeatherType::Clear && duration > 0;
}

std::string Weather::getName() const {
    switch (type) {
        case WeatherType::Rain: return "Rain";
        case WeatherType::Sun: return "Sun";
        case WeatherType::Sandstorm: return "Sandstorm";
        case WeatherType::Hail: return "Hail";
        case WeatherType::Snow: return "Snow";
        default: return "Clear";
    }
}

float Weather::applyDamageModifier(Type targetType) const {
    switch (type) {
        case WeatherType::Rain:
            if (targetType == Type::Fire) return 0.5f;
            if (targetType == Type::Water) return 1.5f;
            break;
        case WeatherType::Sun:
            if (targetType == Type::Water) return 0.5f;
            if (targetType == Type::Fire) return 1.5f;
            break;
        case WeatherType::Sandstorm:
            // 沙尘暴对岩石、地面、钢属性技能无影响
            break;
        case WeatherType::Hail:
            // 冰雹对冰属性技能无影响
            break;
        case WeatherType::Snow:
            // 雪对冰属性技能有增强效果
            if (targetType == Type::Ice) return 1.5f;
            break;
        default:
            break;
    }
    return 1.0f;
}
