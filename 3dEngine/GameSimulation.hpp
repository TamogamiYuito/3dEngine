#pragma once
#include "GameState.hpp"

class GameSimulation {
public:
    void updateSimulation(GameState& state, double dt, bool free);

private:
    void handleFPSMovement(GameState& state, double dt);
};
