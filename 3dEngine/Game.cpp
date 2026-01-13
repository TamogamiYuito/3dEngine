#include "Game.hpp"
#include <Siv3D.hpp>

using namespace s3d;

Game::Game() {
    for (int z = -2; z <= 2; ++z) {
        for (int x = -2; x <= 2; ++x) {
            state.cubes.push_back({ { gPos(x),0,gPos(z) } });
            state.grid.insert({ x, z });
        }
    }
}
void Game::run() {
    const Vec2 WINF{ Scene::Width() * 0.5, Scene::Height() * 0.5 };
    const Point WINP{ (int32)WINF.x, (int32)WINF.y };

    while (System::Update()) {
        Scene::SetBackground(state.backgroundColor);
        const double dt = Scene::DeltaTime();
        state.camera.update(dt, WINF, WINP);

        FrameContext ctx = buildFrameContext(state, WINF);
        bool free = state.camera.free;

        inputHandler.processInput(state, ctx, dt, free);
        simulation.updateSimulation(state, dt, free);
        renderer.drawFrame(state, ctx, free);
    }
}
