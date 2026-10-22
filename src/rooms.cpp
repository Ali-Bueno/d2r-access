#include "rooms.h"

#include "level_exit_layout.h"

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
			openings.push_back({{fromX + stepX * mid, fromY + stepY * mid}, run,
				{fromX + stepX * mid + outX, fromY + stepY * mid + outY}});
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

std::vector<Doorway> Doorways(const Radar::World& world) {
	// A doorway's wall is one wall tile from each side at most, and its gap at most two tiles
	// wide; a pillar is narrower than a tile, so a gap between pillars has no wall on its flanks
	// (INFERRED from the crypt tile set's tiles, 2026-10-21).
	constexpr int Tile = LevelExitLayout::SubtilesPerTile, MaxWidth = 2 * Tile, Depth = 2 * Tile, Flank = Tile;
	// The open ground past a wall reaches this far beyond each edge of its gap: a single cell
	// is the jag of a wall's collision, not a room.
	constexpr int Widens = 2;
	const int passable = PassableWidth(world.bodyPattern);
	struct Face { int row = 0, from = 0, to = 0, side = 0; bool used = false; };
	std::vector<Doorway> result;
	for (const bool columns : {false, true}) {
		// `along` runs along the wall, `row` across it: rows of x for a wall along x, columns of y for one along y.
		const auto state = [&](int along, int row) {
			return columns ? world.Cell(row, along, Radar::Layer::Terrain) : world.Cell(along, row, Radar::Layer::Terrain); };
		const auto open = [&](int along, int row) { return state(along, row) == Radar::State::Open; };
		std::vector<Face> faces;
		for (const auto& room : world.rooms) {
			const int first = columns ? room.y : room.x, length = columns ? room.height : room.width;
			const int top = columns ? room.x : room.y, rows = columns ? room.width : room.height;
			for (int row = top; row < top + rows; ++row)
				for (int along = first; along < first + length; ++along) {
					if (!open(along, row) || open(along - 1, row)) continue;
					int to = along;
					while (open(to + 1, row)) ++to;
					const int from = along;
					along = to;
					if (to - from + 1 < passable || to - from + 1 > MaxWidth) continue;
					if (state(from - 1, row) != Radar::State::Blocked || state(to + 1, row) != Radar::State::Blocked) continue;
					bool walled = true;
					for (int k = 0; k < Flank && walled; ++k) walled = !open(from - 1 - k, row) && !open(to + 1 + k, row);
					if (!walled) continue;
					for (const int side : {-1, 1}) {
						bool widens = true;
						for (int at = from - Widens; at <= to + Widens && widens; ++at) widens = open(at, row + side);
						if (widens) faces.push_back({row, from, to, side});
					}
				}
		}
		// Pair the two faces of one gap through a thick wall; a lone face opens on a passage that runs on.
		for (auto& face : faces) {
			if (face.used) continue;
			face.used = true;
			const int middle = (face.from + face.to) / 2;
			double row = face.row, centre = (face.from + face.to) / 2.0;
			bool through = true;
			for (int depth = 0; depth <= Depth; ++depth) {
				const int at = face.row - face.side * depth;
				if (depth > 0 && !open(middle, at)) { through = false; break; }
				const auto other = std::find_if(faces.begin(), faces.end(), [&](const Face& candidate) {
					return !candidate.used && candidate.row == at && candidate.side == -face.side
						&& candidate.from <= face.to && face.from <= candidate.to; });
				if (other == faces.end()) continue;
				other->used = true;
				row = (face.row + at) / 2.0;
				centre = (face.from + face.to + other->from + other->to) / 4.0;
				break;
			}
			if (!through) continue;
			const Position at = columns ? Position{row, centre} : Position{centre, row};
			const bool known = std::any_of(result.begin(), result.end(), [&](const Doorway& seen) {
				return std::abs(seen.centre.x - at.x) + std::abs(seen.centre.y - at.y) <= passable; });
			if (!known) result.push_back({at, face.to - face.from + 1});
		}
	}
	return result;
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
