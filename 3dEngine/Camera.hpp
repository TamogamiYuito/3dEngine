#pragma once
#include "Math.hpp"
#include <Siv3D.hpp>

struct Camera {
    V3 pos{ 0, HALF + EYE, -200 };
    V3 cam{ pos };
    double yaw = 0;
    double pitch = 0;
    double vy = 0;
    bool free = true;

    void update(double dt, const s3d::Vec2& winF, const s3d::Point& winP);
    V3 forward() const;
    V3 right() const;
    V3 up() const;
    V3 forwardH() const;
};



