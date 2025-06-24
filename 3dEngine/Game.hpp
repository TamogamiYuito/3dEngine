#pragma once
#include "Camera.hpp"
#include "Cube.hpp"
#include "Gizmo.hpp"
#include "Light.hpp"
#include <vector>
#include <unordered_set>

class Game {
public:
    Game();
    void run();

private:
    std::vector<Cube> cubes;
    std::unordered_set<GKey, GHash> grid;
    Camera camera;
    std::vector<Light> lights;
    int    lightSel = -1;
    Mode   mode = Mode::Move;
    int    sel = -1;
    int    hoverIdx = -1;
    Handle hoverHd = Handle::None;
    Handle activeHd = Handle::None;
    Drag   drag;

    int detectHoveredCube(const s3d::Vec2& cur, double cx, double cy);
    void handleFPSMovement(double dt);
    GKey findNearestEmpty(GKey start);
};

