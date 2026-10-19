#include "radar_readout.h"
#include "spoken_direction.h"
#include <cmath>
#include <cstdio>
#include <numbers>

namespace D2RAccess::RadarReadout {
int MovementDirection(Radar::Position velocity) {
	if (velocity.x == 0 && velocity.y == 0) return -1;
	return Radar::ScreenSector(velocity);
}
const char* DirectionName(Radar::Frame frame, size_t direction, const Strings::Table& text) {
	if (frame == Radar::Frame::Cardinal) return SpokenDirection::OfSide(static_cast<Radar::Direction>(direction), text);
	return SpokenDirection::Of(Radar::RaysOf(frame)[direction].world, text);
}
std::string DirectionList(Radar::Frame frame, const Strings::Table& text) {
	std::string result;
	for (size_t i = 0; i < Radar::DirectionCount; ++i) result += std::string(i ? text.listSeparator : "") + DirectionName(frame, i, text);
	return result;
}
std::string RaysPhrase(Radar::Frame frame, const Strings::Table& text) {
	char phrase[256]{};
	std::snprintf(phrase, sizeof phrase, text.radarRays, DirectionList(frame, text).c_str());
	return phrase;
}
std::string Describe(const Radar::Engine& engine, const Strings::Table& text) {
	const int direction = MovementDirection(engine.Velocity());
	std::string result;
	if (!engine.HasMotionSamples()) result = text.radarMotionUnknown;
	else if (direction < 0) result = text.radarStationary;
	else {
		char motion[160]{};
		std::snprintf(motion, sizeof(motion), text.radarMoving, SpokenDirection::Of(engine.Velocity(), text));
		result = motion;
	}
	result += std::string(". ") + text.radarStraight;
	const auto frame = engine.Configuration().frame;
	for (size_t i = 0; i < Radar::DirectionCount; ++i) {
		const auto& passage = engine.Sensors()[i].passage;
		const auto state = passage.available ? passage.observed : Radar::State::Unknown;
		// Inside the hysteresis band the verdict is the one that stands, not a measurement.
		const bool limited = passage.available && passage.depth > passage.closeDepth && passage.depth < passage.openDepth;
		result += std::string(". ") + DirectionName(frame, i, text) + ": "
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
