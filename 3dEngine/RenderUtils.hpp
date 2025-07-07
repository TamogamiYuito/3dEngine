#pragma once
#include "Math.hpp"
#include <Siv3D.hpp>
#include <optional>

P2 screenProject(V3 world, V3 cam, V3 Rv, V3 U, V3 F, double cx, double cy);
V3 rayFromCursor(s3d::Vec2 p, V3 Rv, V3 U, V3 F, const s3d::Vec2& winF);
std::optional<double> angleFromCursor(s3d::Vec2 p, V3 axis, V3 pivot,
                                      V3 cam, V3 Rv, V3 U, V3 F,
                                      double cx, double cy);

inline std::vector<V3> clipNear(const std::vector<V3>& in, double kNear)
{
	std::vector<V3> out;
	const size_t n = in.size();
	for (size_t i = 0; i < n; ++i)
	{
		const V3& a = in[i];
		const V3& b = in[(i + 1) % n];
		const bool ina = (a.z >= kNear);
		const bool inb = (b.z >= kNear);

		if (ina) out.push_back(a);

		if (ina ^ inb)          // 片方だけ内側
		{
			double t = (kNear - a.z) / (b.z - a.z);
			out.push_back({ a.x + t * (b.x - a.x),
							a.y + t * (b.y - a.y),
							kNear });          // z = kNear
		}
	}
	return out;
}
