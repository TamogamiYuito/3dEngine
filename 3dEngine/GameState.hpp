#pragma once
#include "Camera.hpp"
#include "Cube.hpp"
#include "Gizmo.hpp"
#include "Light.hpp"
#include <optional>
#include <unordered_set>
#include <vector>

struct FrameContext {
    s3d::Vec2 cursor;
    s3d::Vec2 windowHalf;
    V3 cam;
    V3 right;
    V3 up;
    V3 forward;

    P2 project(V3 w) const;
    std::optional<double> cursorAngle(s3d::Vec2 p, V3 axis, V3 pivot) const;
};

struct SelectionBox {
    bool on = false;
    s3d::Vec2 start{ 0, 0 };
    s3d::Vec2 current{ 0, 0 };
};

struct GameState {
    std::vector<Cube> cubes;
    std::unordered_set<GKey, GHash> grid;
    Camera camera;
    std::vector<Light> lights;
    int lightSel = -1;
    Mode mode = Mode::Move;
    int sel = -1;
    std::unordered_set<int> selectedCubes;
    std::vector<int> dragCubeIndices;
    std::vector<V3> dragCubePositions;
    std::vector<Quat> dragCubeRotations;
    std::vector<V3> dragCubeScales;
    int hoverIdx = -1;
    int hoverLightIdx = -1;
    Handle hoverHd = Handle::None;
    Handle activeHd = Handle::None;
    Drag drag;
    SelectionBox selectionBox;
};

FrameContext buildFrameContext(const GameState& state, const s3d::Vec2& windowHalf);
