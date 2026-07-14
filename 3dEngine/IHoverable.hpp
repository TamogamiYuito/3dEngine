/**
 * @file IHoverable.hpp
 * @brief 立方体とライトで共通利用するホバー判定インターフェースを定義します。
 */
#pragma once
#include "Math.hpp"
#include <Siv3D.hpp>

/// スクリーン上で選択可能なオブジェクトの共通インターフェースです。
class IHoverable {
public:
    virtual ~IHoverable() = default;
    virtual bool checkHovered(const s3d::Vec2& cur, double cx, double cy,
                              V3 cam, V3 Rv, V3 U, V3 F, double& depth) const = 0;
};
