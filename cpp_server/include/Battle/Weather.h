#pragma once

#include "Types.h"
#include <string>

class Pokemon;

enum class WeatherType {
    Clear,
    Rain,
    Sun,
    Sandstorm,
    Hail,
    Snow,
    Count
};

struct Weather {
    WeatherType type = WeatherType::Clear;
    int duration = 0;

    void setWeather(WeatherType newType, int turns);
    void tick();
    bool isActive() const;
    std::string getName() const;
    float applyDamageModifier(Type targetType) const;
};

Type weatherBallType(const Weather& weather);
int weatherRecoveryAmount(const Weather& weather, const Pokemon* attacker);
