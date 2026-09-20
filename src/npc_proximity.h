#pragma once

#include "entity_radar.h"
#include "radar.h"
#include "speech_strings.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Friendly NPC announcer: while the player explores, the nearest talk-to NPC
// (never an enemy, object or item) is named once with its distance and screen
// direction as soon as it is inside range, so the left stick can track it
// without pushing the right stick. Pure logic over the same World and Entity
// list as the entity scan; unit-tested without the game.
namespace D2RAccess::NpcProximity {
struct Settings {
	// Subtiles, same unit and default reach as the entity scan (EntityRadar::Settings).
	double range = EntityRadar::Settings{}.range;
	// An announced NPC only counts as gone once it is this far beyond range,
	// so pacing on the boundary never repeats it.
	double releaseMargin = 3;
	bool requireLineOfSight = false; // terrain line of sight before an NPC counts as seen
	uint64_t intervalMs = 500; // poll period; announcements are diff-gated, not periodic
};
struct Announcement { EntityRadar::Entity entity; double distance = 0; Radar::Direction direction = Radar::Direction::Up; };

// The screen direction whose axis is closest to the target (the scan's four
// sectors; ties on a diagonal go to the earlier direction in Radar::Direction order).
Radar::Direction DirectionOf(Radar::Position player, Radar::Position target);

class Tracker {
public:
	// One poll. Every friendly NPC inside range is remembered until it leaves
	// range plus the margin (or vanishes); the nearest remembered NPC is
	// announced the first time it is the nearest. Returns that announcement.
	std::optional<Announcement> Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, const Settings& settings);
	void Reset();
private:
	std::map<uint32_t, bool> inRange_; // unit id -> already announced
};

// "Kashya, 12 pasos, a la izquierda"; the direction is dropped under one step.
std::string Describe(const Announcement& announcement, const EntityRadar::Settings& scan, const Strings::Table& strings);
}
