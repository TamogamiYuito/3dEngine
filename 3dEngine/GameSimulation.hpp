/**
 * @file GameSimulation.hpp
 * @brief ゲーム内の物理・移動シミュレーションを入力や描画から分離して宣言します。
 */
#pragma once
#include "GameState.hpp"

/// プレイ中の移動・物理シミュレーションだけを担当します。
class GameSimulation {
public:
    void updateSimulation(GameState& state, double dt, bool free);

private:
    void handleFPSMovement(GameState& state, double dt);
};
