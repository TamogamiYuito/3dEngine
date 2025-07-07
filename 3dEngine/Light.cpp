#include "Light.hpp"
#include "RenderUtils.hpp"
#include <cmath>

bool Light::checkHovered(const s3d::Vec2& cur, double cx, double cy,
                         V3 cam, V3 Rv, V3 U, V3 F, double& depth) const {
    auto scr = [&](V3 w) { return screenProject(w, cam, Rv, U, F, cx, cy); };
    double minX = 1e9, minY = 1e9;
    double maxX = -1e9, maxY = -1e9;
    bool any = false;

    for (int k = 0; k < 8; ++k) {
        P2 p = scr(c + qRotate(q, LOCAL[k] * s));
        if (std::isinf(p.x)) continue;
        any = true;
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    if (!any) return false;
    if (cur.x >= minX && cur.x <= maxX && cur.y >= minY && cur.y <= maxY) {
        depth = len(c - cam);
        return true;
    }
    return false;
}
