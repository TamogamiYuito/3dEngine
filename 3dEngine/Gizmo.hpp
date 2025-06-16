#pragma once
#include "Math.hpp"

enum class Mode {
    Move,
    Rotate,
    Scale
};

enum class Handle {
    None,
    MoveX,
    MoveY,
    MoveZ,
    RotateX,
    RotateY,
    RotateZ,
    ScaleX,
    ScaleY,
    ScaleZ,
    ScaleUniform
};

struct Drag {
    bool on = false;
    s3d::Vec2 cur0;
    V3 p0;
    Quat q0{1,0,0,0};
    double s0{};
    double lenPx = 1;
    V3 axis{};
    double ang0{};
};

inline double segDist2(s3d::Vec2 p, s3d::Vec2 a, s3d::Vec2 b) {
    s3d::Vec2 ab = b - a;
    double l2 = ab.lengthSq();
    if (l2 < 1e-6) return (p - a).lengthSq();
    double t = s3d::Clamp(s3d::Dot(p - a, ab) / l2, 0.0, 1.0);
    return (p - (a + ab * t)).lengthSq();
}
