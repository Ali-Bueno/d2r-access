#pragma once

#include "entity_radar.h"
#include "rooms.h"
#include "speech_strings.h"

#include <cstddef>
#include <string>
#include <vector>

// The ways out of where the player stands, nearest first, reviewed one at a
// time on demand: label, distance, clock bearing and the beacon's verdict on
// getting there. Ported from grimdark's exit cycling, which reads
// "room 190, 5 away, 4 o'clock, 1 of 3". Engine-free, so it is unit-tested
// without the game. `docs/rooms.md`
namespace D2RAccess::ExitReview {
using Radar::Position;

inline constexpr size_t NoItem = static_cast<size_t>(-1);

struct Item {
	std::string label;            // the passage word, or the level exit's own name
	Position point{};             // the opening's centre, or the exit's own position
	double distance = 0;          // straight line to it, in collision subtiles
	int hour = 12;                // clock bearing on screen; 12 is screen up
	bool named = false;           // a real unit the beacon can re-locate by itself
	EntityRadar::Entity entity{}; // set when `named`
};

// Clock bearing of `target` seen from `player`, in the screen frame: 12 is
// screen up, 3 screen right. Over the same isometric projection as the rest.
int ClockHour(Position player, Position target);

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

// One spoken line; `reach` is the beacon's word, or null when it proved nothing.
std::string Describe(const Item& item, size_t index, size_t count, const char* reach,
	const EntityRadar::Settings& scan, const Strings::Table& strings);
}
