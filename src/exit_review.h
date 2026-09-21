#pragma once

#include "entity_radar.h"
#include "rooms.h"
#include "speech_strings.h"

#include <cstddef>
#include <string>
#include <vector>

// The ways out of the dungeon room the player stands in, found by direction:
// the exit finder, a right-stick mode (the user, 2026-09-21: "when on, it would
// take priority and the right stick when pressed in a direction would announce
// if there is an exit relative to that direction"). A room's plain passages are
// gaps in its walls, read from the collision map, not game units, so the entity
// scan has nothing to name for them. Built on upstream's exit review
// (origin/main exit_review.*); spoken the way the scan speaks, the name and the
// distance in steps, and never a position in the list (CRITICAL RULE 5).
// Engine-free, so it is unit-tested without the game.
namespace D2RAccess::ExitReview {
using Radar::Position;

inline constexpr size_t NoItem = static_cast<size_t>(-1);

struct Item {
	std::string label;            // the passage word, or the level exit's own name
	Position point{};             // the opening's centre, or the exit's own position
	double distance = 0;          // straight line to it, in collision subtiles
	bool named = false;           // a real unit guidance can re-locate by itself
	EntityRadar::Entity entity{}; // set when `named`
};

// Every way out as one list, nearest first: the room's plain openings under
// the passage word, the level exits under their own names.
std::vector<Item> Collect(Position player, const std::vector<Rooms::Opening>& openings,
	const std::vector<EntityRadar::Entity>& exits, const Strings::Table& strings);

// The ways out on one screen side, nearest first: the same quarter of the
// plane the entity scan gives a push, so both modes mean the same by "left".
std::vector<Item> Toward(const std::vector<Item>& items, Position player, Radar::Direction direction);

// Cycles a list nearest-first, continuing from the item named last: the first
// push is the nearest, the next the second nearest, and round again, with no
// timer. The caller resets it when the side or the room changes.
class Cycle {
public:
	// The next item's index, or NoItem when there is nothing on that side.
	// Starts at the nearest whenever the item named last is no longer listed.
	size_t Next(const std::vector<Item>& items);
	void Reset();
private:
	bool active_ = false;
	Position last_{};
};

// One spoken line, as the scan says it: "passage, 12 steps".
std::string Describe(const Item& item, const EntityRadar::Settings& scan, const Strings::Table& strings);
// Nothing on that side, as the scan says it: "Nothing to the left".
std::string Nothing(Radar::Direction direction, const Strings::Table& strings);
}
