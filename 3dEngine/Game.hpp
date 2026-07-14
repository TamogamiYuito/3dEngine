/**
 * @file Game.hpp
 * @brief ゲーム全体の状態と、入力・シミュレーション・描画の各責務をまとめます。
 */
#pragma once
#include "GameInputHandler.hpp"
#include "GameRenderer.hpp"
#include "GameSimulation.hpp"
#include "GameState.hpp"

/// ゲーム状態と入力・シミュレーション・描画を統括します。
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
