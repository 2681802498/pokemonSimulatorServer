#pragma once

enum class Nature {
    Hardy,    // Neutral
    Lonely,   // +Atk -Def
    Brave,    // +Atk -Spd
    Adamant,  // +Atk -SpAtk
    Naughty,  // +Atk -SpDef
    Bold,     // +Def -Atk
    Docile,   // Neutral
    Relaxed,  // +Def -Spd
    Impish,   // +Def -SpAtk
    Lax,      // +Def -SpDef
    Timid,    // +Spd -Atk
    Hasty,    // +Spd -Def
    Serious,  // Neutral
    Jolly,    // +Spd -SpAtk
    Naive,    // +Spd -SpDef
    Modest,   // +SpAtk -Atk
    Mild,     // +SpAtk -Def
    Quiet,    // +SpAtk -Spd
    Bashful,  // Neutral
    Rash,     // +SpAtk -SpDef
    Calm,     // +SpDef -Atk
    Gentle,   // +SpDef -Def
    Sassy,    // +SpDef -Spd
    Careful,  // +SpDef -SpAtk
    Quirky,   // Neutral
    Count
};

// Get nature modifier for a stat (0.9 or 1.1)
float getNatureModifier(Nature nature, int statIndex); // 0=Atk, 1=Def, 2=SpAtk, 3=SpDef, 4=Spd
