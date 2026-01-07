#include "Game.hpp"
#include "RenderUtils.hpp"
#include "Light.hpp"
#include "Camera.hpp"
#include "Cube.hpp"
#include "Gizmo.hpp"
#include <vector>
#include <unordered_set>
#include <optional>
#include <Siv3D.hpp>
#include <algorithm>

using namespace s3d;

namespace {
    constexpr double kNearPlane = NEAR_Z - CAM_DIST;

    bool isInsideNearPlane(const V3& v) {
        return v.z >= kNearPlane;
    }

    V3 intersectNearPlane(const V3& a, const V3& b) {
        const double t = (kNearPlane - a.z) / (b.z - a.z);
        return a + t * (b - a);
    }

    std::vector<V3> clipPolygonToNearPlane(const std::vector<V3>& input) {
        if (input.empty()) {
            return {};
        }

        std::vector<V3> output;
        output.reserve(input.size() + 1);

        V3 prev = input.back();
        bool prevInside = isInsideNearPlane(prev);
        for (const auto& curr : input) {
            bool currInside = isInsideNearPlane(curr);
            if (prevInside && currInside) {
                output.push_back(curr);
            } else if (prevInside && !currInside) {
                output.push_back(intersectNearPlane(prev, curr));
            } else if (!prevInside && currInside) {
                output.push_back(intersectNearPlane(prev, curr));
                output.push_back(curr);
            }
            prev = curr;
            prevInside = currInside;
        }

        return output;
    }

    std::vector<std::array<V3, 3>> clipTriangleToNearPlane(const std::array<V3, 3>& tri) {
        std::vector<V3> poly{ tri[0], tri[1], tri[2] };
        std::vector<V3> clipped = clipPolygonToNearPlane(poly);
        if (clipped.size() < 3) {
            return {};
        }
        if (clipped.size() == 3) {
            return { { clipped[0], clipped[1], clipped[2] } };
        }
        if (clipped.size() == 4) {
            return { { clipped[0], clipped[1], clipped[2] },
                     { clipped[0], clipped[2], clipped[3] } };
        }
        std::vector<std::array<V3, 3>> tris;
        tris.reserve(clipped.size() - 2);
        for (size_t i = 1; i + 1 < clipped.size(); ++i) {
            tris.push_back({ clipped[0], clipped[i], clipped[i + 1] });
        }
        return tris;
    }
}

P2 Game::FrameContext::project(V3 w) const {
    return screenProject(w, cam, right, up, forward, windowHalf.x, windowHalf.y);
}

std::optional<double> Game::FrameContext::cursorAngle(Vec2 p, V3 axis, V3 pivot) const {
    return angleFromCursor(p, axis, pivot, cam, right, up, forward, windowHalf.x, windowHalf.y);
}

Game::FrameContext Game::buildFrameContext(const Vec2& windowHalf) const {
    FrameContext ctx;
    ctx.cursor = Cursor::PosF();
    ctx.windowHalf = windowHalf;
    ctx.cam = camera.cam;
    ctx.right = camera.right();
    ctx.up = camera.up();
    ctx.forward = camera.forward();
    return ctx;
}

void Game::updateModeState(bool free) {
    if (free && !drag.on) {
        if (KeyW.down()) mode = Mode::Move;
        if (KeyE.down()) mode = Mode::Rotate;
        if (KeyR.down()) mode = Mode::Scale;
    }
}

void Game::handleCreationUI(const FrameContext& ctx, double dt, bool free) {
    if (!free) {
        return;
    }

    tryCreateCube(ctx);
    tryCreateLight(ctx);
    cycleLightSelection();
    adjustSelectedLightIntensity(dt);
}

void Game::tryCreateCube(const FrameContext& ctx) {
    if (!SimpleGUI::Button(U"+ Cube", { 20,20 })) {
        return;
    }

    V3 Fh = camera.forwardH();
    V3 Rv = camera.right();
    double ang = Random(-Math::Pi / 4.0, Math::Pi / 4.0);
    V3 dir = norm(std::cos(ang) * (-1 * Fh) + std::sin(ang) * Rv);
    double dist = Random(2.0, 4.0) * (2.0 * HALF);
    double yRand = Random(0, 2) * (2.0 * HALF);
    V3 hit = V3{ ctx.cam.x, yRand, ctx.cam.z } + dist * dir;
    GKey g{ gIdx(hit.x), gIdx(hit.z) };
    GKey place = findNearestEmpty(g);
    if (grid.find(place) == grid.end()) {
        cubes.push_back({ { gPos(place.gx), yRand, gPos(place.gz) } });
        grid.insert(place);
    }
}

void Game::tryCreateLight(const FrameContext& ctx) {
    if (!SimpleGUI::Button(U"+ Light", { 20,50 })) {
        return;
    }

    Light lt;
    lt.c = ctx.cam + ctx.forward * 100.0;
    V3 from{ 0,-1,0 };
    V3 to = norm(ctx.forward);
    double cosA = dot(from, to);
    if (cosA > 1.0) cosA = 1.0; if (cosA < -1.0) cosA = -1.0;
    V3 axis = cross(from, to);
    if (len(axis) < 1e-6) axis = { 1,0,0 };
    double angle = std::acos(cosA);
    lt.q = qAxisAngle(axis, angle);
    lights.push_back(lt);
    lightSel = static_cast<int>(lights.size()) - 1;
}

void Game::cycleLightSelection() {
    if (KeyTab.down() && !lights.empty()) {
        lightSel = (lightSel + 1) % static_cast<int>(lights.size());
    }
}

void Game::adjustSelectedLightIntensity(double dt) {
    if (lightSel == -1) {
        return;
    }

    if (KeyZ.pressed())
        lights[lightSel].intensity = std::max(0.0, lights[lightSel].intensity - dt);
    if (KeyX.pressed())
        lights[lightSel].intensity += dt;
    SimpleGUI::Slider(U"Intensity", lights[lightSel].intensity, 0.0, 5.0, Vec2{ 20,80 });
}

