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

// The sides a push and a way out are told by: the screen's (up, left, right,
// down), or the map's, the room's own walls, which D2 draws at the screen
// diagonals (northeast, northwest, southeast, southwest: 2, 10, 4 and 8
// o'clock). A tester, 2026-10-14: "flick up-right to check for passages at
// 2 o'clock". The finder's Up is then the northeast wall, Right the southeast,
// Down the southwest, Left the northwest: the screen's turned 45 degrees
// clockwise. `docs/rooms.md`
enum class Sides { Screen, Map };
// The unit world direction a side stands for.
Position Axis(Radar::Direction direction, Sides sides);
// A world offset as the map's side it points to most: one of the four world axes.
Position MapSide(Position offset);
// The wall a passage is in, outward: a world axis.
Position Wall(const Rooms::Opening& opening);

struct Item {
	std::string label;            // the passage word, or the level exit's own name
	Position point{};             // the opening's centre, or the exit's own position
	Position wall{};              // a passage's wall (Wall); zero for a level exit
	double distance = 0;          // straight line to it, in collision subtiles
	bool named = false;           // a real unit guidance can re-locate by itself
	EntityRadar::Entity entity{}; // set when `named`
	bool unexplored = false;      // never taken: spoken with the unexplored tag
};
// Whether a level exit leads somewhere never seen; the caller owns the memory.
using UnexploredExit = bool (*)(const EntityRadar::Entity&);

// Every way out as one list, nearest first: the room's plain openings under
// the passage word, the level exits under their own names.
std::vector<Item> Collect(Position player, const std::vector<Rooms::Opening>& openings,
	const std::vector<EntityRadar::Entity>& exits, const Strings::Table& strings,
	UnexploredExit unexploredExit = nullptr);

// The ways out on one side, nearest first: the quarter of the plane centred on
// that side's axis; with the map's sides a passage by its own wall.
std::vector<Item> Toward(const std::vector<Item>& items, Position player, Radar::Direction direction, Sides sides = Sides::Screen);

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
// Nothing on that side, as the scan says it: "north: nothing".
std::string Nothing(Radar::Direction direction, const Strings::Table& strings, Sides sides = Sides::Screen);
}
