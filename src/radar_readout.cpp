#include "radar_readout.h"
#include <cmath>
#include <cstdio>
#include <numbers>

namespace D2RAccess::RadarReadout {
int MovementDirection(Radar::Position velocity) {
	if (velocity.x == 0 && velocity.y == 0) return -1;
	return Radar::ScreenSector(velocity);
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
		// Inside the hysteresis band the verdict is the one that stands, not a measurement.
		const bool limited = passage.available && passage.depth > passage.closeDepth && passage.depth < passage.openDepth;
		result += std::string(". ") + text.radarDirections[i] + ": "
			+ (limited ? text.radarLimited : text.radarStates[static_cast<size_t>(state)]);
		if (!passage.available) continue;
		if (passage.unexplored) result += std::string(", ") + text.terrainUnexplored;
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
