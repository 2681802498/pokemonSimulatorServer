#include "Battle/Field.h"

void Field::setField(FieldType newType, int turns) {
    type = newType;
    duration = turns > 0 ? turns : 0;
}

void Field::tick() {
    if (duration > 0) {
        --duration;
        if (duration == 0) {
            type = FieldType::None;
        }
    }
}

bool Field::isActive() const {
    return type != FieldType::None && duration > 0;
}

bool Field::isTrickRoom() const {
    return type == FieldType::TrickRoom && isActive();
}

std::string Field::getName() const {
    switch (type) {
        case FieldType::Psychic: return "Psychic Terrain";
        case FieldType::Electric: return "Electric Terrain";
        case FieldType::Grassy: return "Grassy Terrain";
        case FieldType::Misty: return "Misty Terrain";
        case FieldType::TrickRoom: return "Trick Room";
        default: return "Neutral Field";
    }
}
