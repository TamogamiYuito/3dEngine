#include "GameInputHandler.hpp"
#include "RenderUtils.hpp"
#include <Siv3D.hpp>
#include <algorithm>

using namespace s3d;

void GameInputHandler::processInput(GameState& state, const FrameContext& ctx, double dt, bool free) {
    updateModeState(state, free);
    handleCreationUI(state, ctx, dt, free);
    handleAiPromptUI(state, ctx);
    updateHoverState(state, ctx, free);
    updateSelectionBox(state, ctx, free);
    handleSelectionAndDragStart(state, ctx, free);
    handleDragEnd(state);
    updateDrag(state, ctx);
}

void GameInputHandler::updateModeState(GameState& state, bool free) {
    if (free && !state.drag.on) {
        if (KeyW.down()) state.mode = Mode::Move;
        if (KeyE.down()) state.mode = Mode::Rotate;
        if (KeyR.down()) state.mode = Mode::Scale;
    }
}

void GameInputHandler::handleCreationUI(GameState& state, const FrameContext& ctx, double dt, bool free) {
    if (!free) {
        return;
    }

    tryCreateCube(state, ctx);
    tryCreateLight(state, ctx);
    tryDeleteSelectedCube(state);
    cycleLightSelection(state);
    adjustSelectedLightIntensity(state, dt);
}

void GameInputHandler::handleAiPromptUI(GameState& state, const FrameContext& ctx) {
    constexpr Vec2 panelPos{ 20, 150 };
    constexpr Vec2 panelSize{ 360, 130 };
    const RectF panel{ panelPos, panelSize };

    panel.draw(ColorF{ 0.05, 0.05, 0.08, 0.75 });
    panel.drawFrame(1.0, ColorF{ 0.3, 0.6, 0.9, 0.8 });

    static const Font font{ 14 };
    font(U"AI 検索ボックス").draw(panelPos + Vec2{ 12, 8 }, Palette::White);

    RectF inputBox{ panelPos.x + 12, panelPos.y + 32, panelSize.x - 24, 28 };
    if (inputBox.leftClicked()) {
        state.aiPromptState.active = true;
    }
    if (MouseL.down() && !inputBox.mouseOver()) {
        state.aiPromptState.active = false;
    }

    TextInput::Update(state.aiPrompt, state.aiPromptState);

    ColorF border = state.aiPromptState.active ? ColorF{ 0.3, 0.7, 1.0 } : ColorF{ 0.4, 0.4, 0.4 };
    inputBox.draw(ColorF{ 0.1, 0.1, 0.12, 0.9 });
    inputBox.drawFrame(1.0, border);
    font(state.aiPrompt).draw(inputBox.pos + Vec2{ 6, 4 }, Palette::White);

    bool submit = SimpleGUI::Button(U"実行", Vec2{ panelPos.x + panelSize.x - 72, panelPos.y + 70 }, 60);
    if (state.aiPromptState.active && KeyEnter.down()) {
        submit = true;
    }
    if (submit && !state.aiPrompt.isEmpty()) {
        applyAiPrompt(state, ctx, state.aiPrompt);
        state.aiPrompt.clear();
        state.aiPromptState = TextEditState{};
    }

    if (!state.aiResponse.isEmpty()) {
        font(state.aiResponse).draw(panelPos + Vec2{ 12, 100 }, Palette::Skyblue);
    }
}

