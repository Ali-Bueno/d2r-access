#pragma once

#include "combat_target.h"
#include "entity_radar.h"

#include <array>
#include <optional>

namespace D2RAccess::EntityCues {
enum class Kind { Npc, Important, Resurrector, Ranged, Ordinary };
// Upper bound on simultaneous enemy voices; Settings::maxVoices picks the actual count.
inline constexpr size_t MaxVoices = 8, DefaultVoices = 6;
inline constexpr size_t KindCount = 5, MaxOrdinary = MaxVoices, SlotCount = KindCount - 1 + MaxOrdinary;
// Ranged attackers shoot from just past the screen edge, so they sound this far (subtiles) before they appear.
inline constexpr double RangedMargin = 3;
struct Settings {
	bool npcs = true, enemies = true, ranged = true, ordinary = true;
	double range = EntityRadar::Settings{}.range; // the cue radius when onScreen is off
	// Enemies sound once they are on the sighted player's screen, not within `range`.
	bool onScreen = true;
	CombatTarget::Screen screen;
	size_t maxVoices = DefaultVoices;
	// Listening feedback, 2026-09-22: much faster repeats, including mixed groups.
	// The far interval is longer than the 450 ms of three voices so six stay intelligible.
	uint64_t nearIntervalMs = 150, farIntervalMs = 600, gapMs = 100;
};
struct Bearing { double distance = 0, right = 0, front = 1; };
Bearing BearingOf(Radar::Position player, Radar::Position target);
struct Cue {
	uint32_t id = 0;
	Kind kind = Kind::Npc;
	double distance = 0, right = 0, front = 1;
	uint64_t intervalMs = 0;
	size_t slot = 0;
};

// The nearest NPC plus up to `maxVoices` enemies: specials first, then the nearest ordinary ones.
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
