#include "GameRenderer.hpp"
#include "RenderUtils.hpp"
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

void GameRenderer::drawFrame(const GameState& state, const FrameContext& ctx, bool free) {
    auto scr = [&](V3 w) { return ctx.project(w); };
    const V3 cam = ctx.cam;
    const V3 F = ctx.forward;
    const V3 right = ctx.right;
    const V3 U = ctx.up;
    const V3 pos = state.camera.pos;

    constexpr double DEPTH_EPS = 1e-4;
    constexpr double DEP_TOL = 1e-6;

    std::vector<std::pair<double, int>> drawOrder(state.cubes.size());
    for (size_t i = 0; i < state.cubes.size(); ++i) {
        drawOrder[i] = { dot(state.cubes[i].c - cam, F), static_cast<int>(i) };
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
        const Cube& cb = state.cubes[idx];

        bool isSelected = (state.selectedCubes.count(idx) > 0);
        ColorF col = ColorF{ 1.0, 0.8, 0.3 };
        if (free && idx == state.hoverIdx) col = ColorF(Palette::Yellow);
        if (isSelected)              col = ColorF(Palette::Red);
        double th = isSelected ? 3 : 1;

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

            for (const auto& lt : state.lights) {
                V3 L = -1 * lt.dir();
                double diff = Max(0.0, dot(nW, L));
                shade += lt.intensity * diff;
            }

            shade = Clamp(shade, 0.0, 2.0);
            ColorF shaded{ col.r * shade, col.g * shade, col.b * shade, col.a };
            bool isSel = isSelected;
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
            bool isSel = isSelected;
            if (isSel) depthEdge -= DEPTH_EPS;

            edges.push_back({ depthEdge, vp[a], vp[b], col, th, isSel, idx, static_cast<int>(ei) });
        }
    }

    std::sort(faces.begin(), faces.end(),
        [&](const FaceDrawG& a, const FaceDrawG& b) {
            if (std::abs(a.d - b.d) > DEP_TOL) {
                return a.d > b.d;
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

    for (size_t i = 0; i < state.lights.size(); ++i) {
        const Light& lt = state.lights[i];

        std::array<P2, 8> vp;
        for (int k = 0; k < 8; ++k) {
            V3 p = qRotate(lt.q, LOCAL[k] * lt.s);
            vp[k] = scr(lt.c + p);
        }

        ColorF col = (static_cast<int>(i) == state.lightSel) ? ColorF(Palette::Yellow)
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

    if (free && state.sel != -1) {
        const Cube& cb = state.cubes[state.sel];
        const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
        P2 ctr = scr(cb.c);
        if (std::isinf(ctr.x)) {
            return;
        }

        if (state.mode == Mode::Move || state.mode == Mode::Scale) {
            struct Ax { V3 d; Color col; Handle mv, sc; };
            const Ax AX[3] = {
                { {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
                { {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
                { {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
            };
            for (const auto& a : AX) {
                V3 dir = (state.mode == Mode::Scale) ? qRotate(cb.q, a.d) : a.d;
                double sc = (a.d.x != 0) ? cb.s.x : (a.d.y != 0) ? cb.s.y : cb.s.z;
                P2 tip = scr(cb.c + dir * (HALF * 1.5 * sc));
                if (std::isinf(tip.x)) {
                    continue;
                }
                bool hot = (state.hoverHd == a.mv || state.hoverHd == a.sc ||
                            state.activeHd == a.mv || state.activeHd == a.sc);

                ColorF c = a.col; c.a = hot ? 1.0 : 0.4;
                Line{ ctr.x,ctr.y, tip.x,tip.y }.draw(hot ? 4 : 3, c);

                if (state.mode == Mode::Move) {
                    Vec2 v2 = (Vec2{ tip.x,tip.y } - Vec2{ ctr.x,ctr.y }).normalized() * 8;
                    Vec2 n{ -v2.y,v2.x };
                    Polygon{ Vec2{tip.x,tip.y}, Vec2{tip.x,tip.y} - v2 * 2 + n,
                             Vec2{tip.x,tip.y} - v2 * 2 - n }.draw(c);
                }
                else {
                    RectF{ tip.x - 4, tip.y - 4, 8,8 }.draw(c);
                }
            }
            if (state.mode == Mode::Scale) {
                bool hot = (state.hoverHd == Handle::ScaleUniform || state.activeHd == Handle::ScaleUniform);
                ColorF cu = hot ? ColorF(Palette::White) : ColorF{ 1,1,1,0.4 };
                Circle{ ctr.x,ctr.y,5 }.drawFrame(2, cu);
            }
        }

        if (state.mode == Mode::Rotate) {
            struct Ring { Handle hd; Color col; };
            const Ring RG[3] = {
                { Handle::RotateX, Palette::Red   },
                { Handle::RotateY, Palette::Green },
                { Handle::RotateZ, Palette::Blue  }
            };
            constexpr int SEG = 64;
            for (auto r : RG) {
                bool hot = (state.hoverHd == r.hd || state.activeHd == r.hd);
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
    else if (free && state.lightSel != -1) {
        const Light& cb = state.lights[state.lightSel];
        const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
        P2 ctr = scr(cb.c);
        if (std::isinf(ctr.x)) {
            return;
        }

        if (state.mode == Mode::Move || state.mode == Mode::Scale) {
            struct Ax { V3 d; Color col; Handle mv, sc; };
            const Ax AX[3] = {
                { {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
                { {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
                { {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
            };
            for (const auto& a : AX) {
                V3 dir = (state.mode == Mode::Scale) ? qRotate(cb.q, a.d) : a.d;
                double sc = (a.d.x != 0) ? cb.s.x : (a.d.y != 0) ? cb.s.y : cb.s.z;
                P2 tip = scr(cb.c + dir * (HALF * 1.5 * sc));
                if (std::isinf(tip.x)) {
                    continue;
                }
                bool hot = (state.hoverHd == a.mv || state.hoverHd == a.sc ||
                            state.activeHd == a.mv || state.activeHd == a.sc);

                ColorF c = a.col; c.a = hot ? 1.0 : 0.4;
                Line{ ctr.x,ctr.y, tip.x,tip.y }.draw(hot ? 4 : 3, c);

                if (state.mode == Mode::Move) {
                    Vec2 v2 = (Vec2{ tip.x,tip.y } - Vec2{ ctr.x,ctr.y }).normalized() * 8;
                    Vec2 n{ -v2.y,v2.x };
                    Polygon{ Vec2{tip.x,tip.y}, Vec2{tip.x,tip.y} - v2 * 2 + n,
                             Vec2{tip.x,tip.y} - v2 * 2 - n }.draw(c);
                }
                else {
                    RectF{ tip.x - 4, tip.y - 4, 8,8 }.draw(c);
                }
            }
            if (state.mode == Mode::Scale) {
                bool hot = (state.hoverHd == Handle::ScaleUniform || state.activeHd == Handle::ScaleUniform);
                ColorF cu = hot ? ColorF(Palette::White) : ColorF{ 1,1,1,0.4 };
                Circle{ ctr.x,ctr.y,5 }.drawFrame(2, cu);
            }
        }

        if (state.mode == Mode::Rotate) {
            struct Ring { Handle hd; Color col; };
            const Ring RG[3] = {
                { Handle::RotateX, Palette::Red   },
                { Handle::RotateY, Palette::Green },
                { Handle::RotateZ, Palette::Blue  }
            };
            constexpr int SEG = 64;
            for (auto r : RG) {
                bool hot = (state.hoverHd == r.hd || state.activeHd == r.hd);
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

    if (free && state.selectionBox.on) {
        Vec2 minPos{ std::min(state.selectionBox.start.x, state.selectionBox.current.x),
                     std::min(state.selectionBox.start.y, state.selectionBox.current.y) };
        Vec2 size{ std::abs(state.selectionBox.start.x - state.selectionBox.current.x),
                   std::abs(state.selectionBox.start.y - state.selectionBox.current.y) };
        if (size.x > 1.0 || size.y > 1.0) {
            RectF{ minPos, size }.drawFrame(2.0, ColorF(Palette::Skyblue, 0.8));
            RectF{ minPos, size }.draw(ColorF(Palette::Skyblue, 0.15));
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
