
#pragma once
#include<time.h>
#include <cstdint>

class PRNG {
private:
    static uint32_t state;

public:
    PRNG(uint32_t seed = 0) {
        if (seed == 0) {
            setSeed(static_cast<uint32_t>(time(nullptr)));
        } else {
            setSeed(seed);
        }
    }

    // Generate next random uint32_t
    static uint32_t next() {
        // Linear Congruential Generator parameters
        state = (state * 1664525U + 1013904223U) & 0xFFFFFFFFU;
        return state;
    }

    // Generate random int in range [min, max)
    static int nextInt(int min, int max) {
        if (min >= max) return min;
        uint32_t rand = next();
        return min + (rand % (max - min));
    }

    // Generate random float in range [0, 1)
    static float nextFloat() {
        uint32_t rand = next();
        return static_cast<float>(rand) / static_cast<float>(0xFFFFFFFFU);
    }

    // Generate random float in range [min, max)
    static float nextFloat(float min, float max) {
        return min + nextFloat() * (max - min);
    }

    // Generate random bool with given probability (0.0 to 1.0)
    static bool nextBool(float probability = 0.5f) {
        return nextFloat() < probability;
    }

    // Set new seed
    static void setSeed(uint32_t seed) {
        state = seed;
    }
};