#pragma once
#include "Math.hpp"
#include <Siv3D.hpp>

class IHoverable {
public:
    virtual ~IHoverable() = default;
    virtual bool checkHovered(const s3d::Vec2& cur, double cx, double cy,
                              V3 cam, V3 Rv, V3 U, V3 F, double& depth) const = 0;
};
