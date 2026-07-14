/**
 * @file GameRenderer.hpp
 * @brief 現在のゲーム状態を画面へ描画するレンダラーを宣言します。
 */
#pragma once
#include "GameState.hpp"

/// 画面への描画だけを担当します。
class GameRenderer {
public:
    void drawFrame(const GameState& state, const FrameContext& ctx, bool free);
};
