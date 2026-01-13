#include "GameSimulation.hpp"
#include <Siv3D.hpp>
#include <algorithm>
#include <cmath>

using namespace s3d;

void GameSimulation::updateSimulation(GameState& state, double dt, bool free) {
    if (!free) {
        handleFPSMovement(state, dt);
    }
}

void GameSimulation::handleFPSMovement(GameState& state, double dt) {
    V3 F  = state.camera.forward();
    V3 Rv = state.camera.right();
    V3 Fh = state.camera.forwardH();
    V3& pos = state.camera.pos;
    V3& cam = state.camera.cam;
    double& vy = state.camera.vy;

    if (KeyW.pressed()) pos = pos - MOVE * dt * Fh;
    if (KeyS.pressed()) pos = pos + MOVE * dt * Fh;
    if (KeyD.pressed()) pos = pos + MOVE * dt * Rv;
    if (KeyA.pressed()) pos = pos - MOVE * dt * Rv;

    bool onGround = false; double foot = pos.y - EYE, head = foot + CAP_H;
    for (const auto& cb : state.cubes) {
        double hx = HALF * cb.s.x;
        double hy = HALF * cb.s.y;
        double hz = HALF * cb.s.z;

        Quat invQ = qConj(cb.q);
        V3 posL  = qRotate(invQ, pos - cb.c);
        V3 footL = qRotate(invQ, V3{ pos.x, foot, pos.z } - cb.c);
        V3 headL = qRotate(invQ, V3{ pos.x, head, pos.z } - cb.c);

        double dynamicEps = Max(2.0, (-vy) * dt + 0.5);
        if (vy <= 0 && footL.y >= hy - dynamicEps && footL.y <= hy + dynamicEps) {
            double dx = std::max(std::abs(footL.x) - hx, 0.0);
            double dz = std::max(std::abs(footL.z) - hz, 0.0);
            if (dx * dx + dz * dz <= R * R) {
                double delta = hy - footL.y;
                V3 adj = delta * qRotate(cb.q, { 0,1,0 });
                pos = pos + adj;
                vy = 0; onGround = true;
                foot += adj.y; head += adj.y;
                posL = qRotate(invQ, pos - cb.c);
                footL = qRotate(invQ, V3{ pos.x, foot, pos.z } - cb.c);
                headL = qRotate(invQ, V3{ pos.x, head, pos.z } - cb.c);
            }
        }

        if (headL.y <= -hy || footL.y >= hy) continue;

        double clx = std::clamp(posL.x, -hx, hx);
        double clz = std::clamp(posL.z, -hz, hz);
        double dx = posL.x - clx, dz = posL.z - clz;
        double d2 = dx * dx + dz * dz;
        if (d2 < R * R - 1e-6) {
            double d = std::sqrt(std::max(d2, 1e-6));
            V3 pushL{ dx,0,dz }; pushL = ((R - d) / d) * pushL;
            V3 pushW = qRotate(cb.q, pushL);
            pos = pos + pushW;
            foot += pushW.y; head += pushW.y;
        }
    }

    if (KeySpace.down() && onGround) vy = JUMP; else vy -= GRAV * dt;
    pos.y += vy * dt; cam = pos + V3{ 0,EYE,0 };
}
