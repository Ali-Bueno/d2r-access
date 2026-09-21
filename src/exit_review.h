#pragma once

#include "entity_radar.h"
#include "rooms.h"
#include "speech_strings.h"

#include <cstddef>
#include <string>
#include <vector>

// The ways out of where the player stands, nearest first, reviewed one at a
// time on demand. Ported from upstream's exit review (origin/main
// exit_review.*), in this branch's words: the name, the distance in steps and
// the screen direction, exactly as the pathfinder's catalogue speaks -- no
// clock bearing, no route verdict, and never a position in the list
// (CRITICAL RULE 5). Engine-free, so it is unit-tested without the game.
namespace D2RAccess::ExitReview {
using Radar::Position;

inline constexpr size_t NoItem = static_cast<size_t>(-1);

struct Item {
	std::string label;            // the passage word, or the level exit's own name
	Position point{};             // the opening's centre, or the exit's own position
	double distance = 0;          // straight line to it, in collision subtiles
	Radar::Direction direction = Radar::Direction::Up; // screen direction, as the scan names it
	bool named = false;           // a real unit guidance can re-locate by itself
	EntityRadar::Entity entity{}; // set when `named`
};

// Every way out as one list, nearest first: the room's plain openings under
// the passage word, the level exits under their own names.
std::vector<Item> Collect(Position player, const std::vector<Rooms::Opening>& openings,
	const std::vector<EntityRadar::Entity>& exits, const Strings::Table& strings);

// Cycles the list nearest-first, continuing from the item last reviewed.
class Cycle {
public:
	// The next item's index, or NoItem when there is nothing to review. Starts
	// at the nearest whenever the item last reviewed is no longer in the list.
	size_t Next(const std::vector<Item>& items);
	void Reset();
private:
	bool active_ = false;
	Position last_{};
};

// One spoken line: "passage, 12 steps, to the left".
std::string Describe(const Item& item, const EntityRadar::Settings& scan, const Strings::Table& strings);
}
