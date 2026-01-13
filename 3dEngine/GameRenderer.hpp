#pragma once
#include "GameState.hpp"

class GameRenderer {
public:
    void drawFrame(const GameState& state, const FrameContext& ctx, bool free);
};
