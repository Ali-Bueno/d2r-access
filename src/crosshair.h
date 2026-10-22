#pragma once

#include "entity_cues.h"
#include "entity_radar.h"

#include <cstdint>
#include <optional>
#include <vector>

// The game's own target as a sound: a beep when the auto-target lands on an
// enemy in sight, the moment a sighted player sees it outlined. Pure; the
// rules are in docs/crosshair.md.
namespace D2RAccess::Crosshair {
// The hover entry as NativeHover reads it, kept apart so the rule is testable.
struct Aim { uint32_t type = 0, id = 0; };
struct Settings {
	// Sound design: the pad's target hops between a pack's members several times
	// a second; a hop sooner than this after the last beep waits for the next poll.
	uint64_t gapMs = 250;
	// Sound design, as the monster plate's repeat: within a pack the target trades back and
	// forth (live 2026-10-22: 340, 324, 340, 326, 340 in three seconds). A monster beeped for
	// is not beeped for again while it was the target within this long.
	uint64_t repeatMs = 3000;
};
struct Lock {
	uint32_t id = 0;
	EntityCues::Bearing bearing;
};

class Tracker {
public:
	// A lock to sound when the target has become an enemy in sight not heard as the target lately.
	std::optional<Lock> Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
		std::optional<Aim> aim, uint64_t nowMs, const Settings& settings = {});
	void Reset() { heard_.clear(); lastMs_.reset(); }
private:
	struct Heard { uint32_t id = 0; uint64_t ms = 0; };
	std::vector<Heard> heard_; // monsters beeped for, with the last time each was the target
	std::optional<uint64_t> lastMs_;
};
}
