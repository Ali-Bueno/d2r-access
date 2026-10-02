#pragma once

#include "entity_radar.h"

#include <array>
#include <optional>

namespace D2RAccess::EntityCues {
// Ordinary: any other enemy, each of the nearest few heard on its own, as in Diablo 1.
enum class Kind { Npc, Important, Resurrector, Ranged, Ordinary };
inline constexpr size_t KindCount = 5;
inline constexpr size_t MaxOrdinary = 3;
// One cadence slot per class, and one per rank for ordinary enemies (Ordinary is last).
inline constexpr size_t SlotCount = KindCount - 1 + MaxOrdinary;
struct Settings {
	bool npcs = true, enemies = true, ranged = true, ordinary = true;
	double range = EntityRadar::Settings{}.range;
	// Listening feedback, 2026-09-22: much faster repeats, including mixed groups.
	uint64_t nearIntervalMs = 150, farIntervalMs = 450, gapMs = 100;
};
// Distance and fixed-screen placement of a target from the player.
struct Bearing { double distance = 0, right = 0, front = 1; };
Bearing BearingOf(Radar::Position player, Radar::Position target);
struct Cue {
	uint32_t id = 0;
	Kind kind = Kind::Npc;
	double distance = 0, right = 0, front = 1;
	uint64_t intervalMs = 0;
	size_t slot = 0; // the class, or Ordinary plus the enemy's rank by distance
};

// Nearest NPC plus at most three enemy streams: the nearest of each priority
// class first, then the nearest ordinary enemies in the streams left.
std::vector<Cue> Select(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, const Settings& settings);
class Tracker {
public:
	std::optional<Cue> Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
		uint64_t now, const Settings& settings);
	void Reset();
private:
	// Class cadence survives nearest-target changes to avoid rapid alternating
	// pings when two NPCs or archers repeatedly exchange the nearest position.
	struct Slot { bool played = false; uint64_t last = 0; };
	std::array<Slot, SlotCount> slots_{};
	bool played_ = false;
	uint64_t last_ = 0;
};
}
