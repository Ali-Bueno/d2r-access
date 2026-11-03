#pragma once

#include "native_rooms.h"
#include "radar.h"

#include <cstdint>
#include <optional>
#include <vector>

// Where the teleport pads of a level lead. The game keeps no destination on a
// pad: using one sends the player to the other object of the pad's class in
// its room, onto a free cell within three of it (operate handler 27,
// 0x14058EAD0 in 3.3.93787). Every Arcane Sanctuary room file holds its pads
// two of a class (Act2/Arcane/sanct*2.ds1: 42 pads in 53 files), and the room
// at 25060,5420 measured 2026-10-02 holds the three pairs its file gives, told
// apart by class alone. Where a room holds other than one other pad of the
// class, nothing is assumed until the pad has been used; what a pad was seen
// to do wins over the rule. Pure logic, unit-tested without the game.
namespace D2RAccess::LevelRoute {

struct Pad {
	int x = 0, y = 0;
	uint16_t padClass = 0; // objects.txt class (EntityRadar::Entity::padClass)
	bool operator==(const Pad&) const = default;
};

struct Link {
	Pad from, to;
	bool seen = false; // the player was sent there; otherwise the game's rule says so
};

// The game lands the player on a free cell within this many cells each way of
// the pad led to; a pad at a walkway's end stands on shut ground, so the player
// lands beside it, never on it.
constexpr int LandWithin = 3;
// A free cell within LandWithin each way is at most 4.3 cells off the pad, and
// the position read is no cell's centre: this near a pad is where it puts a player.
constexpr double LandNear = 5;

// Where a pad leads, said beside it the way the automap and the eye tell a
// sighted player (docs/keyboard-play.md#where-a-pad-leads-said).
enum class Destination : uint8_t { Unknown, Been, New };

class Pads {
public:
	// A pad in view, kept by its cell and class: its unit id is new each time
	// its room is loaded again. True when it was not known.
	bool Note(const Pad& pad);
	// The pad at `from` put the player at `landing`. The known pad nearest the
	// landing, `from` aside, is where it leads, when it is near enough to be the
	// one the player was put beside. False, and nothing learned, otherwise.
	// The player has stood at both.
	bool Used(const Pad& from, Radar::Position landing, Pad& to);
	// The player stands at `player`: every known pad within LandNear has been
	// stood at. True when one was not before.
	bool Stood(Radar::Position player);
	bool Been(const Pad& pad) const;
	// The known pad nearest `at`, no farther than `within`.
	std::optional<Pad> Nearest(Radar::Position at, double within) const;
	// Where `from` leads: where it was seen to, else the only other pad of its
	// class in its room.
	std::optional<Link> Lead(const Pad& from, const std::vector<NativeRooms::Room>& rooms) const;
	// Whether `from` leads where the player has stood. New as well when no other
	// pad of its class in its room has been seen: the player was never near one.
	// Unknown when its room is not known or holds more than one.
	Destination Leads(const Pad& from, const std::vector<NativeRooms::Room>& rooms) const;
	std::vector<Link> Links(const std::vector<NativeRooms::Room>& rooms) const;
	const std::vector<Pad>& All() const { return pads_; }
	uint64_t Revision() const { return revision_; } // one more for every pad or use learned
	void Clear() { *this = {}; }
private:
	std::vector<Pad> pads_, been_;
	std::vector<Link> used_;
	uint64_t revision_ = 0;
};
}
