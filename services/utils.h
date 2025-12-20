#pragma once
#include <random>

// inline avoids multiple definitions
inline int generateRandomID(int min = 100000, int max = 999999) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}
