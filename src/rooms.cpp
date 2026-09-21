#include "rooms.h"

#include <algorithm>
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
void Openings(const Radar::World& world, const Radar::RoomGrid& room, std::vector<Opening>& out) {
	const int passable = PassableWidth(world.bodyPattern);
	const int right = room.x + room.width - 1, bottom = room.y + room.height - 1;
	Side(world, room.x, room.y, room.width, 1, 0, 0, -1, passable, out);   // top
	Side(world, room.x, bottom, room.width, 1, 0, 0, 1, passable, out);    // bottom
	Side(world, room.x, room.y, room.height, 0, 1, -1, 0, passable, out);  // left
	Side(world, right, room.y, room.height, 0, 1, 1, 0, passable, out);    // right
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
	// A room covering a point owns it, exactly as World::Cell decides.
	const int index = world.RoomAt(static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)));
	if (index < 0) return false;
	const auto& found = world.rooms[static_cast<size_t>(index)];
	out = {found.x, found.y, found.width, found.height, {}};
	Openings(world, found, out.openings);
	return true;
}

int TypicalOpeningWidth(const Radar::World& world) {
	// Every opening is seen from both of its rooms; a median does not mind.
	std::vector<int> widths;
	std::vector<Opening> openings;
	for (const auto& room : world.rooms) {
		openings.clear();
		Openings(world, room, openings);
		for (const auto& opening : openings) widths.push_back(opening.width);
	}
	if (widths.empty()) return 0;
	const auto middle = widths.begin() + static_cast<long long>((widths.size() - 1) / 2); // lower median
	std::nth_element(widths.begin(), middle, widths.end());
	return *middle;
}
}
