#pragma once
#include "Math.hpp"

struct Light {
    V3 direction{ -1, -1, -1 }; // Direction from light to scene
    double intensity = 1.0;
};

