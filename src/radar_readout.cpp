#include "radar_readout.h"
#include <cmath>
#include <cstdio>
#include <numbers>

namespace D2RAccess::RadarReadout {
int MovementDirection(Radar::Position velocity) {
	if (velocity.x == 0 && velocity.y == 0) return -1;
	// D2R 0x334DB0: screen X=80*(X-Y), Y=40*(X+Y). Preserve the
	// 2:1 isometric ratio when describing actual screen movement.
	const double x = 2 * (velocity.x - velocity.y), y = velocity.x + velocity.y;
	constexpr int Sectors = 8; // radarMovementDirections: eight screen sectors clockwise from up
	const auto sector = std::lround(std::atan2(x, -y) / (2 * std::numbers::pi / Sectors));
	return static_cast<int>((sector + Sectors) % Sectors);
}
std::string Describe(const Radar::Engine& engine, const Strings::Table& text) {
	const int direction = MovementDirection(engine.Velocity());
	std::string result;
	if (!engine.HasMotionSamples()) result = text.radarMotionUnknown;
	else if (direction < 0) result = text.radarStationary;
	else {
		char motion[160]{};
		std::snprintf(motion, sizeof(motion), text.radarMoving, text.radarMovementDirections[direction]);
		result = motion;
	}
	result += std::string(". ") + text.radarStraight;
	for (size_t i = 0; i < Radar::DirectionCount; ++i) {
		const auto& passage = engine.Sensors()[i].passage;
		const auto state = passage.available ? passage.observed : Radar::State::Unknown;
		const bool limited = passage.available && passage.observed == Radar::State::Unknown;
		result += std::string(". ") + text.radarDirections[i] + ": "
			+ (limited ? text.radarLimited : text.radarStates[static_cast<size_t>(state)]);
		if (!passage.available) continue;
		if (passage.depth < 1) result += std::string(", ") + text.radarNearbyObstacle;
		else {
			char depth[80]{};
			std::snprintf(depth, sizeof(depth), text.radarDepth, std::floor(passage.depth));
			result += std::string(", ") + depth;
		}
	}
	return result;
}
}
