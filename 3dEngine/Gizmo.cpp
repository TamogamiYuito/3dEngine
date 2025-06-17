#include "Gizmo.hpp"

double segDist2(s3d::Vec2 p, s3d::Vec2 a, s3d::Vec2 b) {
    s3d::Vec2 ab = b - a;
    double l2 = ab.lengthSq();
    if (l2 < 1e-6) return (p - a).lengthSq();
    double t = s3d::Clamp(s3d::Dot(p - a, ab) / l2, 0.0, 1.0);
    return (p - (a + ab * t)).lengthSq();
}
