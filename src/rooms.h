#pragma once

#include "radar.h"

#include <vector>

// The native room the player stands in, and the ways out of it. A maze level
// is built from preset cells, so one streamed room already is one chamber and
// no segmentation of our own is needed; the caller excludes the level types
// where that is not true. Pure logic over the collision World the sonar reads.
// `docs/rooms.md`
namespace D2RAccess::Rooms {
using Radar::Position;

// A gap in the room's border that leads into another streamed room.
struct Opening {
	Position centre{}; // midpoint of the gap, in collision subtiles
	int width = 0;     // gap width in cells
	Position across{}; // the cell just past the border, inside the room the gap leads to
	bool unexplored = false; // that room has never been stood in (stamped by the caller)
};

struct Room {
	int x = 0, y = 0, width = 0, height = 0; // the native room's own rect
	std::vector<Opening> openings;           // border order: top, bottom, left, right
	bool Contains(Position position) const;
	Position Centre() const;
};

// How wide a gap has to be for the player to fit through it, from the native
// footprint patterns: 0 is a point, 1/3/5 a five-cell cross, 2/4 a 3x3 square.
int PassableWidth(int bodyPattern);

// A room as the announcer keys it: the origin of its own rectangle. Invalid
// means off-grid, which is not a room change.
struct RoomKey {
	int x = 0, y = 0;
	bool valid = false;
	bool operator==(const RoomKey& other) const = default;
};

// Gate on room changes, ported from grimdark `core::rooms::Hysteresis`. Note
// the inversion that makes it feel right: the dwell applies only inside the
// first `settleMs` after a change, so the first room and any room entered
// after a settled second are spoken at once, and only a flip on a threshold
// has to hold. Both values are hand-tuned there; check them by listening.
template <typename Key>
struct BasicHysteresis {
	int settleMs = 1000, dwellMs = 400;
	Key current{}, candidate{};
	long long entered = -1, candidateSince = -1;
	// True when `observed` becomes the current key and is to be announced.
	bool Observe(const Key& observed, long long nowMs) {
		const auto dropCandidate = [&] { candidate = {}; candidateSince = -1; };
		// Off-grid keeps the current room: not knowing is not leaving.
		if (observed == current || !observed.valid) { dropCandidate(); return false; }
		if (const bool settled = !current.valid || nowMs - entered >= settleMs; !settled) {
			if (!(observed == candidate)) { candidate = observed; candidateSince = nowMs; }
			if (nowMs - candidateSince < dwellMs) return false;
		}
		current = observed;
		dropCandidate();
		entered = nowMs;
		return true;
	}
	void Forget() {
		current = candidate = {};
		entered = candidateSince = -1;
	}
};

struct Hysteresis {
	int settleMs = 1000, dwellMs = 400;
	RoomKey current{}, candidate{};
	long long entered = -1, candidateSince = -1;
	// True when `observed` becomes the current room and is to be announced.
	bool Observe(RoomKey observed, long long nowMs);
	void Forget();
};

// The streamed room covering `position` and every passable gap on its border.
// False when no resident room covers the point.
bool Describe(const Radar::World& world, Position position, Room& out);
// The level's own doorway width: the median width of every opening of every
// streamed room, in cells. Zero when no room has an opening loaded.
int TypicalOpeningWidth(const Radar::World& world);
}
