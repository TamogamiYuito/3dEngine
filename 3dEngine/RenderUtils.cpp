/**
 * @file RenderUtils.cpp
 * @brief ワールド座標のスクリーン投影と、カーソル位置から回転角度を求める補助処理を実装します。
 */
#include "RenderUtils.hpp"
#include <optional>
#include <cmath>

/// ワールド座標をカメラ基準へ変換した後、透視投影します。
P2 screenProject(V3 world, V3 cam, V3 Rv, V3 U, V3 F, double cx, double cy) {
    V3 r = world - cam;
    return project({ dot(r,Rv), dot(r,U), -dot(r,F) }, cx, cy);
}

/// スクリーン上のカーソル位置から、ワールド空間の視線方向を生成します。
V3 rayFromCursor(s3d::Vec2 p, V3 Rv, V3 U, V3 F, const s3d::Vec2& winF) {
    double sx = p.x - winF.x, sy = -(p.y - winF.y);
    return norm(sx * Rv + sy * U + FOCAL * F);
}

/// ピボットを中心としたカーソル角度を求め、回転方向が反転しないよう軸向きを考慮します。
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
