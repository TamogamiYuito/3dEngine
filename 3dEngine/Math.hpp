#pragma once
#include <Siv3D.hpp>
#include <cmath>
#include <limits>

struct V3 {
    double x, y, z;
};

inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*(double s, V3 v) { return { s * v.x, s * v.y, s * v.z }; }
inline V3 operator*(V3 v, double s) { return s * v; }
inline V3 operator*(V3 a, V3 b) { return { a.x * b.x, a.y * b.y, a.z * b.z }; }
inline V3 operator/(V3 v, double s) { return { v.x / s, v.y / s, v.z / s }; }
inline double  dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3      cross(V3 a, V3 b) {
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}
inline double  len(V3 v) { return std::sqrt(dot(v, v)); }
inline V3      norm(V3 v) { double l = len(v); return (l > 1e-9) ? v / l : V3{ 0,0,0 }; }

struct P2 {
    double x, y;
};
constexpr double CAM_DIST = 300;
constexpr double FOCAL = 400;
constexpr double NEAR_Z = 1;
inline P2 project(V3 v, double cx, double cy) {
    double z = v.z + CAM_DIST;
    if (z < NEAR_Z) {
        const double inf = std::numeric_limits<double>::infinity();
        return { inf, inf };
    }
    return { (FOCAL * v.x) / z + cx,  -(FOCAL * v.y) / z + cy };
}

constexpr double HALF = 75;
constexpr double EYE = 10;
constexpr double R = 20;
constexpr double CAP_H = 40;
constexpr double MOVE = 120;
constexpr double GRAV = 400;
constexpr double JUMP = 200;
constexpr double RC_SENS = 0.003;
constexpr double MS_SENS = 0.003;
constexpr double PITCH_LIM = s3d::Math::ToRadians(85);
constexpr double PAN_SPD = 200;
constexpr double DOLLY_SPD = 400;

struct Quat {
    double w, x, y, z;
};
inline Quat qMul(Quat a, Quat b) {
    return { a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
             a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
             a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
             a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w };
}
inline Quat qConj(Quat q) { return { q.w, -q.x, -q.y, -q.z }; }
inline Quat qNormalize(Quat q) {
    double l = std::sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    return { q.w / l, q.x / l, q.y / l, q.z / l };
}
inline Quat qAxisAngle(V3 axis, double angle) {
    axis = norm(axis); double s = std::sin(angle * 0.5);
    return { std::cos(angle * 0.5), axis.x*s, axis.y*s, axis.z*s };
}
inline V3 qRotate(Quat q, V3 v) {
    Quat p{ 0,v.x,v.y,v.z };
    Quat r = qMul(qMul(q, p), qConj(q));
    return { r.x, r.y, r.z };
}
