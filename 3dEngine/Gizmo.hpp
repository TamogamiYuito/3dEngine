/**
 * @file Gizmo.hpp
 * @brief 移動・回転・拡縮モード、各操作ハンドル、ドラッグ開始時の状態を定義します。
 */
#pragma once
#include "Math.hpp"

/// 現在のギズモ操作モードを表します。
enum class Mode {
    Move,
    Rotate,
    Scale
};

/// 選択中または操作中のギズモ軸を表します。
enum class Handle {
    None,
    MoveX,
    MoveY,
    MoveZ,
    RotateX,
    RotateY,
    RotateZ,
    ScaleX,
    ScaleY,
    ScaleZ,
    ScaleUniform
};

/// ドラッグ開始時の位置・回転・拡縮を保存します。
struct Drag {
    bool on = false;
    s3d::Vec2 cur0;
    V3 p0;
    Quat q0{1,0,0,0};
    V3 s0{ 1,1,1 };
    double lenPx = 1;
    V3 axis{};
    double ang0{};
};

double segDist2(s3d::Vec2 p, s3d::Vec2 a, s3d::Vec2 b);
