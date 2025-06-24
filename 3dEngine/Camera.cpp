#include "Camera.hpp"
#include <Siv3D.hpp>

V3 Camera::forward() const {
    return norm({ std::cos(pitch) * std::sin(yaw),
                  std::sin(pitch),
                 -std::cos(pitch) * std::cos(yaw) });
}

V3 Camera::right() const {
    V3 F = forward();
    return norm({ -F.z, 0, F.x });
}

V3 Camera::up() const {
    return norm(cross(right(), forward()));
}

V3 Camera::forwardH() const {
    V3 F = forward();
    V3 h{ F.x, 0, F.z };
    return norm((h.x == 0 && h.z == 0) ? V3{ 0,0,1 } : h);
}

void Camera::update(double dt, const s3d::Vec2& winF, const s3d::Point& winP) {
    using namespace s3d;
    if (KeyBackspace.down()) free = true;
    if (KeyEnter.down()) { free = false; Cursor::SetPos(winP); }

    Cursor::RequestStyle(free ? CursorStyle::Default : CursorStyle::Hidden);
    if (free && MouseR.pressed()) {
        Vec2 d = Cursor::DeltaF();
        yaw -= d.x * RC_SENS;
        pitch += d.y * RC_SENS;
    }
    else if (!free) {
        Vec2 d = Cursor::PosF() - winF;
        yaw -= d.x * MS_SENS;
        pitch += d.y * MS_SENS;
        Cursor::SetPos(winP);
    }
    pitch = Clamp(pitch, -PITCH_LIM, PITCH_LIM);

    V3 F = forward();
    V3 Rv = right();
    V3 U = up();
    V3 Fh = forwardH();
    cam = free ? pos : pos + V3{ 0, EYE, 0 };

    if (free) {
        if (MouseR.pressed()) {
            if (KeyW.pressed()) cam = cam - MOVE * dt * Fh;
            if (KeyS.pressed()) cam = cam + MOVE * dt * Fh;
            if (KeyD.pressed()) cam = cam + MOVE * dt * Rv;
            if (KeyA.pressed()) cam = cam - MOVE * dt * Rv;
            if (KeyQ.pressed()) cam.y += MOVE * dt;
            if (KeyE.pressed()) cam.y -= MOVE * dt;
        }
        if (MouseM.pressed() || (KeyAlt.pressed() && MouseL.pressed())) {
            Vec2 d = Cursor::DeltaF();
            cam = cam - (d.x * Rv - d.y * U) * (PAN_SPD * dt / 8);
        }
        if (double w = Mouse::Wheel(); w != 0.0) cam = cam - F * w * DOLLY_SPD * dt;
        pos = cam;
    }
}
