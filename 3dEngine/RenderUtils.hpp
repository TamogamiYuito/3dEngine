/**
 * @file RenderUtils.hpp
 * @brief 描画やギズモ操作で共有する投影・カーソル計算関数を宣言します。
 */
#pragma once
#include "Math.hpp"
#include <Siv3D.hpp>
#include <optional>

P2 screenProject(V3 world, V3 cam, V3 Rv, V3 U, V3 F, double cx, double cy);
V3 rayFromCursor(s3d::Vec2 p, V3 Rv, V3 U, V3 F, const s3d::Vec2& winF);
std::optional<double> angleFromCursor(s3d::Vec2 p, V3 axis, V3 pivot,
                                      V3 cam, V3 Rv, V3 U, V3 F,
                                      double cx, double cy);