void GameInputHandler::applyAiPrompt(GameState& state, const FrameContext& ctx, const String& prompt) {
    auto has = [&](const String& key) { return prompt.includes(key); };

    if (has(U"背景") || has(U"background") || has(U"Background") || has(U"BACKGROUND")) {
        if (has(U"青") || has(U"blue") || has(U"Blue") || has(U"BLUE")) {
            state.backgroundColor = ColorF{ 0.05, 0.12, 0.25 };
            state.aiResponse = U"背景色を青に変更しました。";
            return;
        }
        if (has(U"赤") || has(U"red") || has(U"Red") || has(U"RED")) {
            state.backgroundColor = ColorF{ 0.25, 0.08, 0.08 };
            state.aiResponse = U"背景色を赤に変更しました。";
            return;
        }
        if (has(U"緑") || has(U"green") || has(U"Green") || has(U"GREEN")) {
            state.backgroundColor = ColorF{ 0.08, 0.2, 0.12 };
            state.aiResponse = U"背景色を緑に変更しました。";
            return;
        }
        if (has(U"白") || has(U"white") || has(U"White") || has(U"WHITE")) {
            state.backgroundColor = ColorF{ 0.9, 0.9, 0.9 };
            state.aiResponse = U"背景色を白に変更しました。";
            return;
        }
        if (has(U"黒") || has(U"black") || has(U"Black") || has(U"BLACK")) {
            state.backgroundColor = ColorF{ 0.0, 0.0, 0.0 };
            state.aiResponse = U"背景色を黒に変更しました。";
            return;
        }
    }

    if (has(U"cube") || has(U"Cube") || has(U"CUBE") || has(U"キューブ") || has(U"立方体")) {
        spawnCubeNearCamera(state, ctx);
        state.aiResponse = U"キューブを追加しました。";
        return;
    }

    if (has(U"light") || has(U"Light") || has(U"LIGHT") || has(U"ライト")) {
        spawnLightNearCamera(state, ctx);
        state.aiResponse = U"ライトを追加しました。";
        return;
    }

    if (has(U"カメラ") && (has(U"リセット") || has(U"reset") || has(U"Reset") || has(U"RESET"))) {
        state.camera = Camera{};
        state.aiResponse = U"カメラを初期位置に戻しました。";
        return;
    }

    state.aiResponse = U"対応コマンド: 背景(青/赤/緑/白/黒), キューブ追加, ライト追加, カメラリセット";
}

void GameInputHandler::spawnCubeNearCamera(GameState& state, const FrameContext& ctx) {
    V3 Fh = state.camera.forwardH();
    V3 Rv = state.camera.right();
    double ang = Random(-Math::Pi / 4.0, Math::Pi / 4.0);
    V3 dir = norm(std::cos(ang) * (-1 * Fh) + std::sin(ang) * Rv);
    double dist = Random(2.0, 4.0) * (2.0 * HALF);
    double yRand = Random(0, 2) * (2.0 * HALF);
    V3 hit = V3{ ctx.cam.x, yRand, ctx.cam.z } + dist * dir;
    GKey g{ gIdx(hit.x), gIdx(hit.z) };
    GKey place = findNearestEmpty(state, g);
    if (state.grid.find(place) == state.grid.end()) {
        state.cubes.push_back({ { gPos(place.gx), yRand, gPos(place.gz) } });
        state.grid.insert(place);
    }
}

void GameInputHandler::spawnLightNearCamera(GameState& state, const FrameContext& ctx) {
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
    state.lights.push_back(lt);
    state.lightSel = static_cast<int>(state.lights.size()) - 1;
}

void GameInputHandler::tryCreateCube(GameState& state, const FrameContext& ctx) {
    if (!SimpleGUI::Button(U"+ Cube", { 20,20 })) {
        return;
    }

    V3 Fh = state.camera.forwardH();
    V3 Rv = state.camera.right();
    double ang = Random(-Math::Pi / 4.0, Math::Pi / 4.0);
    V3 dir = norm(std::cos(ang) * (-1 * Fh) + std::sin(ang) * Rv);
    double dist = Random(2.0, 4.0) * (2.0 * HALF);
    double yRand = Random(0, 2) * (2.0 * HALF);
    V3 hit = V3{ ctx.cam.x, yRand, ctx.cam.z } + dist * dir;
    GKey g{ gIdx(hit.x), gIdx(hit.z) };
    GKey place = findNearestEmpty(state, g);
    if (state.grid.find(place) == state.grid.end()) {
        state.cubes.push_back({ { gPos(place.gx), yRand, gPos(place.gz) } });
        state.grid.insert(place);
    }
}

void GameInputHandler::tryCreateLight(GameState& state, const FrameContext& ctx) {
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
    state.lights.push_back(lt);
    state.lightSel = static_cast<int>(state.lights.size()) - 1;
}

