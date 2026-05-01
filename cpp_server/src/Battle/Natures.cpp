#include "Natures.h"

float getNatureModifier(Nature nature, int statIndex) {
    // statIndex: 0=Atk, 1=Def, 2=SpAtk, 3=SpDef, 4=Spd
    static const float modifiers[static_cast<int>(Nature::Count)][5] = {
        // Hardy
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        // Lonely: +Atk -Def
        {1.1f, 0.9f, 1.0f, 1.0f, 1.0f},
        // Brave: +Atk -Spd
        {1.1f, 1.0f, 1.0f, 1.0f, 0.9f},
        // Adamant: +Atk -SpAtk
        {1.1f, 1.0f, 0.9f, 1.0f, 1.0f},
        // Naughty: +Atk -SpDef
        {1.1f, 1.0f, 1.0f, 0.9f, 1.0f},
        // Bold: +Def -Atk
        {0.9f, 1.1f, 1.0f, 1.0f, 1.0f},
        // Docile
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        // Relaxed: +Def -Spd
        {1.0f, 1.1f, 1.0f, 1.0f, 0.9f},
        // Impish: +Def -SpAtk
        {1.0f, 1.1f, 0.9f, 1.0f, 1.0f},
        // Lax: +Def -SpDef
        {1.0f, 1.1f, 1.0f, 0.9f, 1.0f},
        // Timid: +Spd -Atk
        {0.9f, 1.0f, 1.0f, 1.0f, 1.1f},
        // Hasty: +Spd -Def
        {1.0f, 0.9f, 1.0f, 1.0f, 1.1f},
        // Serious
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        // Jolly: +Spd -SpAtk
        {1.0f, 1.0f, 0.9f, 1.0f, 1.1f},
        // Naive: +Spd -SpDef
        {1.0f, 1.0f, 1.0f, 0.9f, 1.1f},
        // Modest: +SpAtk -Atk
        {0.9f, 1.0f, 1.1f, 1.0f, 1.0f},
        // Mild: +SpAtk -Def
        {1.0f, 0.9f, 1.1f, 1.0f, 1.0f},
        // Quiet: +SpAtk -Spd
        {1.0f, 1.0f, 1.1f, 1.0f, 0.9f},
        // Bashful
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        // Rash: +SpAtk -SpDef
        {1.0f, 1.0f, 1.1f, 0.9f, 1.0f},
        // Calm: +SpDef -Atk
        {0.9f, 1.0f, 1.0f, 1.1f, 1.0f},
        // Gentle: +SpDef -Def
        {1.0f, 0.9f, 1.0f, 1.1f, 1.0f},
        // Sassy: +SpDef -Spd
        {1.0f, 1.0f, 1.0f, 1.1f, 0.9f},
        // Careful: +SpDef -SpAtk
        {1.0f, 1.0f, 0.9f, 1.1f, 1.0f},
        // Quirky
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
    };

    int n = static_cast<int>(nature);
    if (n < 0 || n >= static_cast<int>(Nature::Count) || statIndex < 0 || statIndex >= 5) {
        return 1.0f;
    }
    return modifiers[n][statIndex];
}