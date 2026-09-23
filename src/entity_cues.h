#pragma once

#include "entity_radar.h"

#include <array>
#include <optional>

namespace D2RAccess::EntityCues {
enum class Kind { Npc, Important, Resurrector, Ranged };
struct Settings {
	bool npcs = true, enemies = true, ranged = true;
	double range = EntityRadar::Settings{}.range;
	// Sound-design defaults: an unhurried distant pulse, faster near contact.
	uint64_t nearIntervalMs = 550, farIntervalMs = 1800, gapMs = 160;
};
struct Cue {
	uint32_t id = 0;
	Kind kind = Kind::Npc;
	double distance = 0, right = 0, front = 1;
	uint64_t intervalMs = 0;
};

// Nearest NPC plus at most two enemy classes; ordinary melee packs stay silent.
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
	std::array<Slot, 4> slots_{};
	bool played_ = false;
	uint64_t last_ = 0;
};
}
