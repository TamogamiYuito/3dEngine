#pragma once
#include "GameInputHandler.hpp"
#include "GameRenderer.hpp"
#include "GameSimulation.hpp"
#include "GameState.hpp"

class Game {
public:
    Game();
    void run();

private:
    GameState state;
    GameInputHandler inputHandler;
    GameSimulation simulation;
    GameRenderer renderer;
};
