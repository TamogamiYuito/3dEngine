/*-------------------------------------------------
  Siv3D v0.6.16
  FreeCam + FPS + Move / Rotate / Scale Gizmo
  Backspace : FreeCam   /   Enter : FPS
  LMB       : Select / Drag
  W / E / R : Move / Rotate / Scale
 -------------------------------------------------*/
# include <Siv3D.hpp>
# include <vector>
# include "Math.hpp"
# include "Cube.hpp"
# include "Gizmo.hpp"


/*=================================================
   Main
=================================================*/
void Main()
{
	using namespace s3d;
	Scene::SetBackground(ColorF{ 0.92,0.95,1 });
	const Vec2  WINF{ Scene::Width() * 0.5, Scene::Height() * 0.5 };
	const Point WINP{ (int32)WINF.x,(int32)WINF.y };

	/*--- キューブ集合 ---*/
	std::vector<Cube> cubes;
	std::unordered_set<GKey, GHash> grid;
	for (int z = -2; z <= 2; ++z)
		for (int x = -2; x <= 2; ++x)
		{
			cubes.push_back({ {gPos(x),0,gPos(z)} });
			grid.insert({ x,z });
		}

	/*--- カメラ & プレイヤー ---*/
	V3 pos{ 0,HALF + EYE,-200 }, cam = pos;
	double yaw = 0, pitch = 0, vy = 0; bool free = true;

	/*--- 編集状態 ---*/
	Mode   mode = Mode::Move;
	int    sel = -1, hoverIdx = -1;
	Handle hoverHd = Handle::None, activeHd = Handle::None;
	Drag   drag;

	/*================== ループ ==================*/
	while (System::Update())
	{
		const double dt = Scene::DeltaTime();
		const Vec2 cur = Cursor::PosF();

		/*--- 視点モード切替 ---*/
		if (KeyBackspace.down()) free = true;
		if (KeyEnter.down()) { free = false; Cursor::SetPos(WINP); }

		/*--- 視点回転 ---*/
		Cursor::RequestStyle(free ? CursorStyle::Default
								 : CursorStyle::Hidden);
		if (free && MouseR.pressed())
		{
			Vec2 d = Cursor::DeltaF();
			yaw -= d.x * RC_SENS;
			pitch += d.y * RC_SENS;
		}
		else if (!free)
		{
			Vec2 d = Cursor::PosF() - WINF;
			yaw -= d.x * MS_SENS;
			pitch += d.y * MS_SENS;
			Cursor::SetPos(WINP);
		}
		pitch = Clamp(pitch, -PITCH_LIM, PITCH_LIM);

		/*--- カメラ基底ベクトル ---*/
		V3 F = norm({ std::cos(pitch) * std::sin(yaw),
					  std::sin(pitch),
					 -std::cos(pitch) * std::cos(yaw) });
		V3 Rv = norm({ -F.z,0,F.x });
		V3 U = norm(cross(Rv, F));
		V3 Fh = norm(V3{ F.x,0,F.z }.x == 0 && V3{ F.x,0,F.z }.z == 0
					 ? V3{ 0,0,1 } : V3{ F.x,0,F.z });
		cam = free ? pos : pos + V3{ 0,EYE,0 };

		/*--- FreeCam 平行移動 ---*/
		if (free)
		{
			if (MouseR.pressed())
			{
				if (KeyW.pressed()) cam = cam - MOVE * dt * Fh;
				if (KeyS.pressed()) cam = cam + MOVE * dt * Fh;
				if (KeyD.pressed()) cam = cam + MOVE * dt * Rv;
				if (KeyA.pressed()) cam = cam - MOVE * dt * Rv;
				if (KeyQ.pressed()) cam.y += MOVE * dt;
				if (KeyE.pressed()) cam.y -= MOVE * dt;
			}
			if (MouseM.pressed() || (KeyAlt.pressed() && MouseL.pressed()))
			{
				Vec2 d = Cursor::DeltaF();
				cam = cam - (d.x * Rv - d.y * U) * (PAN_SPD * dt / 8);
			}
			if (double w = Mouse::Wheel(); w != 0.0) cam = cam - F * w * DOLLY_SPD * dt;
			pos = cam;
		}

		/*--- モードキー ---*/
		if (free && !drag.on)
		{
			if (KeyW.down()) mode = Mode::Move;
			if (KeyE.down()) mode = Mode::Rotate;
			if (KeyR.down()) mode = Mode::Scale;
		}

		/*--- ＋Cube ボタン ---*/
		if (free && SimpleGUI::Button(U"＋ Cube", { 20,20 }))
		{
			double t = (std::abs(F.y) > 1e-6) ? -cam.y / F.y : 6 * HALF;
			if (t <= 0) t = 6 * HALF;
			V3 hit = cam + t * F;
			GKey g{ gIdx(hit.x),gIdx(hit.z) };
			if (!grid.contains(g))
			{
				cubes.push_back({ {gPos(g.gx),0,gPos(g.gz)} });
				grid.insert(g);
			}
		}

		/*--- 2D 変換 λ ---*/
		auto scr = [&](V3 w)
			{
				V3 r = w - cam;
				return project({ dot(r,Rv), dot(r,U), -dot(r,F) },
							   WINF.x, WINF.y);
			};

		/*--- マウスレイ ---*/
		auto makeRay = [&](Vec2 p)->V3
			{
				double sx = p.x - WINF.x, sy = -(p.y - WINF.y);
				return norm(sx * Rv + sy * U + FOCAL * F);
			};


		/*--- カーソル位置 → 回転角を返す ---*/
                auto cursorAngle = [&](s3d::Vec2 p, V3 axis, V3 pivot)->std::optional<double>
                        {
                                V3 rd = makeRay(p);
                                V3 ax = norm(axis);

                                auto scrAngle = [&](double axF)->std::optional<double>
                                        {
                                                P2 sp = scr(pivot);
                                                if (std::isinf(sp.x)) return std::nullopt;
                                                s3d::Vec2 diff = p - s3d::Vec2{ sp.x, sp.y };
                                                double ang = std::atan2(diff.y, diff.x);
                                                if (axF < 0) ang = -ang;
                                                return ang;
                                        };

                                // カメラ視線と軸がほぼ平行な場合はスクリーン座標で計算
                                double axF = dot(ax, F);
                                if (std::abs(axF) > 0.95)
                                        return scrAngle(axF);

                                // カメラと軸の両方に垂直な平面
                                V3 n = norm(cross(ax, F));
                                if (len(n) < 1e-6) return scrAngle(axF);

                                double denom = dot(rd, n);
                                V3 hit; bool hitOk = false;

                                // 1) 交点を求める
                                if (std::abs(denom) > 1e-4)
                                {
                                        double t = dot(n, pivot - cam) / denom;
                                        hit = cam + rd * t;
                                        hitOk = true;
                                }
                                // 2) 平行時はカメラ平面で代用
                                else
                                {
                                        double denomF = dot(rd, F);
                                        if (std::abs(denomF) > 1e-6)
                                        {
                                                double t = dot(F, pivot - cam) / denomF;
                                                hit = cam + rd * t;
                                                hitOk = true;
                                        }
                                }

                                if (!hitOk)
                                        return scrAngle(axF);

                                V3 v = hit - pivot;

                                // 軸とカメラに基づく基底ベクトル
                                V3 p1 = norm(cross(ax, n));
                                V3 p2 = n;

                                return std::atan2(dot(v, p2), dot(v, p1));
                        };


		/*--- Hover キューブ（スクリーン AABB 判定） ---*/
		hoverIdx = -1; double bestDepth = 1e9;
		for (size_t i = 0; i < cubes.size(); ++i)
		{
			const Cube& cb = cubes[i];

			/* 8 頂点をスクリーンへ投影しバウンディング矩形を作成 */
			double minX = 1e9, minY = 1e9;
			double maxX = -1e9, maxY = -1e9;
			bool   any = false;
                        for (int k = 0; k < 8; ++k)
                        {
                                P2 p = scr(cb.c + qRotate(cb.q, LOCAL[k] * cb.s));
                                if (std::isinf(p.x)) continue;     // 画面外
				any = true;
				minX = std::min(minX, p.x);  maxX = std::max(maxX, p.x);
				minY = std::min(minY, p.y);  maxY = std::max(maxY, p.y);
			}
			if (!any) continue;

			/* カーソルが矩形内なら Hover 候補 */
			if (cur.x >= minX && cur.x <= maxX && cur.y >= minY && cur.y <= maxY)
			{
				/* 奥行きが浅いもの（画面手前）を優先 */
				double depth = len(cb.c - cam);
				if (depth < bestDepth)
				{
					bestDepth = depth;
					hoverIdx = static_cast<int>(i);
				}
			}
		}

		/*--- Hover ギズモ ---*/
		hoverHd = Handle::None;
		if (free && sel != -1 && !KeyAlt.pressed())
		{
			const Cube& cb = cubes[sel];
			const double L = HALF * cb.s * 1.5;
			P2 ctr = scr(cb.c);

			/* Move / Scale 軸 */
			if (!std::isinf(ctr.x) &&
				(mode == Mode::Move || mode == Mode::Scale))
			{
				struct Ax { V3 d; Color col; Handle mv, sc; };
				const Ax AX[3] = {
					{ {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
					{ {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
					{ {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
				};
				double bestA = 1e9;
				for (const auto& a : AX)
				{
					P2 tip = scr(cb.c + a.d * L); if (std::isinf(tip.x)) continue;
					double dLine = segDist2(cur, { ctr.x,ctr.y }, { tip.x,tip.y });
					double dTip = (cur - Vec2{ tip.x,tip.y }).lengthSq();

					if (mode == Mode::Move && dLine < 64 && dLine < bestA)
					{
						bestA = dLine; hoverHd = a.mv;
					}
					else if (mode == Mode::Scale)
					{
						if (dTip < 64 && dTip < bestA)
						{
							bestA = dTip; hoverHd = a.sc;
						}
						else if (dLine < 64 && dLine < bestA)
						{
							bestA = dLine; hoverHd = a.sc;
						}
					}
				}
				if (mode == Mode::Scale &&
					(cur - Vec2{ ctr.x,ctr.y }).lengthSq() < 64)
					hoverHd = Handle::ScaleUniform;
			}

			/* Rotate リング */
			if (mode == Mode::Rotate && !std::isinf(ctr.x))
			{
				struct Ring { Handle hd; Color col; };
				const Ring RG[3] = {
					{ Handle::RotateX, Palette::Red   },
					{ Handle::RotateY, Palette::Green },
					{ Handle::RotateZ, Palette::Blue  }
				};
				constexpr int SEG = 64;
				for (auto r : RG)
				{
					double bestR = 1e9;
					for (int k = 0; k < SEG; ++k)
					{
						double a0 = Math::TwoPi * k / SEG,
							a1 = Math::TwoPi * (k + 1) / SEG;
						V3 p0, p1;
						if (r.hd == Handle::RotateX)
						{
							p0 = { 0,std::sin(a0) * L,std::cos(a0) * L };
							p1 = { 0,std::sin(a1) * L,std::cos(a1) * L };
						}
						else if (r.hd == Handle::RotateY)
						{
							p0 = { std::sin(a0) * L,0,std::cos(a0) * L };
							p1 = { std::sin(a1) * L,0,std::cos(a1) * L };
						}
						else
						{
							p0 = { std::sin(a0) * L,std::cos(a0) * L,0 };
							p1 = { std::sin(a1) * L,std::cos(a1) * L,0 };
						}
						P2 s0 = scr(cb.c + p0), s1 = scr(cb.c + p1);
						if (std::isinf(s0.x) || std::isinf(s1.x)) continue;
						bestR = std::min(bestR,
										 segDist2(cur, { s0.x,s0.y }, { s1.x,s1.y }));
					}
					if (bestR < 64) { hoverHd = r.hd; break; }
				}
			}
		}

		/*--- Mouse Down ---*/
		if (free && MouseL.down())
		{
			if (hoverHd != Handle::None && sel != -1)
			{
				activeHd = hoverHd;
				drag.on = true;
				drag.cur0 = cur;

                                Cube& cb = cubes[sel];
                                drag.p0 = cb.c;
                                drag.q0 = cb.q;
                                drag.s0 = cb.s;

				/* grid から抜く (X / Z 移動時) */
				if (activeHd == Handle::MoveX || activeHd == Handle::MoveZ)
					grid.erase({ gIdx(cb.c.x), gIdx(cb.c.z) });

				if (activeHd == Handle::MoveX || activeHd == Handle::MoveY
				 || activeHd == Handle::MoveZ)
				{
					V3 ax = (activeHd == Handle::MoveX) ? V3{ 1,0,0 } :
						(activeHd == Handle::MoveY) ? V3{ 0,1,0 } :
						V3{ 0,0,1 };
					P2 p0 = scr(cb.c), p1 = scr(cb.c + ax);
					drag.lenPx = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).length();
					if (drag.lenPx < 1) drag.lenPx = 1;
				}
				/* 回転：軸ベクトルと開始角を保存 */
				if (activeHd == Handle::RotateX || activeHd == Handle::RotateY
				|| activeHd == Handle::RotateZ)
					 {
					drag.axis = (activeHd == Handle::RotateX) ? V3{ 1,0,0 }
						 : (activeHd == Handle::RotateY) ? V3{ 0,1,0 }
					 : V3{ 0,0,1 };
					drag.ang0 = cursorAngle(cur, drag.axis, cb.c).value_or(0.0);
					}
			}
			else if (hoverIdx != -1) sel = hoverIdx;
		}

		/*--- Mouse Up ---*/
		if (free && MouseL.up())
		{
			if (drag.on &&
				(activeHd == Handle::MoveX || activeHd == Handle::MoveZ))
			{
				Cube& cb = cubes[sel];
				grid.insert({ gIdx(cb.c.x), gIdx(cb.c.z) });
			}
			drag.on = false; activeHd = Handle::None;
		}

		/*--- ドラッグ処理 ---*/
		if (drag.on && sel != -1)
		{
			Vec2 d = cur - drag.cur0;
			Cube& cb = cubes[sel];

			/* Move */
			if (activeHd == Handle::MoveX || activeHd == Handle::MoveY
			 || activeHd == Handle::MoveZ)
			{
				V3 ax = (activeHd == Handle::MoveX) ? V3{ 1,0,0 } :
					(activeHd == Handle::MoveY) ? V3{ 0,1,0 } : V3{ 0,0,1 };
				P2 p0 = scr(drag.p0), p1 = scr(drag.p0 + ax);
				Vec2 dir = (Vec2{ p1.x,p1.y } - Vec2{ p0.x,p0.y }).normalized();
				double pix = Dot(d, dir);
				double world = (pix / drag.lenPx) * 40.0;   // 1px = 40 world
				cb.c = drag.p0 + ax * world;
			}
			/* Rotate：カーソル角度差分で回転 */
			 else if (activeHd == Handle::RotateX || activeHd == Handle::RotateY
			 || activeHd == Handle::RotateZ)
				 {
				if (auto ang = cursorAngle(cur, drag.axis, cb.c))
					 {
					double delta = *ang - drag.ang0;
					if (delta > s3d::Math::Pi)      delta -= s3d::Math::TwoPi;
					if (delta < -s3d::Math::Pi)      delta += s3d::Math::TwoPi;
                                        Quat dq = qAxisAngle(drag.axis, delta);
                                        cb.q = qNormalize(qMul(dq, drag.q0));
					}
				 }
			/* Scale */
			else
			{
				cb.s = Clamp(drag.s0 * (1.0 + d.x * 0.002), 0.1, 5.0);
			}
		}

		/*--- FPS 移動 (簡略) ---*/
		if (!free)
		{
			if (KeyW.pressed()) pos = pos - MOVE * dt * Fh;
			if (KeyS.pressed()) pos = pos + MOVE * dt * Fh;
			if (KeyD.pressed()) pos = pos + MOVE * dt * Rv;
			if (KeyA.pressed()) pos = pos - MOVE * dt * Rv;

			bool onGround = false; double foot = pos.y - EYE, head = foot + CAP_H;
			for (const auto& cb : cubes)
			{
				double h = HALF * cb.s;
				/* 着地：下降中 ＋ ±LAND_EPS 以内 */
				double dynamicEps = Max(2.0, (-vy) * dt + 0.5); // 最低 2px、速いほど広げる
				if (vy <= 0 &&
					foot >= cb.c.y + h - dynamicEps &&
					foot <= cb.c.y + h + dynamicEps)
				{
					double dx = std::max(std::abs(pos.x - cb.c.x) - h, 0.0);
					double dz = std::max(std::abs(pos.z - cb.c.z) - h, 0.0);
					if (dx * dx + dz * dz <= R * R)
					{
						pos.y = cb.c.y + h + EYE; vy = 0; onGround = true;
						foot = pos.y - EYE; head = foot + CAP_H;
					}
				}
				/* 横衝突 */
				if (head <= cb.c.y - h || foot >= cb.c.y + h) continue;
				double cx = std::clamp(pos.x, cb.c.x - h, cb.c.x + h);
				double cz = std::clamp(pos.z, cb.c.z - h, cb.c.z + h);
				double dx = pos.x - cx, dz = pos.z - cz, d2 = dx * dx + dz * dz;
				if (d2 < R * R - 1e-6)
				{
					double d = std::sqrt(std::max(d2, 1e-6));
					V3 push{ dx,0,dz }; push = ((R - d) / d) * push; pos = pos + push;
				}
			}
			if (KeySpace.down() && onGround) vy = JUMP; else vy -= GRAV * dt;
			pos.y += vy * dt; cam = pos + V3{ 0,EYE,0 };
		}

		/*================= 描画 =================*/
		Scene::SetBackground(ColorF{ 0.92,0.95,1 });

		/*--- キューブ ---*/
		for (size_t i = 0; i < cubes.size(); ++i)
		{
			const Cube& cb = cubes[i];
			ColorF col = ((int)i == sel) ? ColorF(Palette::Red)
				: (free && (int)i == hoverIdx) ? ColorF(Palette::Yellow)
				: ColorF{ 1.0,0.8,0.3 };
			double th = ((int)i == sel) ? 3 : 1;

			std::array<P2, 8> v;
                        for (int k = 0; k < 8; ++k)
                        {
                                V3 p = LOCAL[k] * cb.s;
                                v[k] = scr(cb.c + qRotate(cb.q, p));
                        }
			for (auto [a, b] : EDGE)
				if (!std::isinf(v[a].x) && !std::isinf(v[b].x))
					Line{ v[a].x,v[a].y, v[b].x,v[b].y }.draw(th, col);
		}

		/*--- ギズモ ---*/
		if (free && sel != -1)
		{
			const Cube& cb = cubes[sel];
			const double L = HALF * cb.s * 1.5;
			P2 ctr = scr(cb.c); if (std::isinf(ctr.x)) continue;

			/* Move / Scale */
			if (mode == Mode::Move || mode == Mode::Scale)
			{
				struct Ax { V3 d; Color col; Handle mv, sc; };
				const Ax AX[3] = {
					{ {1,0,0}, Palette::Red,   Handle::MoveX, Handle::ScaleX },
					{ {0,1,0}, Palette::Green, Handle::MoveY, Handle::ScaleY },
					{ {0,0,1}, Palette::Blue,  Handle::MoveZ, Handle::ScaleZ }
				};
				for (const auto& a : AX)
				{
					P2 tip = scr(cb.c + a.d * L); if (std::isinf(tip.x)) continue;
					bool hot = (hoverHd == a.mv || hoverHd == a.sc
							 || activeHd == a.mv || activeHd == a.sc);

					ColorF col = a.col; col.a = hot ? 1.0 : 0.4;
					Line{ ctr.x,ctr.y, tip.x,tip.y }.draw(hot ? 4 : 3, col);

					if (mode == Mode::Move)
					{
						Vec2 v = (Vec2{ tip.x,tip.y } - Vec2{ ctr.x,ctr.y }).normalized() * 8;
						Vec2 n{ -v.y,v.x };
						Polygon{ Vec2{tip.x,tip.y},
								 Vec2{tip.x,tip.y} - v * 2 + n,
								 Vec2{tip.x,tip.y} - v * 2 - n }.draw(col);
					}
					else /* Scale */
						RectF{ tip.x - 4, tip.y - 4, 8,8 }.draw(col);
				}
				if (mode == Mode::Scale)
				{
					bool hot = (hoverHd == Handle::ScaleUniform
							 || activeHd == Handle::ScaleUniform);
					ColorF cu = hot ? ColorF(Palette::White)
						: ColorF{ 1,1,1,0.4 };
					Circle{ ctr.x,ctr.y,5 }.drawFrame(2, cu);
				}
			}

			/* Rotate */
			if (mode == Mode::Rotate)
			{
				struct Ring { Handle hd; Color col; };
				const Ring RG[3] = {
					{ Handle::RotateX, Palette::Red   },
					{ Handle::RotateY, Palette::Green },
					{ Handle::RotateZ, Palette::Blue  }
				};
				constexpr int SEG = 64;
				for (auto r : RG)
				{
					bool hot = (hoverHd == r.hd || activeHd == r.hd);
					ColorF col = r.col; col.a = hot ? 1.0 : 0.4;
					for (int k = 0; k < SEG; ++k)
					{
						double a0 = Math::TwoPi * k / SEG,
							a1 = Math::TwoPi * (k + 1) / SEG;
						V3 p0, p1;
						if (r.hd == Handle::RotateX)
						{
							p0 = { 0,std::sin(a0) * L,std::cos(a0) * L };
							p1 = { 0,std::sin(a1) * L,std::cos(a1) * L };
						}
						else if (r.hd == Handle::RotateY)
						{
							p0 = { std::sin(a0) * L,0,std::cos(a0) * L };
							p1 = { std::sin(a1) * L,0,std::cos(a1) * L };
						}
						else
						{
							p0 = { std::sin(a0) * L,std::cos(a0) * L,0 };
							p1 = { std::sin(a1) * L,std::cos(a1) * L,0 };
						}
						P2 s0 = scr(cb.c + p0), s1 = scr(cb.c + p1);
						if (!std::isinf(s0.x) && !std::isinf(s1.x))
							Line{ s0.x,s0.y, s1.x,s1.y }.draw(hot ? 3 : 2, col);
					}
				}
			}
		}
		/* ====== プレイヤー当たり判定円柱 ====== */
		if (!free)                                // ← FPS モードのときだけ
		{
			constexpr int SEG = 24;               // 分割数
			double foot = pos.y - EYE;            // 足元
			double top = foot + CAP_H;           // 頭の高さ

			for (int k = 0; k < SEG; ++k)
			{
				double a0 = Math::TwoPi * k / SEG;
				double a1 = Math::TwoPi * (k + 1) / SEG;

				V3 b0{ pos.x + R * std::cos(a0), foot, pos.z + R * std::sin(a0) };
				V3 b1{ pos.x + R * std::cos(a1), foot, pos.z + R * std::sin(a1) };
				V3 t0{ b0.x, top, b0.z }, t1{ b1.x, top, b1.z };

				P2 p0 = scr(b0), p1 = scr(t0),
					p2 = scr(b1), p3 = scr(t1);

				if (!std::isinf(p0.x) && !std::isinf(p1.x))
					Line{ p0.x,p0.y, p1.x,p1.y }.draw(1, Palette::Cyan);
				if (!std::isinf(p0.x) && !std::isinf(p2.x))
					Line{ p0.x,p0.y, p2.x,p2.y }.draw(1, Palette::Cyan);
				if (!std::isinf(p1.x) && !std::isinf(p3.x))
					Line{ p1.x,p1.y, p3.x,p3.y }.draw(1, Palette::Cyan);
			}
		}
	}
}
