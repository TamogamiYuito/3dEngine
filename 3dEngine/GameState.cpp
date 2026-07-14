/**
 * @file GameState.cpp
 * @brief 1フレーム内で共有するカメラ情報と、投影・カーソル角度計算の窓口を構築します。
 */
#include "GameState.hpp"
#include "RenderUtils.hpp"
#include <Siv3D.hpp>

/// 保存済みのカメラ基底を使用し、ワールド座標をスクリーンへ投影します。
P2 FrameContext::project(V3 w) const {
    return screenProject(w, cam, right, up, forward, windowHalf.x, windowHalf.y);
}

/// 回転ギズモ用に、指定軸まわりのカーソル角度を求めます。
std::optional<double> FrameContext::cursorAngle(s3d::Vec2 p, V3 axis, V3 pivot) const {
    return angleFromCursor(p, axis, pivot, cam, right, up, forward, windowHalf.x, windowHalf.y);
}

/// 1フレーム中に繰り返し使うカメラ・カーソル情報をまとめます。
FrameContext buildFrameContext(const GameState& state, const s3d::Vec2& windowHalf) {
    FrameContext ctx;
    ctx.cursor = s3d::Cursor::PosF();
    ctx.windowHalf = windowHalf;
    ctx.cam = state.camera.cam;
    ctx.right = state.camera.right();
    ctx.up = state.camera.up();
    ctx.forward = state.camera.forward();
    return ctx;
}