void GameInputHandler::tryDeleteSelectedCube(GameState& state) {
    if (state.sel == -1 || state.drag.on) {
        return;
    }

    if (!KeyDelete.down()) {
        return;
    }

    std::vector<int> deleteIndices;
    if (state.selectedCubes.empty()) {
        deleteIndices.push_back(state.sel);
    } else {
        deleteIndices.reserve(state.selectedCubes.size());
        for (int idx : state.selectedCubes) {
            deleteIndices.push_back(idx);
        }
    }

    std::sort(deleteIndices.begin(), deleteIndices.end(), std::greater<int>());
    for (int idx : deleteIndices) {
        const Cube& cb = state.cubes[idx];
        state.grid.erase({ gIdx(cb.c.x), gIdx(cb.c.z) });
        state.cubes.erase(state.cubes.begin() + idx);
        updateSelectionAfterErase(state, idx);
    }
    state.hoverIdx = -1;
}

void GameInputHandler::cycleLightSelection(GameState& state) {
    if (KeyTab.down() && !state.lights.empty()) {
        state.lightSel = (state.lightSel + 1) % static_cast<int>(state.lights.size());
    }
}

void GameInputHandler::adjustSelectedLightIntensity(GameState& state, double dt) {
    if (state.lightSel == -1) {
        return;
    }

    if (KeyZ.pressed())
        state.lights[state.lightSel].intensity = std::max(0.0, state.lights[state.lightSel].intensity - dt);
    if (KeyX.pressed())
        state.lights[state.lightSel].intensity += dt;
    SimpleGUI::Slider(U"Intensity", state.lights[state.lightSel].intensity, 0.0, 5.0, Vec2{ 20,110 });
}

