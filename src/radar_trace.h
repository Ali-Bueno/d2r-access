#pragma once

#include "radar_audio.h"

namespace D2RAccess::RadarTrace {
// Log name of a radar direction; the cardinal frame for other features' indices.
const char* DirectionName(size_t direction, Radar::Frame frame = Radar::Frame::Cardinal);
const char* StateName(Radar::State state);
void Snapshot(const D2RL::PluginContext* context, const Radar::World& world, const Radar::Engine& engine, uint64_t now);
// The raw collision flags of the square `radius` cells around the player, as [radar-collision] lines.
void Collision(const D2RL::PluginContext* context, const Radar::World& world, int radius, uint64_t now);
// Call only for lifecycle events, never for every poll/frame. All writes use
// AsyncLog; event times are captured here before its delayed disk flush.
void Audio(const D2RL::PluginContext* context, size_t direction, Radar::State state,
	const RadarAudio::CueSource& source, const char* stage, const char* reason,
	uint64_t now, unsigned result = 0, uint64_t audioMs = 0);
}
