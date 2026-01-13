#pragma once
#include "Camera.hpp"
#include "Cube.hpp"
#include "Gizmo.hpp"
#include "Light.hpp"
#include <vector>
#include <unordered_set>
#include <optional>

class Game {
public:
    Game();
    void run();

private:
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

    std::vector<Cube> cubes;
    std::unordered_set<GKey, GHash> grid;
    Camera camera;
    std::vector<Light> lights;
    int    lightSel = -1;
    Mode   mode = Mode::Move;
    int    sel = -1;
    std::unordered_set<int> selectedCubes;
    std::vector<int> dragCubeIndices;
    std::vector<V3> dragCubePositions;
    std::vector<Quat> dragCubeRotations;
    std::vector<V3> dragCubeScales;
    int    hoverIdx = -1;
    int    hoverLightIdx = -1;
    Handle hoverHd = Handle::None;
    Handle activeHd = Handle::None;
    Drag   drag;
    struct SelectionBox {
        bool on = false;
        s3d::Vec2 start{ 0,0 };
        s3d::Vec2 current{ 0,0 };
    } selectionBox;

    FrameContext buildFrameContext(const s3d::Vec2& windowHalf) const;
    void updateModeState(bool free);
    void handleCreationUI(const FrameContext& ctx, double dt, bool free);
    void tryCreateCube(const FrameContext& ctx);
    void tryCreateLight(const FrameContext& ctx);
    void tryDeleteSelectedCube();
    void cycleLightSelection();
    void adjustSelectedLightIntensity(double dt);
    void updateHoverState(const FrameContext& ctx, bool free);
    void updateSelectionBox(const FrameContext& ctx, bool free);
    void applySelectionBox(const FrameContext& ctx);
    void updateSelectionAfterErase(int removedIdx);
    void handleSelectionAndDragStart(const FrameContext& ctx, bool free);
    void handleDragEnd();
    void updateDrag(const FrameContext& ctx);
    void processInput(const FrameContext& ctx, double dt, bool free);
    void updateSimulation(double dt, bool free);
    void drawFrame(const FrameContext& ctx, bool free);
    int detectHoveredCube(const s3d::Vec2& cur, double cx, double cy);
    int detectHoveredLight(const Vec2& cur, double cx, double cy, bool free);
    void handleFPSMovement(double dt);
    GKey findNearestEmpty(GKey start);
};