void Game::updateHoverState(const FrameContext& ctx, bool free) {
    hoverHd = Handle::None;
    hoverIdx = detectHoveredCube(ctx.cursor, ctx.windowHalf.x, ctx.windowHalf.y);
    hoverLightIdx = detectHoveredLight(ctx.cursor, ctx.windowHalf.x, ctx.windowHalf.y, free);

    if (free && sel != -1 && !KeyAlt.pressed()) {
        const Cube& cb = cubes[sel];
        const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
        P2 ctr = ctx.project(cb.c);
        if (!std::isinf(ctr.x) && (mode == Mode::Move || mode == Mode::Scale)) {
            struct Ax { V3 d; Color col; Handle mv, sc; };
            const Ax AX[3] = {
                { {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
                { {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
                { {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
            };
            double bestA = 1e9;
            for (const auto& a : AX) {
                V3 dir = (mode == Mode::Scale) ? qRotate(cb.q, a.d) : a.d;
                double sc = (a.d.x != 0) ? cb.s.x : (a.d.y != 0) ? cb.s.y : cb.s.z;
                P2 tip = ctx.project(cb.c + dir * (HALF * 1.5 * sc));
                if (std::isinf(tip.x)) continue;
                double dLine = segDist2(ctx.cursor, { ctr.x,ctr.y }, { tip.x,tip.y });
                double dTip = (ctx.cursor - Vec2{ tip.x,tip.y }).lengthSq();
                if (mode == Mode::Move && dLine < 64 && dLine < bestA) {
                    bestA = dLine; hoverHd = a.mv;
                }
                else if (mode == Mode::Scale) {
                    if (dTip < 64 && dTip < bestA) {
                        bestA = dTip; hoverHd = a.sc;
                    }
                    else if (dLine < 64 && dLine < bestA) {
                        bestA = dLine; hoverHd = a.sc;
                    }
                }
            }
            if (mode == Mode::Scale && (ctx.cursor - Vec2{ ctr.x,ctr.y }).lengthSq() < 64)
                hoverHd = Handle::ScaleUniform;
        }
        if (mode == Mode::Rotate && !std::isinf(ctr.x)) {
            struct Ring { Handle hd; Color col; };
            const Ring RG[3] = {
                { Handle::RotateX, Palette::Red },
                { Handle::RotateY, Palette::Green },
                { Handle::RotateZ, Palette::Blue }
            };
            constexpr int SEG = 64;
            for (auto r : RG) {
                V3 axLocal = (r.hd == Handle::RotateX) ? V3{ 1,0,0 }
                        : (r.hd == Handle::RotateY) ? V3{ 0,1,0 }
                        : V3{ 0,0,1 };
                V3 ax = qRotate(cb.q, axLocal);
                double axF = dot(ax, ctx.forward);
                double bestR = 1e9;
                if (std::abs(axF) > 0.9) {
                    P2 sp = ctx.project(cb.c);
                    if (!std::isinf(sp.x)) {
                        V3 dir = cross(ax, ctx.right);
                        if (len(dir) < 1e-6) dir = cross(ax, ctx.up);
                        dir = norm(dir);
                        P2 tip = ctx.project(cb.c + dir * L);
                        if (!std::isinf(tip.x)) {
                            double rpx = (Vec2{ tip.x,tip.y } - Vec2{ sp.x,sp.y }).length();
                            bestR = std::abs((ctx.cursor - Vec2{ sp.x,sp.y }).length() - rpx);
                        }
                    }
                } else {
                    for (int k = 0; k < SEG; ++k) {
                        double a0 = Math::TwoPi * k / SEG,
                               a1 = Math::TwoPi * (k + 1) / SEG;
                        V3 p0, p1;
                        if (r.hd == Handle::RotateX) {
                            p0 = qRotate(cb.q, { 0,std::sin(a0) * L,std::cos(a0) * L });
                            p1 = qRotate(cb.q, { 0,std::sin(a1) * L,std::cos(a1) * L });
                        } else if (r.hd == Handle::RotateY) {
                            p0 = qRotate(cb.q, { std::sin(a0) * L,0,std::cos(a0) * L });
                            p1 = qRotate(cb.q, { std::sin(a1) * L,0,std::cos(a1) * L });
                        } else {
                            p0 = qRotate(cb.q, { std::sin(a0) * L,std::cos(a0) * L,0 });
                            p1 = qRotate(cb.q, { std::sin(a1) * L,std::cos(a1) * L,0 });
                        }
                        P2 s0 = ctx.project(cb.c + p0), s1 = ctx.project(cb.c + p1);
                        if (std::isinf(s0.x) || std::isinf(s1.x)) continue;
                        bestR = std::min(bestR, segDist2(ctx.cursor, { s0.x,s0.y }, { s1.x,s1.y }));
                    }
                }
                if (bestR < 64) { hoverHd = r.hd; break; }
            }
        }
    }
}

void Game::handleSelectionAndDragStart(const FrameContext& ctx, bool free) {
    if (!(free && MouseL.down())) {
        return;
    }

    auto scr = [&](V3 w) { return ctx.project(w); };

    // まずギズモがクリックされているか確認（優先）
    if (hoverHd != Handle::None && (sel != -1 || lightSel != -1)) {
        activeHd = hoverHd;
        drag.on = true;
        drag.cur0 = ctx.cursor;

        // ギズモ操作対象の設定
        if (sel != -1) {
            Cube& cb = cubes[sel];
            drag.p0 = cb.c;
            drag.q0 = cb.q;
            drag.s0 = cb.s;
            if (activeHd == Handle::MoveX || activeHd == Handle::MoveZ)
                grid.erase({ gIdx(cb.c.x), gIdx(cb.c.z) });
        }
        else if (lightSel != -1) {
            Light& lt = lights[lightSel];
            drag.p0 = lt.c;
            drag.q0 = lt.q;
            drag.s0 = lt.s;
        }

        // 移動・スケール用の軸計算
        if (activeHd == Handle::MoveX || activeHd == Handle::MoveY || activeHd == Handle::MoveZ ||
            activeHd == Handle::ScaleX || activeHd == Handle::ScaleY || activeHd == Handle::ScaleZ) {

            V3 axLocal = (activeHd == Handle::MoveX || activeHd == Handle::ScaleX) ? V3{ 1,0,0 } :
                    (activeHd == Handle::MoveY || activeHd == Handle::ScaleY) ? V3{ 0,1,0 } :
                    V3{ 0,0,1 };
            V3 ax = (activeHd == Handle::ScaleX || activeHd == Handle::ScaleY || activeHd == Handle::ScaleZ)
                    ? qRotate(drag.q0, axLocal) : axLocal;

            P2 p0 = scr(drag.p0), p1 = scr(drag.p0 + ax);
            drag.lenPx = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).length();
            if (drag.lenPx < 1) drag.lenPx = 1;
        }

        // 回転用の軸計算
        if (activeHd == Handle::RotateX || activeHd == Handle::RotateY || activeHd == Handle::RotateZ) {
            V3 axLocal = (activeHd == Handle::RotateX) ? V3{ 1,0,0 } :
                    (activeHd == Handle::RotateY) ? V3{ 0,1,0 } : V3{ 0,0,1 };
            drag.axis = qRotate(drag.q0, axLocal);
            drag.ang0 = ctx.cursorAngle(ctx.cursor, drag.axis, drag.p0).value_or(0.0);
        }
    }
    else {
        // ギズモがクリックされていなければオブジェクト選択
        if (hoverIdx != -1 || hoverLightIdx != -1) {
            const double dc = (hoverIdx != -1) ? len(cubes[hoverIdx].c - ctx.cam) : 1e9;
            const double dl = (hoverLightIdx != -1) ? len(lights[hoverLightIdx].c - ctx.cam) : 1e9;
            const bool pickLight = (dl < dc);

            if (pickLight) { lightSel = hoverLightIdx; sel = -1; }
            else { sel = hoverIdx; lightSel = -1; }
        }
    }
}

void Game::handleDragEnd() {
    if (MouseL.up()) {
        if (drag.on && sel != -1 && (activeHd == Handle::MoveX || activeHd == Handle::MoveZ)) {
            Cube& cb = cubes[sel];
            grid.insert({ gIdx(cb.c.x), gIdx(cb.c.z) });
        }
        drag.on = false; activeHd = Handle::None;
    }
}

void Game::updateDrag(const FrameContext& ctx) {
    if (!(drag.on && (sel != -1 || lightSel != -1))) {
        return;
    }

    Vec2 d = ctx.cursor - drag.cur0;

    bool isCube = (sel != -1);
    auto& objPos = isCube ? cubes[sel].c : lights[lightSel].c;
    auto& objRot = isCube ? cubes[sel].q : lights[lightSel].q;
    auto& objScl = isCube ? cubes[sel].s : lights[lightSel].s;

    /* Move */
    if (activeHd == Handle::MoveX || activeHd == Handle::MoveY || activeHd == Handle::MoveZ) {
        V3 axLocal = (activeHd == Handle::MoveX || activeHd == Handle::ScaleX) ? V3{ 1,0,0 }
                : (activeHd == Handle::MoveY || activeHd == Handle::ScaleY) ? V3{ 0,1,0 }
                : V3{ 0,0,1 };

        V3 ax = (isCube || activeHd == Handle::MoveX || activeHd == Handle::MoveY || activeHd == Handle::MoveZ)
                ? axLocal
                : qRotate(drag.q0, axLocal);
        P2 p0 = ctx.project(drag.p0), p1 = ctx.project(drag.p0 + ax);
        Vec2 dir = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).normalized();
        double pix = Dot(d, dir);
        double world = (pix / drag.lenPx) * 40.0;
        objPos = drag.p0 + ax * world;
    }

    /* Rotate */
    else if (activeHd == Handle::RotateX || activeHd == Handle::RotateY || activeHd == Handle::RotateZ) {
        if (auto ang = ctx.cursorAngle(ctx.cursor, drag.axis, drag.p0)) {
            double delta = *ang - drag.ang0;
            if (delta > s3d::Math::Pi)      delta -= s3d::Math::TwoPi;
            if (delta < -s3d::Math::Pi)     delta += s3d::Math::TwoPi;
            Quat dq = qAxisAngle(drag.axis, delta);
            objRot = qNormalize(qMul(dq, drag.q0));
        }
    }

    /* Scale */
    else if (activeHd == Handle::ScaleX || activeHd == Handle::ScaleY || activeHd == Handle::ScaleZ) {
        V3 axLocal = (activeHd == Handle::ScaleX) ? V3{ 1,0,0 }
                : (activeHd == Handle::ScaleY) ? V3{ 0,1,0 }
                : V3{ 0,0,1 };
        V3 ax = qRotate(drag.q0, axLocal);
        P2 p0 = ctx.project(drag.p0), p1 = ctx.project(drag.p0 + ax);
        Vec2 dir = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).normalized();
        double pix = Dot(d, dir);
        double f = 1.0 + pix * 0.002;
        V3 s = drag.s0;
        if (activeHd == Handle::ScaleX) s.x = Clamp(s.x * f, 0.1, 5.0);
        else if (activeHd == Handle::ScaleY) s.y = Clamp(s.y * f, 0.1, 5.0);
        else s.z = Clamp(s.z * f, 0.1, 5.0);
        objScl = s;
    }

    /* Uniform Scale */
    else {
        V3 f = drag.s0 * (1.0 + d.x * 0.002);
        f.x = Clamp(f.x, 0.1, 5.0);
        f.y = Clamp(f.y, 0.1, 5.0);
        f.z = Clamp(f.z, 0.1, 5.0);
        objScl = f;
    }
}

void Game::processInput(const FrameContext& ctx, double dt, bool free) {
    updateModeState(free);
    handleCreationUI(ctx, dt, free);
    updateHoverState(ctx, free);
    handleSelectionAndDragStart(ctx, free);
    handleDragEnd();
    updateDrag(ctx);
}

void Game::updateSimulation(double dt, bool free) {
    if (!free) {
        handleFPSMovement(dt);
    }
}

Game::Game() {
    for (int z = -2; z <= 2; ++z) {
        for (int x = -2; x <= 2; ++x) {
            cubes.push_back({ { gPos(x),0,gPos(z) } });
            grid.insert({ x, z });
        }
    }
}

int Game::detectHoveredCube(const Vec2& cur, double cx, double cy) {
    V3 cam = camera.cam;
    V3 Rv = camera.right();
    V3 U  = camera.up();
    V3 F  = camera.forward();

    int hover = -1; double bestDepth = 1e9;
    for (size_t i = 0; i < cubes.size(); ++i) {
        double depth = 1e9;
        if (cubes[i].checkHovered(cur, cx, cy, cam, Rv, U, F, depth)) {
            if (depth < bestDepth) {
                bestDepth = depth;
                hover = static_cast<int>(i);
            }
        }
    }
    return hover;
}

int Game::detectHoveredLight(const Vec2& cur, double cx, double cy, bool free) {
        V3 cam = camera.cam;
        V3 Rv = camera.right();
        V3 U = camera.up();
        V3 F = camera.forward();
        auto scr = [&](V3 w) { return screenProject(w, cam, Rv, U, F, cx, cy); };

        int hover = -1; double bestDepth = 1e9;
        for (size_t i = 0; i < lights.size(); ++i) {
                double depth = 1e9;
                if (lights[i].checkHovered(cur, cx, cy, cam, Rv, U, F, depth)) {
                        if (depth < bestDepth) {
                                bestDepth = depth;
                                hover = static_cast<int>(i);
                        }
                }
        }

	if (free && lightSel != -1 && !KeyAlt.pressed()) {
		const Light& lt = lights[lightSel];
		const double L = HALF * std::max({ lt.s.x, lt.s.y, lt.s.z }) * 1.5;
		P2 ctr = scr(lt.c);
		if (!std::isinf(ctr.x) && (mode == Mode::Move || mode == Mode::Scale)) {
			struct Ax { V3 d; Color col; Handle mv, sc; };
			const Ax AX[3] = {
				{ {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
				{ {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
				{ {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
			};
			double bestA = 1e9;
			for (const auto& a : AX) {
				V3 dir = (mode == Mode::Scale) ? qRotate(lt.q, a.d) : a.d;
				double sc = (a.d.x != 0) ? lt.s.x : (a.d.y != 0) ? lt.s.y : lt.s.z;
				P2 tip = scr(lt.c + dir * (HALF * 1.5 * sc));
				if (std::isinf(tip.x)) continue;
				double dLine = segDist2(cur, { ctr.x,ctr.y }, { tip.x,tip.y });
				double dTip = (cur - Vec2{ tip.x,tip.y }).lengthSq();
				if (mode == Mode::Move && dLine < 64 && dLine < bestA) {
					bestA = dLine; hoverHd = a.mv;
				}
				else if (mode == Mode::Scale) {
					if (dTip < 64 && dTip < bestA) {
						bestA = dTip; hoverHd = a.sc;
					}
					else if (dLine < 64 && dLine < bestA) {
						bestA = dLine; hoverHd = a.sc;
					}
				}
			}
			if (mode == Mode::Scale && (cur - Vec2{ ctr.x,ctr.y }).lengthSq() < 64)
				hoverHd = Handle::ScaleUniform;
		}
		if (mode == Mode::Rotate && !std::isinf(ctr.x)) {
			struct Ring { Handle hd; Color col; };
			const Ring RG[3] = {
				{ Handle::RotateX, Palette::Red },
				{ Handle::RotateY, Palette::Green },
				{ Handle::RotateZ, Palette::Blue }
			};
			constexpr int SEG = 64;
			for (auto r : RG) {
				V3 axLocal = (r.hd == Handle::RotateX) ? V3{ 1,0,0 }
					: (r.hd == Handle::RotateY) ? V3{ 0,1,0 }
				: V3{ 0,0,1 };
				V3 ax = qRotate(lt.q, axLocal);
				double axF = dot(ax, F);
				double bestR = 1e9;
				if (std::abs(axF) > 0.9) {
					P2 sp = scr(lt.c);
					if (!std::isinf(sp.x)) {
						V3 dir = cross(ax, Rv);
						if (len(dir) < 1e-6) dir = cross(ax, U);
						dir = norm(dir);
						P2 tip = scr(lt.c + dir * L);
						if (!std::isinf(tip.x)) {
							double rpx = (Vec2{ tip.x,tip.y } - Vec2{ sp.x,sp.y }).length();
							bestR = std::abs((cur - Vec2{ sp.x,sp.y }).length() - rpx);
						}
					}
				}
				else {
					for (int k = 0; k < SEG; ++k) {
						double a0 = Math::TwoPi * k / SEG,
							a1 = Math::TwoPi * (k + 1) / SEG;
						V3 p0, p1;
						if (r.hd == Handle::RotateX) {
							p0 = qRotate(lt.q, { 0,std::sin(a0) * L,std::cos(a0) * L });
							p1 = qRotate(lt.q, { 0,std::sin(a1) * L,std::cos(a1) * L });
						}
						else if (r.hd == Handle::RotateY) {
							p0 = qRotate(lt.q, { std::sin(a0) * L,0,std::cos(a0) * L });
							p1 = qRotate(lt.q, { std::sin(a1) * L,0,std::cos(a1) * L });
						}
						else {
							p0 = qRotate(lt.q, { std::sin(a0) * L,std::cos(a0) * L,0 });
							p1 = qRotate(lt.q, { std::sin(a1) * L,std::cos(a1) * L,0 });
						}
						P2 s0 = scr(lt.c + p0), s1 = scr(lt.c + p1);
						if (std::isinf(s0.x) || std::isinf(s1.x)) continue;
						bestR = std::min(bestR, segDist2(cur, { s0.x,s0.y }, { s1.x,s1.y }));
					}
				}
				if (bestR < 64) { hoverHd = r.hd; break; }
			}
		}
	}
	return hover;
}

void Game::handleFPSMovement(double dt) {
    V3 F  = camera.forward();
    V3 Rv = camera.right();
    V3 Fh = camera.forwardH();
    V3& pos = camera.pos;
    V3& cam = camera.cam;
    double& vy = camera.vy;

    if (KeyW.pressed()) pos = pos - MOVE * dt * Fh;
    if (KeyS.pressed()) pos = pos + MOVE * dt * Fh;
    if (KeyD.pressed()) pos = pos + MOVE * dt * Rv;
    if (KeyA.pressed()) pos = pos - MOVE * dt * Rv;

    bool onGround = false; double foot = pos.y - EYE, head = foot + CAP_H;
    for (const auto& cb : cubes) {
        double hx = HALF * cb.s.x;
        double hy = HALF * cb.s.y;
        double hz = HALF * cb.s.z;

        Quat invQ = qConj(cb.q);
        V3 posL  = qRotate(invQ, pos - cb.c);
        V3 footL = qRotate(invQ, V3{ pos.x, foot, pos.z } - cb.c);
        V3 headL = qRotate(invQ, V3{ pos.x, head, pos.z } - cb.c);

        double dynamicEps = Max(2.0, (-vy) * dt + 0.5);
        if (vy <= 0 && footL.y >= hy - dynamicEps && footL.y <= hy + dynamicEps) {
            double dx = std::max(std::abs(footL.x) - hx, 0.0);
            double dz = std::max(std::abs(footL.z) - hz, 0.0);
            if (dx * dx + dz * dz <= R * R) {
                double delta = hy - footL.y;
                V3 adj = delta * qRotate(cb.q, { 0,1,0 });
                pos = pos + adj;
                vy = 0; onGround = true;
                foot += adj.y; head += adj.y;
                posL = qRotate(invQ, pos - cb.c);
                footL = qRotate(invQ, V3{ pos.x, foot, pos.z } - cb.c);
                headL = qRotate(invQ, V3{ pos.x, head, pos.z } - cb.c);
            }
        }

        if (headL.y <= -hy || footL.y >= hy) continue;

        double clx = std::clamp(posL.x, -hx, hx);
        double clz = std::clamp(posL.z, -hz, hz);
        double dx = posL.x - clx, dz = posL.z - clz;
        double d2 = dx * dx + dz * dz;
        if (d2 < R * R - 1e-6) {
            double d = std::sqrt(std::max(d2, 1e-6));
            V3 pushL{ dx,0,dz }; pushL = ((R - d) / d) * pushL;
            V3 pushW = qRotate(cb.q, pushL);
            pos = pos + pushW;
            foot += pushW.y; head += pushW.y;
        }
    }

    if (KeySpace.down() && onGround) vy = JUMP; else vy -= GRAV * dt;
    pos.y += vy * dt; cam = pos + V3{ 0,EYE,0 };
}

GKey Game::findNearestEmpty(GKey start) {
    if (grid.find(start) == grid.end()) return start;
    constexpr int MAX_RADIUS = 20;
    for (int r = 1; r <= MAX_RADIUS; ++r) {
        for (int dz = -r; dz <= r; ++dz) {
            int dx = r - std::abs(dz);
            GKey g1{ start.gx + dx, start.gz + dz };
            if (grid.find(g1) == grid.end()) return g1;
            if (dx != 0) {
                GKey g2{ start.gx - dx, start.gz + dz };
                if (grid.find(g2) == grid.end()) return g2;
            }
        }
    }
    return start;
}

void Game::drawFrame(const FrameContext& ctx, bool free) {
    auto scr = [&](V3 w) { return ctx.project(w); };
    const V3 cam = ctx.cam;
    const V3 F = ctx.forward;
    const V3 right = ctx.right;
    const V3 U = ctx.up;
    const V3 pos = camera.pos;

    constexpr double DEPTH_EPS = 1e-4;
    constexpr double DEP_TOL = 1e-6;

    std::vector<std::pair<double, int>> drawOrder(cubes.size());
    for (size_t i = 0; i < cubes.size(); ++i) {
        drawOrder[i] = { dot(cubes[i].c - cam, F), static_cast<int>(i) };
    }
    std::stable_sort(drawOrder.begin(), drawOrder.end(),
        [&](const auto& a, const auto& b) {
            if (std::abs(a.first - b.first) > DEP_TOL) {
                return a.first > b.first;
            }
            return a.second < b.second;
        });

    struct FaceDrawG { double d; std::array<P2, 3> p; ColorF col; bool selected; int cubeIdx; int faceIdx; int triIdx; };
    struct EdgeDrawG { double d; P2 a, b; ColorF col; double th; bool selected; int cubeIdx; int edgeIdx; };
    std::vector<FaceDrawG> faces;
    std::vector<EdgeDrawG> edges;

    for (auto [_, idx] : drawOrder) {
        const Cube& cb = cubes[idx];

        ColorF col = ColorF{ 1.0, 0.8, 0.3 };
        if (free && idx == hoverIdx) col = ColorF(Palette::Yellow);
        if (idx == sel)              col = ColorF(Palette::Red);
        double th = (idx == sel) ? 3 : 1;

        std::array<V3, 8> vw;
        std::array<V3, 8> vv;
        std::array<P2, 8> vp;
        for (int k = 0; k < 8; ++k) {
            V3 p = LOCAL[k] * cb.s;
            vw[k] = cb.c + qRotate(cb.q, p);
            V3 r = vw[k] - cam;
            vv[k] = { dot(r, right), dot(r, U), -dot(r, F) };
            vp[k] = project(vv[k], ctx.windowHalf.x, ctx.windowHalf.y);
        }

        for (size_t fi = 0; fi < FACE.size(); ++fi) {
            const auto& f = FACE[fi];
            V3 v0 = vw[f[0]];
            V3 v1 = vw[f[1]];
            V3 v2 = vw[f[2]];
            V3 v3 = vw[f[3]];
            V3 nW = norm(cross(v1 - v0, v2 - v0));
            
            const double AMBIENT = 0.15;
            double shade = AMBIENT;

            for (const auto& lt : lights) {
                V3 L = -1 * lt.dir();
                double diff = Max(0.0, dot(nW, L));
                shade += lt.intensity * diff;
            }

            shade = Clamp(shade, 0.0, 2.0);
            ColorF shaded = col * shade;

            bool isSel = (idx == sel);
            shaded = col * shade;
            const std::array<std::array<int, 3>, 2> triIdx{ {
                { f[0], f[1], f[2] },
                { f[0], f[2], f[3] }
            } };
            for (int tri = 0; tri < 2; ++tri) {
                std::array<V3, 3> triView{ vv[triIdx[tri][0]], vv[triIdx[tri][1]], vv[triIdx[tri][2]] };
                auto clippedTris = clipTriangleToNearPlane(triView);
                for (const auto& clipTri : clippedTris) {
                    std::array<P2, 3> clipProj{ project(clipTri[0], ctx.windowHalf.x, ctx.windowHalf.y),
                                                project(clipTri[1], ctx.windowHalf.x, ctx.windowHalf.y),
                                                project(clipTri[2], ctx.windowHalf.x, ctx.windowHalf.y) };
                    double depth = (-clipTri[0].z - clipTri[1].z - clipTri[2].z) / 3.0;
                    faces.push_back({ depth, clipProj, shaded, isSel,
                                      idx, static_cast<int>(fi), tri });
                }
            }
        }

        for (size_t ei = 0; ei < EDGE.size(); ++ei) {
            auto [a, b] = EDGE[ei];
            if (std::isinf(vp[a].x) || std::isinf(vp[b].x)) {
                continue;
            }

            double depthEdge = (dot(vw[a] - cam, F) + dot(vw[b] - cam, F)) * 0.5;
            bool isSel = (idx == sel);
            if (isSel) depthEdge -= DEPTH_EPS;

            edges.push_back({ depthEdge, vp[a], vp[b], col, th, isSel, idx, static_cast<int>(ei) });
        }
    }

    std::sort(faces.begin(), faces.end(),
        [&](const FaceDrawG& a, const FaceDrawG& b) {
            if (std::abs(a.d - b.d) > DEP_TOL) {
                return a.d < b.d;
            }
            if (a.cubeIdx != b.cubeIdx) {
                return a.cubeIdx < b.cubeIdx;
            }
            if (a.faceIdx != b.faceIdx) {
                return a.faceIdx < b.faceIdx;
            }
            return a.triIdx < b.triIdx;
        });
    for (const auto& fc : faces) {
        Polygon{ Vec2{fc.p[0].x,fc.p[0].y}, Vec2{fc.p[1].x,fc.p[1].y},
                 Vec2{fc.p[2].x,fc.p[2].y} }.draw(fc.col);
    }

    std::stable_sort(edges.begin(), edges.end(),
        [](const EdgeDrawG& a, const EdgeDrawG& b) {
            if (std::abs(a.d - b.d) > DEP_TOL) {
                return a.d > b.d;
            }
            if (a.selected != b.selected) {
                return !a.selected && b.selected;
            }
            if (a.cubeIdx != b.cubeIdx) {
                return a.cubeIdx < b.cubeIdx;
            }
            return a.edgeIdx < b.edgeIdx;
        });

    for (size_t i = 0; i < lights.size(); ++i) {
        const Light& lt = lights[i];

        std::array<P2, 8> vp;
        for (int k = 0; k < 8; ++k) {
            V3 p = qRotate(lt.q, LOCAL[k] * lt.s);
            vp[k] = scr(lt.c + p);
        }

        ColorF col = (static_cast<int>(i) == lightSel) ? ColorF(Palette::Yellow)
                                                       : ColorF(Palette::White);
        for (auto [a, b] : EDGE) {
            if (std::isinf(vp[a].x) || std::isinf(vp[b].x)) {
                continue;
            }
            Line{ vp[a].x, vp[a].y, vp[b].x, vp[b].y }.draw(2, col);
        }

        P2 a = scr(lt.c);
        P2 b = scr(lt.c + lt.dir() * 60.0);
        if (!std::isinf(a.x) && !std::isinf(b.x)) {
            Line{ a.x,a.y, b.x,b.y }.draw(3, col);
            Vec2 v = (Vec2{ b.x,b.y } - Vec2{ a.x,a.y }).normalized() * 6;
            Vec2 n{ -v.y, v.x };
            Polygon{ Vec2{b.x,b.y}, Vec2{b.x,b.y} - v * 2 + n, Vec2{b.x,b.y} - v * 2 - n }.draw(col);
        }
    }

    if (free && sel != -1) {
        const Cube& cb = cubes[sel];
        const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
        P2 ctr = scr(cb.c);
        if (std::isinf(ctr.x)) {
            return;
        }

        if (mode == Mode::Move || mode == Mode::Scale) {
            struct Ax { V3 d; Color col; Handle mv, sc; };
            const Ax AX[3] = {
                { {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
                { {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
                { {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
            };
            for (const auto& a : AX) {
                V3 dir = (mode == Mode::Scale) ? qRotate(cb.q, a.d) : a.d;
                double sc = (a.d.x != 0) ? cb.s.x : (a.d.y != 0) ? cb.s.y : cb.s.z;
                P2 tip = scr(cb.c + dir * (HALF * 1.5 * sc));
                if (std::isinf(tip.x)) {
                    continue;
                }
                bool hot = (hoverHd == a.mv || hoverHd == a.sc ||
                            activeHd == a.mv || activeHd == a.sc);

                ColorF c = a.col; c.a = hot ? 1.0 : 0.4;
                Line{ ctr.x,ctr.y, tip.x,tip.y }.draw(hot ? 4 : 3, c);

                if (mode == Mode::Move) {
                    Vec2 v2 = (Vec2{ tip.x,tip.y } - Vec2{ ctr.x,ctr.y }).normalized() * 8;
                    Vec2 n{ -v2.y,v2.x };
                    Polygon{ Vec2{tip.x,tip.y}, Vec2{tip.x,tip.y} - v2 * 2 + n,
                             Vec2{tip.x,tip.y} - v2 * 2 - n }.draw(c);
                }
                else {
                    RectF{ tip.x - 4, tip.y - 4, 8,8 }.draw(c);
                }
            }
            if (mode == Mode::Scale) {
                bool hot = (hoverHd == Handle::ScaleUniform || activeHd == Handle::ScaleUniform);
                ColorF cu = hot ? ColorF(Palette::White) : ColorF{ 1,1,1,0.4 };
                Circle{ ctr.x,ctr.y,5 }.drawFrame(2, cu);
            }
        }

        if (mode == Mode::Rotate) {
            struct Ring { Handle hd; Color col; };
            const Ring RG[3] = {
                { Handle::RotateX, Palette::Red   },
                { Handle::RotateY, Palette::Green },
                { Handle::RotateZ, Palette::Blue  }
            };
            constexpr int SEG = 64;
            for (auto r : RG) {
                bool hot = (hoverHd == r.hd || activeHd == r.hd);
                ColorF col = r.col; col.a = hot ? 1.0 : 0.4;
                for (int k = 0; k < SEG; ++k) {
                    double a0 = Math::TwoPi * k / SEG;
                    double a1 = Math::TwoPi * (k + 1) / SEG;
                    V3 p0, p1;
                    if (r.hd == Handle::RotateX) {
                        p0 = qRotate(cb.q, { 0,std::sin(a0) * L,std::cos(a0) * L });
                        p1 = qRotate(cb.q, { 0,std::sin(a1) * L,std::cos(a1) * L });
                    }
                    else if (r.hd == Handle::RotateY) {
                        p0 = qRotate(cb.q, { std::sin(a0) * L,0,std::cos(a0) * L });
                        p1 = qRotate(cb.q, { std::sin(a1) * L,0,std::cos(a1) * L });
                    }
                    else {
                        p0 = qRotate(cb.q, { std::sin(a0) * L,std::cos(a0) * L,0 });
                        p1 = qRotate(cb.q, { std::sin(a1) * L,std::cos(a1) * L,0 });
                    }
                    P2 s0 = scr(cb.c + p0), s1 = scr(cb.c + p1);
                    if (!std::isinf(s0.x) && !std::isinf(s1.x)) {
                        Line{ s0.x,s0.y, s1.x,s1.y }.draw(hot ? 3 : 2, col);
                    }
                }
            }
        }
    }
    else if (free && lightSel != -1) {
        const Light& cb = lights[lightSel];
        const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
        P2 ctr = scr(cb.c);
        if (std::isinf(ctr.x)) {
            return;
        }

        if (mode == Mode::Move || mode == Mode::Scale) {
            struct Ax { V3 d; Color col; Handle mv, sc; };
            const Ax AX[3] = {
                { {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
                { {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
                { {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
            };
            for (const auto& a : AX) {
                V3 dir = (mode == Mode::Scale) ? qRotate(cb.q, a.d) : a.d;
                double sc = (a.d.x != 0) ? cb.s.x : (a.d.y != 0) ? cb.s.y : cb.s.z;
                P2 tip = scr(cb.c + dir * (HALF * 1.5 * sc));
                if (std::isinf(tip.x)) {
                    continue;
                }
                bool hot = (hoverHd == a.mv || hoverHd == a.sc ||
                            activeHd == a.mv || activeHd == a.sc);

                ColorF c = a.col; c.a = hot ? 1.0 : 0.4;
                Line{ ctr.x,ctr.y, tip.x,tip.y }.draw(hot ? 4 : 3, c);

                if (mode == Mode::Move) {
                    Vec2 v2 = (Vec2{ tip.x,tip.y } - Vec2{ ctr.x,ctr.y }).normalized() * 8;
                    Vec2 n{ -v2.y,v2.x };
                    Polygon{ Vec2{tip.x,tip.y}, Vec2{tip.x,tip.y} - v2 * 2 + n,
                             Vec2{tip.x,tip.y} - v2 * 2 - n }.draw(c);
                }
                else {
                    RectF{ tip.x - 4, tip.y - 4, 8,8 }.draw(c);
                }
            }
            if (mode == Mode::Scale) {
                bool hot = (hoverHd == Handle::ScaleUniform || activeHd == Handle::ScaleUniform);
                ColorF cu = hot ? ColorF(Palette::White) : ColorF{ 1,1,1,0.4 };
                Circle{ ctr.x,ctr.y,5 }.drawFrame(2, cu);
            }
        }

        if (mode == Mode::Rotate) {
            struct Ring { Handle hd; Color col; };
            const Ring RG[3] = {
                { Handle::RotateX, Palette::Red   },
                { Handle::RotateY, Palette::Green },
                { Handle::RotateZ, Palette::Blue  }
            };
            constexpr int SEG = 64;
            for (auto r : RG) {
                bool hot = (hoverHd == r.hd || activeHd == r.hd);
                ColorF col = r.col; col.a = hot ? 1.0 : 0.4;
                for (int k = 0; k < SEG; ++k) {
                    double a0 = Math::TwoPi * k / SEG;
                    double a1 = Math::TwoPi * (k + 1) / SEG;
                    V3 p0, p1;
                    if (r.hd == Handle::RotateX) {
                        p0 = qRotate(cb.q, { 0,std::sin(a0) * L,std::cos(a0) * L });
                        p1 = qRotate(cb.q, { 0,std::sin(a1) * L,std::cos(a1) * L });
                    }
                    else if (r.hd == Handle::RotateY) {
                        p0 = qRotate(cb.q, { std::sin(a0) * L,0,std::cos(a0) * L });
                        p1 = qRotate(cb.q, { std::sin(a1) * L,0,std::cos(a1) * L });
                    }
                    else {
                        p0 = qRotate(cb.q, { std::sin(a0) * L,std::cos(a0) * L,0 });
                        p1 = qRotate(cb.q, { std::sin(a1) * L,std::cos(a1) * L,0 });
                    }
                    P2 s0 = scr(cb.c + p0), s1 = scr(cb.c + p1);
                    if (!std::isinf(s0.x) && !std::isinf(s1.x)) {
                        Line{ s0.x,s0.y, s1.x,s1.y }.draw(hot ? 3 : 2, col);
                    }
                }
            }
        }
    }

    if (!free) {
        constexpr int SEG = 24;
        constexpr double DEP_TOL = 1e-6;

        double foot = pos.y - EYE;
        double top = foot + CAP_H;

        std::array<V3, SEG> bv, tv;
        std::array<P2, SEG> bp, tp;

        for (int k = 0; k < SEG; ++k) {
            double a = Math::TwoPi * k / SEG;
            bv[k] = { pos.x + R * std::cos(a), foot, pos.z + R * std::sin(a) };
            tv[k] = { bv[k].x, top, bv[k].z };
            bp[k] = scr(bv[k]);
            tp[k] = scr(tv[k]);
        }

        struct QuadDraw { double d; std::vector<P2> p; };
        struct EdgeDraw { double d; P2 a, b; double th; };

        std::vector<QuadDraw> qs;
        std::vector<EdgeDraw> es;

        for (int k = 0; k < SEG; ++k) {
            int k1 = (k + 1) % SEG;
            if (std::isinf(bp[k].x) || std::isinf(bp[k1].x) ||
                std::isinf(tp[k].x) || std::isinf(tp[k1].x)) {
                continue;
            }

            double d = (dot(bv[k] - cam, F) + dot(bv[k1] - cam, F) +
                        dot(tv[k1] - cam, F) + dot(tv[k] - cam, F)) * 0.25;
            qs.push_back({ d, { bp[k], bp[k1], tp[k1], tp[k] } });

            double dv = 0.5 * (dot(bv[k] - cam, F) + dot(tv[k] - cam, F));
            es.push_back({ dv, bp[k], tp[k], 2.0 });

            double dt = 0.5 * (dot(tv[k] - cam, F) + dot(tv[k1] - cam, F));
            es.push_back({ dt, tp[k], tp[k1], 2.0 });

            double db = 0.5 * (dot(bv[k] - cam, F) + dot(bv[k1] - cam, F));
            es.push_back({ db, bp[k1], bp[k], 2.0 });
        }

        {
            V3 topC{ pos.x, top, pos.z };
            if (dot(V3{ 0,1,0 }, topC - cam) < 0) {
                std::vector<P2> poly(SEG);
                double d = 0;
                bool ok = true;
                for (int k = 0; k < SEG; ++k) {
                    if (std::isinf(tp[k].x)) {
                        ok = false;
                        break;
                    }
                    poly[k] = tp[k];
                    d += dot(tv[k] - cam, F);
                }
                if (ok) {
                    qs.push_back({ d / SEG, poly });
                }
            }
        }

        {
            V3 botC{ pos.x, foot, pos.z };
            if (dot(V3{ 0,-1,0 }, botC - cam) < 0) {
                std::vector<P2> poly(SEG);
                double d = 0;
                bool ok = true;
                for (int k = 0; k < SEG; ++k) {
                    int kk = SEG - 1 - k;
                    if (std::isinf(bp[kk].x)) {
                        ok = false;
                        break;
                    }
                    poly[k] = bp[kk];
                    d += dot(bv[kk] - cam, F);
                }
                if (ok) {
                    qs.push_back({ d / SEG, poly });
                }
            }
        }

        std::stable_sort(qs.begin(), qs.end(),
            [&](const QuadDraw& a, const QuadDraw& b) {
                if (std::abs(a.d - b.d) > DEP_TOL) {
                    return a.d < b.d;
                }
                return false;
            });
        std::stable_sort(es.begin(), es.end(),
            [&](const EdgeDraw& a, const EdgeDraw& b) {
                if (std::abs(a.d - b.d) > DEP_TOL) {
                    return a.d < b.d;
                }
                return false;
            });

        for (const auto& q : qs) {
            if (q.p.size() == 4) {
                Polygon{ Vec2{ q.p[0].x,q.p[0].y }, Vec2{ q.p[1].x,q.p[1].y },
                         Vec2{ q.p[2].x,q.p[2].y }, Vec2{ q.p[3].x,q.p[3].y } }
                    .draw(ColorF(Palette::Cyan, 0.4));
            }
            else {
                Array<Vec2> arr(q.p.size());
                for (size_t i = 0; i < q.p.size(); ++i) {
                    arr[i] = Vec2{ q.p[i].x, q.p[i].y };
                }
                Polygon{ arr }.draw(ColorF(Palette::Cyan, 0.4));
            }
        }

        for (const auto& e : es) {
            Line{ e.a.x, e.a.y, e.b.x, e.b.y }.draw(e.th, ColorF(Palette::Cyan));
        }
    }
}

void Game::run() {
    Scene::SetBackground(ColorF{ 0,0,0 });
    const Vec2 WINF{ Scene::Width() * 0.5, Scene::Height() * 0.5 };
    const Point WINP{ (int32)WINF.x, (int32)WINF.y };

    while (System::Update()) {
        const double dt = Scene::DeltaTime();
        camera.update(dt, WINF, WINP);

        FrameContext ctx = buildFrameContext(WINF);
        bool free = camera.free;

        processInput(ctx, dt, free);
        updateSimulation(dt, free);
        drawFrame(ctx, free);
    }
}
