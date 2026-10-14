#pragma once

#include "radar.h"

#include <cstdint>
#include <optional>
#include <vector>

// Where the game has shown an area border to be walkable, kept for the game.
// A border is named by the middle of the edge two areas share, which is often
// in the trees or cliffs lining it; the crossing itself is known only while
// its rooms are loaded (ZoneTransitions::Find). 2026-10-14, Cold Plains: a
// tester crossed into the Stony Field, came back by waypoint, and the beacon
// refused the Stony Field eight times from 129 to 334 units away, because the
// way was planned to the middle of the edge. docs/beacon.md#area-borders
namespace D2RAccess::CrossingMemory {
// The crossings ZoneTransitions::Find gave on `level` of game `session`.
void Note(uint64_t session, int level, const std::vector<Radar::LevelExit>& crossings);
// The crossing into `destination` seen nearest `near`, in this game.
std::optional<Radar::Position> Find(uint64_t session, int level, uint32_t destination, Radar::Position near);
void Clear();
}
