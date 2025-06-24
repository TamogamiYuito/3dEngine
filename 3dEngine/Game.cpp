#include "Game.hpp"
#include "RenderUtils.hpp"
#include <Siv3D.hpp>
#include <algorithm>

using namespace s3d;

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
    auto scr = [&](V3 w) { return screenProject(w, cam, Rv, U, F, cx, cy); };

    int hover = -1; double bestDepth = 1e9;
    for (size_t i = 0; i < cubes.size(); ++i) {
        const Cube& cb = cubes[i];
        double minX = 1e9, minY = 1e9;
        double maxX = -1e9, maxY = -1e9;
        bool any = false;
        for (int k = 0; k < 8; ++k) {
            P2 p = scr(cb.c + qRotate(cb.q, LOCAL[k] * cb.s));
            if (std::isinf(p.x)) continue;
            any = true;
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        if (!any) continue;
        if (cur.x >= minX && cur.x <= maxX && cur.y >= minY && cur.y <= maxY) {
            double depth = len(cb.c - cam);
            if (depth < bestDepth) {
                bestDepth = depth;
                hover = static_cast<int>(i);
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
    if (!grid.contains(start)) return start;
    constexpr int MAX_RADIUS = 20;
    for (int r = 1; r <= MAX_RADIUS; ++r) {
        for (int dz = -r; dz <= r; ++dz) {
            int dx = r - std::abs(dz);
            GKey g1{ start.gx + dx, start.gz + dz };
            if (!grid.contains(g1)) return g1;
            if (dx != 0) {
                GKey g2{ start.gx - dx, start.gz + dz };
                if (!grid.contains(g2)) return g2;
            }
        }
    }
    return start;
}

void Game::run() {
    Scene::SetBackground(ColorF{ 0.92,0.95,1 });
    const Vec2  WINF{ Scene::Width() * 0.5, Scene::Height() * 0.5 };
    const Point WINP{ (int32)WINF.x,(int32)WINF.y };

    while (System::Update()) {
        const double dt = Scene::DeltaTime();
        const Vec2 cur = Cursor::PosF();

        camera.update(dt, WINF, WINP);
        V3 F  = camera.forward();
        V3 Rv = camera.right();
        V3 U  = camera.up();
        V3 Fh = camera.forwardH();
        V3& cam = camera.cam;
        V3& pos = camera.pos;
        double& vy = camera.vy;
        bool free = camera.free;

        if (free && !drag.on) {
            if (KeyW.down()) mode = Mode::Move;
            if (KeyE.down()) mode = Mode::Rotate;
            if (KeyR.down()) mode = Mode::Scale;
        }

        if (free && SimpleGUI::Button(U"+ Cube", { 20,20 })) {
            V3 Fh = camera.forwardH();
            V3 Rv = camera.right();
            double ang = Random(-Math::Pi / 4.0, Math::Pi / 4.0);
            V3 dir = norm(std::cos(ang) * (-1*Fh) + std::sin(ang) * Rv);
            double dist = Random(2.0, 4.0) * (2.0 * HALF);
            double yRand = Random(0, 2) * (2.0 * HALF);
            V3 hit = V3{ cam.x, yRand, cam.z } + dist * dir;
            GKey g{ gIdx(hit.x), gIdx(hit.z) };
            GKey place = findNearestEmpty(g);
            if (!grid.contains(place)) {
                cubes.push_back({ { gPos(place.gx), yRand, gPos(place.gz) } });
                grid.insert(place);
            }
        }

        auto scr = [&](V3 w) { return screenProject(w, cam, Rv, U, F, WINF.x, WINF.y); };
        auto toView = [&](V3 w) {
            V3 r = w - cam;
            return V3{ dot(r, Rv), dot(r, U), -dot(r, F) };
        };
        constexpr double EPS = 1e-4;
        auto cursorAngle = [&](Vec2 p, V3 axis, V3 pivot)->std::optional<double> {
            return angleFromCursor(p, axis, pivot, cam, Rv, U, F, WINF.x, WINF.y);
        };

        hoverIdx = detectHoveredCube(cur, WINF.x, WINF.y);
        hoverHd = Handle::None;
        if (free && sel != -1 && !KeyAlt.pressed()) {
            const Cube& cb = cubes[sel];
            const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
            P2 ctr = scr(cb.c);
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
                    P2 tip = scr(cb.c + dir * (HALF * 1.5 * sc));
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
                    V3 ax = qRotate(cb.q, axLocal);
                    double axF = dot(ax, F);
                    double bestR = 1e9;
                    if (std::abs(axF) > 0.9) {
                        P2 sp = scr(cb.c);
                        if (!std::isinf(sp.x)) {
                            V3 dir = cross(ax, Rv);
                            if (len(dir) < 1e-6) dir = cross(ax, U);
                            dir = norm(dir);
                            P2 tip = scr(cb.c + dir * L);
                            if (!std::isinf(tip.x)) {
                                double rpx = (Vec2{ tip.x,tip.y } - Vec2{ sp.x,sp.y }).length();
                                bestR = std::abs((cur - Vec2{ sp.x,sp.y }).length() - rpx);
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
                            P2 s0 = scr(cb.c + p0), s1 = scr(cb.c + p1);
                            if (std::isinf(s0.x) || std::isinf(s1.x)) continue;
                            bestR = std::min(bestR, segDist2(cur, {s0.x,s0.y }, { s1.x,s1.y }));
                        }
                    }
                    if (bestR < 64) { hoverHd = r.hd; break; }
                }
            }
        }

        if (free && MouseL.down()) {
            if (hoverHd != Handle::None && sel != -1) {
                activeHd = hoverHd;
                drag.on = true;
                drag.cur0 = cur;
                Cube& cb = cubes[sel];
                drag.p0 = cb.c;
                drag.q0 = cb.q;
                drag.s0 = cb.s;
                if (activeHd == Handle::MoveX || activeHd == Handle::MoveZ)
                    grid.erase({ gIdx(cb.c.x), gIdx(cb.c.z) });
                if (activeHd == Handle::MoveX || activeHd == Handle::MoveY
                 || activeHd == Handle::MoveZ || activeHd == Handle::ScaleX
                 || activeHd == Handle::ScaleY || activeHd == Handle::ScaleZ) {
                    V3 axLocal = (activeHd == Handle::MoveX || activeHd == Handle::ScaleX) ? V3{ 1,0,0 } :
                                 (activeHd == Handle::MoveY || activeHd == Handle::ScaleY) ? V3{ 0,1,0 } :
                                 V3{ 0,0,1 };
                    V3 ax = (activeHd == Handle::ScaleX || activeHd == Handle::ScaleY || activeHd == Handle::ScaleZ)
                        ? qRotate(cb.q, axLocal) : axLocal;
                    P2 p0 = scr(cb.c), p1 = scr(cb.c + ax);
                    drag.lenPx = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).length();
                    if (drag.lenPx < 1) drag.lenPx = 1;
                }
                if (activeHd == Handle::RotateX || activeHd == Handle::RotateY || activeHd == Handle::RotateZ) {
                    V3 axLocal = (activeHd == Handle::RotateX) ? V3{ 1,0,0 }
                            : (activeHd == Handle::RotateY) ? V3{ 0,1,0 }
                            : V3{ 0,0,1 };
                    drag.axis = qRotate(cb.q, axLocal);
                    drag.ang0 = cursorAngle(cur, drag.axis, cb.c).value_or(0.0);
                }
            } else if (hoverIdx != -1) {
                sel = hoverIdx;
            }
        }

        if (free && MouseL.up()) {
            if (drag.on && (activeHd == Handle::MoveX || activeHd == Handle::MoveZ)) {
                Cube& cb = cubes[sel];
                grid.insert({ gIdx(cb.c.x), gIdx(cb.c.z) });
            }
            drag.on = false; activeHd = Handle::None;
        }

        /*--- ドラッグ処理 ---*/
        if (drag.on && sel != -1) {
            Vec2 d = cur - drag.cur0;
            Cube& cb = cubes[sel];

            /* Move */
            if (activeHd == Handle::MoveX || activeHd == Handle::MoveY || activeHd == Handle::MoveZ) {
                V3 ax = (activeHd == Handle::MoveX) ? V3{ 1,0,0 }
                        : (activeHd == Handle::MoveY) ? V3{ 0,1,0 } : V3{ 0,0,1 };
                P2 p0 = scr(drag.p0), p1 = scr(drag.p0 + ax);
                Vec2 dir = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).normalized();
                double pix = Dot(d, dir);
                double world = (pix / drag.lenPx) * 40.0;   // 1px = 40 world
                cb.c = drag.p0 + ax * world;
            }
            /* Rotate: apply cursor angle difference */
            else if (activeHd == Handle::RotateX || activeHd == Handle::RotateY || activeHd == Handle::RotateZ) {
                if (auto ang = cursorAngle(cur, drag.axis, cb.c)) {
                    double delta = *ang - drag.ang0;
                    if (delta > s3d::Math::Pi)      delta -= s3d::Math::TwoPi;
                    if (delta < -s3d::Math::Pi)     delta += s3d::Math::TwoPi;
                    Quat dq = qAxisAngle(drag.axis, delta);
                    cb.q = qNormalize(qMul(dq, drag.q0));
                }
            }
            /* Scale */
            else if (activeHd == Handle::ScaleX || activeHd == Handle::ScaleY || activeHd == Handle::ScaleZ) {
                V3 axLocal = (activeHd == Handle::ScaleX) ? V3{1,0,0}
                                : (activeHd == Handle::ScaleY) ? V3{0,1,0}
                                : V3{0,0,1};
                V3 ax = qRotate(cb.q, axLocal);
                P2 p0 = scr(drag.p0), p1 = scr(drag.p0 + ax);
                Vec2 dir = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).normalized();
                double pix = Dot(d, dir);
                double f = 1.0 + pix * 0.002;
                V3 s = drag.s0;
                if (activeHd == Handle::ScaleX) s.x = Clamp(s.x * f, 0.1, 5.0);
                else if (activeHd == Handle::ScaleY) s.y = Clamp(s.y * f, 0.1, 5.0);
                else s.z = Clamp(s.z * f, 0.1, 5.0);
                cb.s = s;
            } else {
                /* ScaleUniform */
                V3 f = drag.s0 * (1.0 + d.x * 0.002);
                f.x = Clamp(f.x, 0.1, 5.0);
                f.y = Clamp(f.y, 0.1, 5.0);
                f.z = Clamp(f.z, 0.1, 5.0);
                cb.s = f;
            }
        }

        if (!free) {
            handleFPSMovement(dt);
        }

        /*--- キューブ ---*/
		constexpr double DEPTH_EPS = 1e-4;


        std::vector<std::pair<double, int>> drawOrder(cubes.size());
        for (size_t i = 0; i < cubes.size(); ++i) {
            drawOrder[i] = { dot(cubes[i].c - cam, F), static_cast<int>(i) };
        }
        std::sort(drawOrder.begin(), drawOrder.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        struct FaceDrawG { double d; std::array<P2,4> p; ColorF col; bool selected;};
        struct EdgeDrawG { double d; P2 a, b; ColorF col; double th; bool selected;};
		std::vector<FaceDrawG> faces;
		std::vector<EdgeDrawG> edges;

        for (auto [_, idx] : drawOrder) {
			const Cube& cb = cubes[idx];

			// ── 色決定 ───────────────────────
			ColorF col = ColorF{ 1.0, 0.8, 0.3 };
			if (free && idx == hoverIdx) col = ColorF(Palette::Yellow);
			if (idx == sel)              col = ColorF(Palette::Red);
			double th = (idx == sel) ? 3 : 1;

            std::array<V3, 8> vw;
            std::array<P2, 8> vp;
            for (int k = 0; k < 8; ++k) {
                V3 p = LOCAL[k] * cb.s;
                vw[k] = cb.c + qRotate(cb.q, p);
                vp[k] = scr(vw[k]);
            }
			

            for (const auto& f : FACE) {
                V3 v0 = toView(vw[f[0]]);
                V3 v1 = toView(vw[f[1]]);
                V3 v2 = toView(vw[f[2]]);
                V3 n = cross(v1 - v0, v2 - v0);
				if (n.z > EPS) {
					V3 w0 = vw[f[0]];
					V3 w1 = vw[f[1]];
					V3 w2 = vw[f[2]];
					V3 faceNormal = cross(w1 - w0, w2 - w0);
					V3 center = (w0 + w1 + w2) / 3.0;
					V3 toCam = cam - center;
					constexpr double EPS = 1e-5;

					if (dot(faceNormal, toCam) <= EPS) continue; // 背面
				}

                if (std::isinf(vp[f[0]].x) || std::isinf(vp[f[1]].x) ||
                    std::isinf(vp[f[2]].x) || std::isinf(vp[f[3]].x)) continue;
                double depth = (dot(vw[f[0]] - cam, F) + dot(vw[f[1]] - cam, F) +
                                dot(vw[f[2]] - cam, F) + dot(vw[f[3]] - cam, F)) / 4.0;
				bool isSel = (idx == sel);
				faces.push_back({ depth, { vp[f[0]], vp[f[1]], vp[f[2]], vp[f[3]] },
								 col, isSel });
            }

			// ── 辺を push ────────────────────
			for (auto [a, b] : EDGE)
			{
				if (std::isinf(vp[a].x) || std::isinf(vp[b].x)) continue;

				double depthEdge = (dot(vw[a] - cam, F) + dot(vw[b] - cam, F)) * 0.5;
				bool isSel = (idx == sel);
				if (isSel) depthEdge -= DEPTH_EPS;   // ← 符号は faces と同じに

				edges.push_back({ depthEdge, vp[a], vp[b], col, th, isSel });
			}
        }

		constexpr double DEP_TOL = 1e-6;    // 「ほぼ同じ深度」とみなす閾値

		std::sort(faces.begin(), faces.end(),
		  [](auto& a, auto& b) { return a.d < b.d; });
        for (const auto& fc : faces)
            Polygon{ Vec2{fc.p[0].x,fc.p[0].y}, Vec2{fc.p[1].x,fc.p[1].y},
                     Vec2{fc.p[2].x,fc.p[2].y}, Vec2{fc.p[3].x,fc.p[3].y} }.draw(fc.col);


		std::stable_sort(edges.begin(), edges.end(),
			[](const EdgeDrawG& a, const EdgeDrawG& b)
			{
						if (std::abs(a.d - b.d) > DEP_TOL)     // 1. 深度優先
							return a.d > b.d;                  // （奥→手前）

						return !a.selected && b.selected;      // 2. 同深度なら選択を後描き
			});



        /*--- ギズモ ---*/
        if (free && sel != -1) {
            const Cube& cb = cubes[sel];
            const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
            P2 ctr = scr(cb.c); if (std::isinf(ctr.x)) continue;

            /* Move / Scale */
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
                    P2 tip = scr(cb.c + dir * (HALF * 1.5 * sc)); if (std::isinf(tip.x)) continue;
                    bool hot = (hoverHd == a.mv || hoverHd == a.sc
                                 || activeHd == a.mv || activeHd == a.sc);

                    ColorF c = a.col; c.a = hot ? 1.0 : 0.4;
                    Line{ ctr.x,ctr.y, tip.x,tip.y }.draw(hot ? 4 : 3, c);

                    if (mode == Mode::Move) {
                        Vec2 v2 = (Vec2{ tip.x,tip.y } - Vec2{ ctr.x,ctr.y }).normalized() * 8;
                        Vec2 n{ -v2.y,v2.x };
                        Polygon{ Vec2{tip.x,tip.y},
                                 Vec2{tip.x,tip.y} - v2 * 2 + n,
                                 Vec2{tip.x,tip.y} - v2 * 2 - n }.draw(c);
                    } else /* Scale */ {
                        RectF{ tip.x - 4, tip.y - 4, 8,8 }.draw(c);
                    }
                }
                if (mode == Mode::Scale) {
                    bool hot = (hoverHd == Handle::ScaleUniform
                                || activeHd == Handle::ScaleUniform);
                    ColorF cu = hot ? ColorF(Palette::White)
                                    : ColorF{ 1,1,1,0.4 };
                    Circle{ ctr.x,ctr.y,5 }.drawFrame(2, cu);
                }
            }

            /* Rotate */
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
                        P2 s0 = scr(cb.c + p0), s1 = scr(cb.c + p1);
                        if (!std::isinf(s0.x) && !std::isinf(s1.x))
                            Line{ s0.x,s0.y, s1.x,s1.y }.draw(hot ? 3 : 2, col);
                    }
                }
            }
        }



		if (!free)
		{
			constexpr int  SEG = 24;
			constexpr double DEP_TOL = 1e-6;
			constexpr double DEP_EPS = 1e-4;

			double foot = pos.y - EYE;
			double top = foot + CAP_H;

			std::array<V3, SEG> bv, tv;
			std::array<P2, SEG> bp, tp;

			for (int k = 0; k < SEG; ++k)
			{
				double a = Math::TwoPi * k / SEG;
				bv[k] = { pos.x + R * std::cos(a), foot, pos.z + R * std::sin(a) };
				tv[k] = { bv[k].x, top,            bv[k].z };
				bp[k] = scr(bv[k]);
				tp[k] = scr(tv[k]);
			}

			struct QuadDraw { double d; std::vector<P2> p; };
			struct EdgeDraw { double d; P2 a, b; double th; };

			std::vector<QuadDraw> qs;
			std::vector<EdgeDraw> es;

			/* --- 側面 --- */
			for (int k = 0; k < SEG; ++k)
			{
				int k1 = (k + 1) % SEG;
				if (std::isinf(bp[k].x) || std::isinf(bp[k1].x) ||
					std::isinf(tp[k].x) || std::isinf(tp[k1].x)) continue;

				double d = (dot(bv[k] - cam, F) + dot(bv[k1] - cam, F) +
							dot(tv[k1] - cam, F) + dot(tv[k] - cam, F)) * 0.25;
				qs.push_back({ d, { bp[k], bp[k1], tp[k1], tp[k] } });

				/* 縦エッジ */
				double dv = 0.5 * (dot(bv[k] - cam, F) + dot(tv[k] - cam, F));
				es.push_back({ dv, bp[k], tp[k], 2.0 });

				/* 上リング */
				double dt = 0.5 * (dot(tv[k] - cam, F) + dot(tv[k1] - cam, F));
				es.push_back({ dt, tp[k], tp[k1], 2.0 });

				/* 下リング */
				double db = 0.5 * (dot(bv[k] - cam, F) + dot(bv[k1] - cam, F));
				es.push_back({ db, bp[k1], bp[k], 2.0 });     // 時計回りに反転
			}

			/* --- 上面 --- */
			{
				V3 topC{ pos.x, top, pos.z };
				if (dot(V3{ 0,1,0 }, topC - cam) < 0)
				{
					std::vector<P2> poly(SEG);
					double d = 0;
					bool ok = true;
					for (int k = 0; k < SEG; ++k)
					{
						if (std::isinf(tp[k].x)) { ok = false; break; }
						poly[k] = tp[k];
						d += dot(tv[k] - cam, F);
					}
					if (ok) qs.push_back({ d / SEG, poly });
				}
			}

			/* --- 下面 --- */
			{
				V3 botC{ pos.x, foot, pos.z };
				if (dot(V3{ 0,-1,0 }, botC - cam) < 0)
				{
					std::vector<P2> poly(SEG);
					double d = 0;
					bool ok = true;
					for (int k = 0; k < SEG; ++k)
					{
						int kk = SEG - 1 - k;
						if (std::isinf(bp[kk].x)) { ok = false; break; }
						poly[k] = bp[kk];
						d += dot(bv[kk] - cam, F);
					}
					if (ok) qs.push_back({ d / SEG, poly });
				}
			}

			/* --- 描画順ソート --- */
			std::stable_sort(qs.begin(), qs.end(),
				[&](const QuadDraw& a, const QuadDraw& b)
				{
							if (std::abs(a.d - b.d) > DEP_TOL) return a.d < b.d;
							return false;
				});
			std::stable_sort(es.begin(), es.end(),
				[&](const EdgeDraw& a, const EdgeDraw& b)
				{
							if (std::abs(a.d - b.d) > DEP_TOL) return a.d < b.d;
							return false;
				});

			/* --- フィル --- */
			for (const auto& q : qs)
			{
				if (q.p.size() == 4)
				{
					Polygon{ Vec2{ q.p[0].x,q.p[0].y },
							 Vec2{ q.p[1].x,q.p[1].y },
							 Vec2{ q.p[2].x,q.p[2].y },
							 Vec2{ q.p[3].x,q.p[3].y } }
					.draw(ColorF(Palette::Cyan, 0.4));
				}
				else
				{
					Array<Vec2> arr(q.p.size());
					for (size_t i = 0; i < q.p.size(); ++i)
						arr[i] = Vec2{ q.p[i].x, q.p[i].y };
					Polygon{ arr }.draw(ColorF(Palette::Cyan, 0.4));
				}
			}

			/* --- エッジ --- */
			for (const auto& e : es)
				Line{ e.a.x, e.a.y, e.b.x, e.b.y }.draw(e.th, ColorF(Palette::Cyan));
		}
    }
}

