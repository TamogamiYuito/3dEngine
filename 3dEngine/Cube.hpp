#pragma once
#include <array>
#include <unordered_set>
#include <cmath>
#include "Math.hpp"

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

struct GKey {
    int gx, gz;
    bool operator==(const GKey&) const = default;
};
struct GHash {
    size_t operator()(GKey k) const noexcept { return (size_t)k.gx << 32 ^ (size_t)k.gz; }
};
int    gIdx(double v);
double gPos(int g);

struct Cube {
    V3 c;
    Quat q{1,0,0,0};
    V3 s{ 1,1,1 };
};
