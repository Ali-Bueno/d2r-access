#pragma once
#include "ground_items.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace D2RAccess::ItemSpatial {
// X3DAudio speaker matrix; attenuation/pitch stay in the world-distance policy.
std::array<float, 2> StereoMatrix(const GroundItems::Voice& voice);
// The balance the panning setting's 100 percent stands for: half of X3DAudio's, the
// 50 percent the testers found natural (2026-10-25). docs/ground-items.md
inline constexpr double NeutralPanScale = 0.5;
// `matrix` with its left-right balance scaled by `scale` (1 as given, 0 centred,
// past 1 further out to its side) and its power kept, the enemies' constant-power law.
inline std::array<float, 2> ScalePan(std::array<float, 2> matrix, double scale) {
	const double left = matrix[0], right = matrix[1], power = left * left + right * right;
	if (power <= 0) return matrix;
	const double pan = std::clamp((right * right - left * left) / power * scale, -1.0, 1.0);
	return {static_cast<float>(std::sqrt(power * (1 - pan) / 2)), static_cast<float>(std::sqrt(power * (1 + pan) / 2))};
}
}
