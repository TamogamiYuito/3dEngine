#pragma once
#include <array>
#include <unordered_set>
#include <cmath>
#include "Math.hpp"
#include "IHoverable.hpp"

constexpr std::array<V3, 8> LOCAL{ {
    {-HALF,-HALF,-HALF},{ HALF,-HALF,-HALF},
    { HALF, HALF,-HALF},{-HALF, HALF,-HALF},
    {-HALF,-HALF, HALF},{ HALF,-HALF, HALF},
    { HALF, HALF, HALF},{-HALF, HALF, HALF}
} };
constexpr std::array<std::pair<int, int>, 12> EDGE{ {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
} };
constexpr std::array<std::array<int, 4>, 6> FACE{ {
	{ 0, 3, 2, 1 }, // -Z（手前）  外側から見て CCW → 法線 -Z
	{ 4, 5, 6, 7 }, // +Z（奥）    外側から見て CCW → 法線 +Z
	{ 0, 4, 7, 3 }, // -X（左）    外側から見て CCW → 法線 -X
	{ 1, 2, 6, 5 }, // +X（右）    外側から見て CCW → 法線 +X
	{ 3, 7, 6, 2 }, // +Y（上）    外側から見て CCW → 法線 +Y
	{ 0, 1, 5, 4 }  // -Y（下）    外側から見て CCW → 法線 -Y
} };

struct GKey {
    int gx, gz;
    bool operator==(const GKey&) const = default;
};
struct GHash {
    size_t operator()(GKey k) const noexcept { return (size_t)k.gx << 32 ^ (size_t)k.gz; }
};
int    gIdx(double v);
double gPos(int g);

struct Cube : public IHoverable {
    V3 c;
    Quat q{1,0,0,0};
    V3 s{ 1,1,1 };

    Cube() = default;
    Cube(V3 pos, Quat rot = {1,0,0,0}, V3 scale = {1,1,1})
        : c(pos), q(rot), s(scale) {}

    bool checkHovered(const s3d::Vec2& cur, double cx, double cy,
                      V3 cam, V3 Rv, V3 U, V3 F, double& depth) const override;
};