void GameInputHandler::updateHoverState(GameState& state, const FrameContext& ctx, bool free) {
    state.hoverHd = Handle::None;
    state.hoverIdx = detectHoveredCube(state, ctx.cursor, ctx.windowHalf.x, ctx.windowHalf.y);
    state.hoverLightIdx = detectHoveredLight(state, ctx.cursor, ctx.windowHalf.x, ctx.windowHalf.y, free);

    if (free && state.sel != -1 && !KeyAlt.pressed()) {
        const Cube& cb = state.cubes[state.sel];
        const double L = HALF * std::max({ cb.s.x, cb.s.y, cb.s.z }) * 1.5;
        P2 ctr = ctx.project(cb.c);
        if (!std::isinf(ctr.x) && (state.mode == Mode::Move || state.mode == Mode::Scale)) {
            struct Ax { V3 d; Color col; Handle mv, sc; };
            const Ax AX[3] = {
                { {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
                { {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
                { {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
            };
            double bestA = 1e9;
            for (const auto& a : AX) {
                V3 dir = (state.mode == Mode::Scale) ? qRotate(cb.q, a.d) : a.d;
                double sc = (a.d.x != 0) ? cb.s.x : (a.d.y != 0) ? cb.s.y : cb.s.z;
                P2 tip = ctx.project(cb.c + dir * (HALF * 1.5 * sc));
                if (std::isinf(tip.x)) continue;
                double dLine = segDist2(ctx.cursor, { ctr.x,ctr.y }, { tip.x,tip.y });
                double dTip = (ctx.cursor - Vec2{ tip.x,tip.y }).lengthSq();
                if (state.mode == Mode::Move && dLine < 64 && dLine < bestA) {
                    bestA = dLine; state.hoverHd = a.mv;
                }
                else if (state.mode == Mode::Scale) {
                    if (dTip < 64 && dTip < bestA) {
                        bestA = dTip; state.hoverHd = a.sc;
                    }
                    else if (dLine < 64 && dLine < bestA) {
                        bestA = dLine; state.hoverHd = a.sc;
                    }
                }
            }
            if (state.mode == Mode::Scale && (ctx.cursor - Vec2{ ctr.x,ctr.y }).lengthSq() < 64)
                state.hoverHd = Handle::ScaleUniform;
        }
        if (state.mode == Mode::Rotate && !std::isinf(ctr.x)) {
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
                if (bestR < 64) { state.hoverHd = r.hd; break; }
            }
        }
    }
}

void GameInputHandler::handleSelectionAndDragStart(GameState& state, const FrameContext& ctx, bool free) {
    if (!(free && MouseL.down())) {
        return;
    }

    auto scr = [&](V3 w) { return ctx.project(w); };

    if (state.hoverHd != Handle::None && (state.sel != -1 || state.lightSel != -1)) {
        state.activeHd = state.hoverHd;
        state.drag.on = true;
        state.drag.cur0 = ctx.cursor;

        if (state.sel != -1) {
            Cube& cb = state.cubes[state.sel];
            state.drag.p0 = cb.c;
            state.drag.q0 = cb.q;
            state.drag.s0 = cb.s;
            state.dragCubeIndices.clear();
            state.dragCubePositions.clear();
            state.dragCubeRotations.clear();
            state.dragCubeScales.clear();
            if (state.selectedCubes.empty()) {
                state.dragCubeIndices.push_back(state.sel);
                state.dragCubePositions.push_back(cb.c);
                state.dragCubeRotations.push_back(cb.q);
                state.dragCubeScales.push_back(cb.s);
            } else {
                state.dragCubeIndices.reserve(state.selectedCubes.size());
                state.dragCubePositions.reserve(state.selectedCubes.size());
                state.dragCubeRotations.reserve(state.selectedCubes.size());
                state.dragCubeScales.reserve(state.selectedCubes.size());
                for (int idx : state.selectedCubes) {
                    const Cube& selected = state.cubes[idx];
                    state.dragCubeIndices.push_back(idx);
                    state.dragCubePositions.push_back(selected.c);
                    state.dragCubeRotations.push_back(selected.q);
                    state.dragCubeScales.push_back(selected.s);
                }
            }
            if (state.activeHd == Handle::MoveX || state.activeHd == Handle::MoveZ)
                for (const V3& pos : state.dragCubePositions)
                    state.grid.erase({ gIdx(pos.x), gIdx(pos.z) });
        }
        else if (state.lightSel != -1) {
            Light& lt = state.lights[state.lightSel];
            state.drag.p0 = lt.c;
            state.drag.q0 = lt.q;
            state.drag.s0 = lt.s;
        }

        if (state.activeHd == Handle::MoveX || state.activeHd == Handle::MoveY || state.activeHd == Handle::MoveZ ||
            state.activeHd == Handle::ScaleX || state.activeHd == Handle::ScaleY || state.activeHd == Handle::ScaleZ) {

            V3 axLocal = (state.activeHd == Handle::MoveX || state.activeHd == Handle::ScaleX) ? V3{ 1,0,0 } :
                    (state.activeHd == Handle::MoveY || state.activeHd == Handle::ScaleY) ? V3{ 0,1,0 } :
                    V3{ 0,0,1 };
            V3 ax = (state.activeHd == Handle::ScaleX || state.activeHd == Handle::ScaleY || state.activeHd == Handle::ScaleZ)
                    ? qRotate(state.drag.q0, axLocal) : axLocal;

            P2 p0 = scr(state.drag.p0), p1 = scr(state.drag.p0 + ax);
            state.drag.lenPx = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).length();
            if (state.drag.lenPx < 1) state.drag.lenPx = 1;
        }

        if (state.activeHd == Handle::RotateX || state.activeHd == Handle::RotateY || state.activeHd == Handle::RotateZ) {
            V3 axLocal = (state.activeHd == Handle::RotateX) ? V3{ 1,0,0 } :
                    (state.activeHd == Handle::RotateY) ? V3{ 0,1,0 } : V3{ 0,0,1 };
            state.drag.axis = qRotate(state.drag.q0, axLocal);
            state.drag.ang0 = ctx.cursorAngle(ctx.cursor, state.drag.axis, state.drag.p0).value_or(0.0);
        }
    }
    else {
        if (state.hoverIdx != -1 || state.hoverLightIdx != -1) {
            const double dc = (state.hoverIdx != -1) ? len(state.cubes[state.hoverIdx].c - ctx.cam) : 1e9;
            const double dl = (state.hoverLightIdx != -1) ? len(state.lights[state.hoverLightIdx].c - ctx.cam) : 1e9;
            const bool pickLight = (dl < dc);

            if (pickLight) {
                state.lightSel = state.hoverLightIdx;
                state.sel = -1;
                state.selectedCubes.clear();
            } else {
                state.sel = state.hoverIdx;
                state.lightSel = -1;
                state.selectedCubes = { state.sel };
            }
        }
    }
}

void GameInputHandler::handleDragEnd(GameState& state) {
    if (MouseL.up()) {
        if (state.drag.on && state.sel != -1 && (state.activeHd == Handle::MoveX || state.activeHd == Handle::MoveZ)) {
            for (int idx : state.dragCubeIndices) {
                Cube& cb = state.cubes[idx];
                state.grid.insert({ gIdx(cb.c.x), gIdx(cb.c.z) });
            }
        }
        state.drag.on = false; state.activeHd = Handle::None;
        state.dragCubeIndices.clear();
        state.dragCubePositions.clear();
        state.dragCubeRotations.clear();
        state.dragCubeScales.clear();
    }
}

void GameInputHandler::updateDrag(GameState& state, const FrameContext& ctx) {
    if (!(state.drag.on && (state.sel != -1 || state.lightSel != -1))) {
        return;
    }

    Vec2 d = ctx.cursor - state.drag.cur0;

    bool isCube = (state.sel != -1);
    auto& objPos = isCube ? state.cubes[state.sel].c : state.lights[state.lightSel].c;
    auto& objRot = isCube ? state.cubes[state.sel].q : state.lights[state.lightSel].q;
    auto& objScl = isCube ? state.cubes[state.sel].s : state.lights[state.lightSel].s;

    if (state.activeHd == Handle::MoveX || state.activeHd == Handle::MoveY || state.activeHd == Handle::MoveZ) {
        V3 axLocal = (state.activeHd == Handle::MoveX || state.activeHd == Handle::ScaleX) ? V3{ 1,0,0 }
                : (state.activeHd == Handle::MoveY || state.activeHd == Handle::ScaleY) ? V3{ 0,1,0 }
                : V3{ 0,0,1 };

        V3 ax = (isCube || state.activeHd == Handle::MoveX || state.activeHd == Handle::MoveY || state.activeHd == Handle::MoveZ)
                ? axLocal
                : qRotate(state.drag.q0, axLocal);
        P2 p0 = ctx.project(state.drag.p0), p1 = ctx.project(state.drag.p0 + ax);
        Vec2 dir = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).normalized();
        double pix = Dot(d, dir);
        double world = (pix / state.drag.lenPx) * 40.0;
        V3 delta = ax * world;
        if (isCube) {
            for (size_t i = 0; i < state.dragCubeIndices.size(); ++i) {
                state.cubes[state.dragCubeIndices[i]].c = state.dragCubePositions[i] + delta;
            }
        } else {
            objPos = state.drag.p0 + delta;
        }
    }

    else if (state.activeHd == Handle::RotateX || state.activeHd == Handle::RotateY || state.activeHd == Handle::RotateZ) {
        if (auto ang = ctx.cursorAngle(ctx.cursor, state.drag.axis, state.drag.p0)) {
            double delta = *ang - state.drag.ang0;
            if (delta > s3d::Math::Pi)      delta -= s3d::Math::TwoPi;
            if (delta < -s3d::Math::Pi)     delta += s3d::Math::TwoPi;
            Quat dq = qAxisAngle(state.drag.axis, delta);
            if (isCube) {
                for (size_t i = 0; i < state.dragCubeIndices.size(); ++i) {
                    state.cubes[state.dragCubeIndices[i]].q = qNormalize(qMul(dq, state.dragCubeRotations[i]));
                }
            } else {
                objRot = qNormalize(qMul(dq, state.drag.q0));
            }
        }
    }

    else if (state.activeHd == Handle::ScaleX || state.activeHd == Handle::ScaleY || state.activeHd == Handle::ScaleZ) {
        V3 axLocal = (state.activeHd == Handle::ScaleX) ? V3{ 1,0,0 }
                : (state.activeHd == Handle::ScaleY) ? V3{ 0,1,0 }
                : V3{ 0,0,1 };
        V3 ax = qRotate(state.drag.q0, axLocal);
        P2 p0 = ctx.project(state.drag.p0), p1 = ctx.project(state.drag.p0 + ax);
        Vec2 dir = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).normalized();
        double pix = Dot(d, dir);
        double f = 1.0 + pix * 0.002;
        if (isCube) {
            for (size_t i = 0; i < state.dragCubeIndices.size(); ++i) {
                V3 s = state.dragCubeScales[i];
                if (state.activeHd == Handle::ScaleX) s.x = Clamp(s.x * f, 0.1, 5.0);
                else if (state.activeHd == Handle::ScaleY) s.y = Clamp(s.y * f, 0.1, 5.0);
                else s.z = Clamp(s.z * f, 0.1, 5.0);
                state.cubes[state.dragCubeIndices[i]].s = s;
            }
        } else {
            V3 s = state.drag.s0;
            if (state.activeHd == Handle::ScaleX) s.x = Clamp(s.x * f, 0.1, 5.0);
            else if (state.activeHd == Handle::ScaleY) s.y = Clamp(s.y * f, 0.1, 5.0);
            else s.z = Clamp(s.z * f, 0.1, 5.0);
            objScl = s;
        }
    }

    else {
        double scaleFactor = 1.0 + d.x * 0.002;
        if (isCube) {
            for (size_t i = 0; i < state.dragCubeIndices.size(); ++i) {
                V3 f = state.dragCubeScales[i] * scaleFactor;
                f.x = Clamp(f.x, 0.1, 5.0);
                f.y = Clamp(f.y, 0.1, 5.0);
                f.z = Clamp(f.z, 0.1, 5.0);
                state.cubes[state.dragCubeIndices[i]].s = f;
            }
        } else {
            V3 f = state.drag.s0 * scaleFactor;
            f.x = Clamp(f.x, 0.1, 5.0);
            f.y = Clamp(f.y, 0.1, 5.0);
            f.z = Clamp(f.z, 0.1, 5.0);
            objScl = f;
        }
    }
}

void GameInputHandler::updateSelectionBox(GameState& state, const FrameContext& ctx, bool free) {
    if (!free || state.drag.on || state.activeHd != Handle::None) {
        return;
    }

    if (MouseL.down()) {
        if (state.hoverHd == Handle::None && state.hoverIdx == -1 && state.hoverLightIdx == -1) {
            state.selectionBox.on = true;
            state.selectionBox.start = ctx.cursor;
            state.selectionBox.current = ctx.cursor;
        } else {
            state.selectionBox.on = false;
        }
    }

    if (state.selectionBox.on && MouseL.pressed()) {
        state.selectionBox.current = ctx.cursor;
    }

    if (state.selectionBox.on && MouseL.up()) {
        applySelectionBox(state, ctx);
        state.selectionBox.on = false;
    }
}

void GameInputHandler::applySelectionBox(GameState& state, const FrameContext& ctx) {
    Vec2 minPos{ std::min(state.selectionBox.start.x, state.selectionBox.current.x),
                 std::min(state.selectionBox.start.y, state.selectionBox.current.y) };
    Vec2 size{ std::abs(state.selectionBox.start.x - state.selectionBox.current.x),
               std::abs(state.selectionBox.start.y - state.selectionBox.current.y) };

    constexpr double MIN_BOX = 4.0;
    if (size.x < MIN_BOX && size.y < MIN_BOX) {
        return;
    }

    RectF box{ minPos, size };
    std::unordered_set<int> nextSelection;
    for (size_t i = 0; i < state.cubes.size(); ++i) {
        P2 proj = ctx.project(state.cubes[i].c);
        if (std::isinf(proj.x) || std::isinf(proj.y)) {
            continue;
        }
        if (box.contains(Vec2{ proj.x, proj.y })) {
            nextSelection.insert(static_cast<int>(i));
        }
    }

    state.selectedCubes = std::move(nextSelection);
    if (!state.selectedCubes.empty()) {
        state.sel = *std::min_element(state.selectedCubes.begin(), state.selectedCubes.end());
        state.lightSel = -1;
    } else {
        state.sel = -1;
    }
}

void GameInputHandler::updateSelectionAfterErase(GameState& state, int removedIdx) {
    std::unordered_set<int> nextSelection;
    for (int idx : state.selectedCubes) {
        if (idx == removedIdx) {
            continue;
        }
        if (idx > removedIdx) {
            nextSelection.insert(idx - 1);
        } else {
            nextSelection.insert(idx);
        }
    }
    state.selectedCubes = std::move(nextSelection);
    if (state.sel == removedIdx) {
        state.sel = state.selectedCubes.empty() ? -1 : *std::min_element(state.selectedCubes.begin(), state.selectedCubes.end());
    } else if (state.sel > removedIdx) {
        state.sel -= 1;
    }
}

int GameInputHandler::detectHoveredCube(GameState& state, const Vec2& cur, double cx, double cy) {
    V3 cam = state.camera.cam;
    V3 Rv = state.camera.right();
    V3 U  = state.camera.up();
    V3 F  = state.camera.forward();

    int hover = -1; double bestDepth = 1e9;
    for (size_t i = 0; i < state.cubes.size(); ++i) {
        double depth = 1e9;
        if (state.cubes[i].checkHovered(cur, cx, cy, cam, Rv, U, F, depth)) {
            if (depth < bestDepth) {
                bestDepth = depth;
                hover = static_cast<int>(i);
            }
        }
    }
    return hover;
}

int GameInputHandler::detectHoveredLight(GameState& state, const Vec2& cur, double cx, double cy, bool free) {
    V3 cam = state.camera.cam;
    V3 Rv = state.camera.right();
    V3 U = state.camera.up();
    V3 F = state.camera.forward();
    auto scr = [&](V3 w) { return screenProject(w, cam, Rv, U, F, cx, cy); };

    int hover = -1; double bestDepth = 1e9;
    for (size_t i = 0; i < state.lights.size(); ++i) {
        double depth = 1e9;
        if (state.lights[i].checkHovered(cur, cx, cy, cam, Rv, U, F, depth)) {
            if (depth < bestDepth) {
                bestDepth = depth;
                hover = static_cast<int>(i);
            }
        }
    }

    if (free && state.lightSel != -1 && !KeyAlt.pressed()) {
        const Light& lt = state.lights[state.lightSel];
        const double L = HALF * std::max({ lt.s.x, lt.s.y, lt.s.z }) * 1.5;
        P2 ctr = scr(lt.c);
        if (!std::isinf(ctr.x) && (state.mode == Mode::Move || state.mode == Mode::Scale)) {
            struct Ax { V3 d; Color col; Handle mv, sc; };
            const Ax AX[3] = {
                { {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
                { {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
                { {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
            };
            double bestA = 1e9;
            for (const auto& a : AX) {
                V3 dir = (state.mode == Mode::Scale) ? qRotate(lt.q, a.d) : a.d;
                double sc = (a.d.x != 0) ? lt.s.x : (a.d.y != 0) ? lt.s.y : lt.s.z;
                P2 tip = scr(lt.c + dir * (HALF * 1.5 * sc));
                if (std::isinf(tip.x)) continue;
                double dLine = segDist2(cur, { ctr.x,ctr.y }, { tip.x,tip.y });
                double dTip = (cur - Vec2{ tip.x,tip.y }).lengthSq();
                if (state.mode == Mode::Move && dLine < 64 && dLine < bestA) {
                    bestA = dLine; state.hoverHd = a.mv;
                }
                else if (state.mode == Mode::Scale) {
                    if (dTip < 64 && dTip < bestA) {
                        bestA = dTip; state.hoverHd = a.sc;
                    }
                    else if (dLine < 64 && dLine < bestA) {
                        bestA = dLine; state.hoverHd = a.sc;
                    }
                }
            }
            if (state.mode == Mode::Scale && (cur - Vec2{ ctr.x,ctr.y }).lengthSq() < 64)
                state.hoverHd = Handle::ScaleUniform;
        }
        if (state.mode == Mode::Rotate && !std::isinf(ctr.x)) {
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
                if (bestR < 64) { state.hoverHd = r.hd; break; }
            }
        }
    }
    return hover;
}

GKey GameInputHandler::findNearestEmpty(GameState& state, GKey start) {
    if (state.grid.find(start) == state.grid.end()) return start;
    constexpr int MAX_RADIUS = 20;
    for (int r = 1; r <= MAX_RADIUS; ++r) {
        for (int dz = -r; dz <= r; ++dz) {
            int dx = r - std::abs(dz);
            GKey g1{ start.gx + dx, start.gz + dz };
            if (state.grid.find(g1) == state.grid.end()) return g1;
            if (dx != 0) {
                GKey g2{ start.gx - dx, start.gz + dz };
                if (state.grid.find(g2) == state.grid.end()) return g2;
            }
        }
    }
    return start;
}
