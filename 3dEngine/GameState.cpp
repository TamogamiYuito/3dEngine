#include "GameState.hpp"
#include "RenderUtils.hpp"
#include <Siv3D.hpp>

P2 FrameContext::project(V3 w) const {
    return screenProject(w, cam, right, up, forward, windowHalf.x, windowHalf.y);
}

std::optional<double> FrameContext::cursorAngle(s3d::Vec2 p, V3 axis, V3 pivot) const {
    return angleFromCursor(p, axis, pivot, cam, right, up, forward, windowHalf.x, windowHalf.y);
}

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
