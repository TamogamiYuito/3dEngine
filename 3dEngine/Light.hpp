/**
 * @file Light.hpp
 * @brief 方向ライトの位置、回転、表示用スケール、光量と選択判定を管理します。
 */
#pragma once
#include "Math.hpp"
#include "IHoverable.hpp"
#include "Cube.hpp"

/// 方向と光量を持つ編集可能なライトです。
struct Light : public IHoverable {
    // Center position of the light gizmo
    V3   c{ 0,0,0 };
    // Orientation of the light gizmo
    Quat q{ 1,0,0,0 };
    // Scale of the gizmo (for visualization only)
    V3   s{ 0.5,0.5,0.5 };
    // Light intensity (brightness)
    double intensity = 1.0;

	double range = 200.0;

    // Get the light direction from the orientation
    V3 dir() const { return qRotate(q, { 0,-1,0 }); }

    bool checkHovered(const s3d::Vec2& cur, double cx, double cy,
                      V3 cam, V3 Rv, V3 U, V3 F, double& depth) const override;
};
