#pragma once
#include "Math.hpp"

struct Light {
    // Center position of the light gizmo
    V3   c{ 0,0,0 };
    // Orientation of the light gizmo
    Quat q{ 1,0,0,0 };
    // Scale of the gizmo (for visualization only)
    V3   s{ 0.5,0.5,0.5 };
    // Light intensity (brightness)
    double intensity = 1.0;

	double range = 200.0;

    // Get the light direction from the orientation
    V3 dir() const { return qRotate(q, { 0,-1,0 }); }
};
