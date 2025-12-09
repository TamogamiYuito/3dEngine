#include "RenderUtils.hpp"
#include <optional>
#include <cmath>

P2 screenProject(V3 world, V3 cam, V3 Rv, V3 U, V3 F, double cx, double cy) {
    V3 r = world - cam;
    return project({ dot(r,Rv), dot(r,U), dot(r,F) }, cx, cy);
}

V3 rayFromCursor(s3d::Vec2 p, V3 Rv, V3 U, V3 F, const s3d::Vec2& winF) {
    double sx = p.x - winF.x, sy = -(p.y - winF.y);
    return norm(sx * Rv + sy * U + FOCAL * F);
}

std::optional<double> angleFromCursor(s3d::Vec2 p, V3 axis, V3 pivot,
                                      V3 cam, V3 Rv, V3 U, V3 F,
                                      double cx, double cy) {
    V3 ax = norm(axis);
    double axF = dot(ax, F);
    P2 sp = screenProject(pivot, cam, Rv, U, F, cx, cy);
    if (std::isinf(sp.x)) return std::nullopt;
    s3d::Vec2 diff = p - s3d::Vec2{ sp.x, sp.y };
    double ang = std::atan2(diff.y, diff.x);
    if (axF < 0) ang = -ang;
    return ang;
}
