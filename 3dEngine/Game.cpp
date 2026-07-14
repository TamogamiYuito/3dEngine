/**
 * @file Game.cpp
 * @brief 初期シーンを構築し、入力・シミュレーション・描画を順番に実行するメインループを管理します。
 */
#include "Game.hpp"
#include <Siv3D.hpp>

using namespace s3d;

/// 初期ステージとして5×5個の立方体をグリッド上へ配置します。
Game::Game() {
    for (int z = -2; z <= 2; ++z) {
        for (int x = -2; x <= 2; ++x) {
            state.cubes.push_back({ { gPos(x),0,gPos(z) } });
            state.grid.insert({ x, z });
        }
    }
}
/// 毎フレーム、カメラ更新、入力、シミュレーション、描画の順に処理します。
void Game::run() {
    Scene::SetBackground(ColorF{ 0,0,0 });
    const Vec2 WINF{ Scene::Width() * 0.5, Scene::Height() * 0.5 };
    const Point WINP{ (int32)WINF.x, (int32)WINF.y };

    while (System::Update()) {
        const double dt = Scene::DeltaTime();
        state.camera.update(dt, WINF, WINP);

        FrameContext ctx = buildFrameContext(state, WINF);
        bool free = state.camera.free;

        inputHandler.processInput(state, ctx, dt, free);
        simulation.updateSimulation(state, dt, free);
        renderer.drawFrame(state, ctx, free);
    }
}
