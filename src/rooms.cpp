#include "rooms.h"

#include <cmath>

namespace D2RAccess::Rooms {
namespace {
// The terrain layer, as the beacon and the radar use it: a monster standing in
// a doorway must not close the doorway.
bool Open(const Radar::World& world, int x, int y) {
	return world.Cell(x, y, Radar::Layer::Terrain) == Radar::State::Open;
}

// Walks one border of the room and collects the runs of cells that are open on
// both sides of it. `step` moves along the border, `out` crosses it.
void Side(const Radar::World& world, int fromX, int fromY, int length,
	int stepX, int stepY, int outX, int outY, int passable, std::vector<Opening>& openings) {
	int run = 0;
	for (int i = 0; i <= length; ++i) {
		const int x = fromX + stepX * i, y = fromY + stepY * i;
		const bool gap = i < length && Open(world, x, y) && Open(world, x + outX, y + outY);
		if (gap) { ++run; continue; }
		// The run covers indices i-run .. i-1, so its middle is i-(run+1)/2.
		if (run >= passable) {
			const double mid = i - (run + 1) / 2.0;
			openings.push_back({{fromX + stepX * mid, fromY + stepY * mid}, run});
		}
		run = 0;
	}
}
}

bool Room::Contains(Position position) const {
	const int px = static_cast<int>(std::floor(position.x)), py = static_cast<int>(std::floor(position.y));
	return px >= x && py >= y && px < x + width && py < y + height;
}

Position Room::Centre() const {
	return {x + (width - 1) / 2.0, y + (height - 1) / 2.0};
}

int PassableWidth(int bodyPattern) {
	// A cross or a square body occupies the cells beside it too, so a one-cell
	// slot in a wall is not a way through for anything but a point.
	return bodyPattern == 0 ? 1 : 3;
}

void Hysteresis::Forget() {
	current = candidate = {};
	entered = candidateSince = -1;
}

bool Hysteresis::Observe(RoomKey observed, long long nowMs) {
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

bool Describe(const Radar::World& world, Position position, Room& out) {
	const Radar::RoomGrid* found = nullptr;
	for (const auto& room : world.rooms) {
		// A room covering a point owns it, exactly as World::Cell decides.
		const int px = static_cast<int>(std::floor(position.x)), py = static_cast<int>(std::floor(position.y));
		if (px >= room.x && py >= room.y && px < room.x + room.width && py < room.y + room.height) { found = &room; break; }
	}
	if (!found) return false;
	out = {found->x, found->y, found->width, found->height, {}};
	const int passable = PassableWidth(world.bodyPattern);
	const int right = out.x + out.width - 1, bottom = out.y + out.height - 1;
	Side(world, out.x, out.y, out.width, 1, 0, 0, -1, passable, out.openings);   // top
	Side(world, out.x, bottom, out.width, 1, 0, 0, 1, passable, out.openings);   // bottom
	Side(world, out.x, out.y, out.height, 0, 1, -1, 0, passable, out.openings);  // left
	Side(world, right, out.y, out.height, 0, 1, 1, 0, passable, out.openings);   // right
	return true;
}
}
