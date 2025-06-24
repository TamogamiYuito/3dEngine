#pragma once
#include "Math.hpp"

struct Light {
    // Direction from the light to the scene
    V3     dir{ 0, -1, 0 };
    // Light intensity (brightness)
    double intensity = 1.0;
};
