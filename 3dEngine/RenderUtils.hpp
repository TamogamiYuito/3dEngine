#pragma once
#include "Math.hpp"
#include <Siv3D.hpp>
#include <optional>

P2 screenProject(V3 world, V3 cam, V3 Rv, V3 U, V3 F, double cx, double cy);
V3 rayFromCursor(s3d::Vec2 p, V3 Rv, V3 U, V3 F, const s3d::Vec2& winF);
std::optional<double> angleFromCursor(s3d::Vec2 p, V3 axis, V3 pivot,
                                      V3 cam, V3 Rv, V3 U, V3 F,
                                      double cx, double cy);

// Clip a world-space quad against the near plane and project the remaining
// vertices to screen space. The returned polygon may have between 3 and 5
// vertices. The minimum view-space z of the clipped vertices is written to
// `depth` so the Painter's algorithm can continue to be used.
std::vector<P2> clipProjectQuad(const std::array<V3, 4>& vw,
                                V3 cam, V3 Rv, V3 U, V3 F,
                                double cx, double cy,
                                double& depth);


