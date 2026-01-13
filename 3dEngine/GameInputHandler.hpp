#pragma once
#include "GameState.hpp"

class GameInputHandler {
public:
    void processInput(GameState& state, const FrameContext& ctx, double dt, bool free);

private:
    void updateModeState(GameState& state, bool free);
    void handleCreationUI(GameState& state, const FrameContext& ctx, double dt, bool free);
    void tryCreateCube(GameState& state, const FrameContext& ctx);
    void tryCreateLight(GameState& state, const FrameContext& ctx);
    void tryDeleteSelectedCube(GameState& state);
    void cycleLightSelection(GameState& state);
    void adjustSelectedLightIntensity(GameState& state, double dt);
    void updateHoverState(GameState& state, const FrameContext& ctx, bool free);
    void updateSelectionBox(GameState& state, const FrameContext& ctx, bool free);
    void applySelectionBox(GameState& state, const FrameContext& ctx);
    void updateSelectionAfterErase(GameState& state, int removedIdx);
    void handleSelectionAndDragStart(GameState& state, const FrameContext& ctx, bool free);
    void handleDragEnd(GameState& state);
    void updateDrag(GameState& state, const FrameContext& ctx);
    int detectHoveredCube(GameState& state, const s3d::Vec2& cur, double cx, double cy);
    int detectHoveredLight(GameState& state, const s3d::Vec2& cur, double cx, double cy, bool free);
    GKey findNearestEmpty(GameState& state, GKey start);
};
