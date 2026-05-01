#pragma once

#include <string>

enum class FieldType {
    None,
    Psychic,
    Electric,
    Grassy,
    Misty,
    TrickRoom,
    Count
};

struct Field {
    FieldType type = FieldType::None;
    int duration = 0;

    void setField(FieldType newType, int turns);
    void tick();
    bool isActive() const;
    bool isTrickRoom() const;
    std::string getName() const;
};
