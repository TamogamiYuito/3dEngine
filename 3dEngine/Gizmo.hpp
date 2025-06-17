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
    V3 s0{ 1,1,1 };
    double lenPx = 1;
    V3 axis{};
    double ang0{};
};

double segDist2(s3d::Vec2 p, s3d::Vec2 a, s3d::Vec2 b);
