#include "RenderUtils.hpp"
#include <optional>
#include <cmath>
#include <array>
#include <vector>

P2 screenProject(V3 world, V3 cam, V3 Rv, V3 U, V3 F, double cx, double cy) {
    V3 r = world - cam;
    return project({ dot(r,Rv), dot(r,U), -dot(r,F) }, cx, cy);
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

namespace {
    // Clip polygon against the near plane in view space using Sutherland–Hodgman
    std::vector<V3> clipNearZ(const std::vector<V3>& poly) {
        std::vector<V3> out;
        if (poly.empty()) return out;
        for (size_t i = 0; i < poly.size(); ++i) {
            const V3& a = poly[i];
            const V3& b = poly[(i + 1) % poly.size()];
            bool aIn = (a.z >= NEAR_Z);
            bool bIn = (b.z >= NEAR_Z);
            if (aIn && bIn) {
                out.push_back(b);
            } else if (aIn && !bIn) {
                double t = (NEAR_Z - a.z) / (b.z - a.z);
                out.push_back({ a.x + t * (b.x - a.x),
                                a.y + t * (b.y - a.y),
                                NEAR_Z });
            } else if (!aIn && bIn) {
                double t = (NEAR_Z - a.z) / (b.z - a.z);
                out.push_back({ a.x + t * (b.x - a.x),
                                a.y + t * (b.y - a.y),
                                NEAR_Z });
                out.push_back(b);
            }
        }
        return out;
    }
}

std::vector<P2> clipProjectQuad(const std::array<V3, 4>& vw,
                                V3 cam, V3 Rv, V3 U, V3 F,
                                double cx, double cy,
                                double& depth) {
    std::vector<V3> view(4);
    for (size_t i = 0; i < 4; ++i) {
        V3 r = vw[i] - cam;
        view[i] = { dot(r, Rv), dot(r, U), dot(r, F) };
    }

    std::vector<V3> clipped = clipNearZ(view);
    if (clipped.size() < 3) {
        depth = 0.0;
        return {};
    }

    depth = clipped[0].z;
    for (const auto& v : clipped) depth = std::min(depth, v.z);

    std::vector<P2> projected(clipped.size());
    for (size_t i = 0; i < clipped.size(); ++i) {
        // Convert back to the coordinate system expected by project()
        projected[i] = project({ clipped[i].x, clipped[i].y, -clipped[i].z },
                               cx, cy);
    }
    return projected;
}
