#include "rooms.h"

#include "level_exit_layout.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <optional>
#include <utility>

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
// controllersettings.json RoomTile.targetAcquisitionDistance: the pad takes a stair from this
// many subtiles away, so a stair this near a chamber's floor is that chamber's.
constexpr int StairReach = 4;
// Rooms::Doorways' deepest wall: a passage through a thick wall ends this far past its middle.
constexpr int DoorwayDepth = 2 * LevelExitLayout::SubtilesPerTile;

// The cells across a doorway's gap at its middle (two rows halfway through a thick wall), and
// one past each edge: the wall's own first cells.
template <typename Visit> void GapCells(const Doorway& doorway, Visit&& visit) {
	const double along = doorway.wallAlongY ? doorway.centre.y : doorway.centre.x;
	const double row = doorway.wallAlongY ? doorway.centre.x : doorway.centre.y;
	const int middle = static_cast<int>(std::lround(along)), half = doorway.width / 2 + 1;
	for (int r = static_cast<int>(std::floor(row)); r <= static_cast<int>(std::ceil(row)); ++r)
		for (int a = middle - half; a <= middle + half; ++a) doorway.wallAlongY ? visit(r, a) : visit(a, r);
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
	const auto within = [&](int cx, int cy) { return cx >= x && cy >= y && cx < x + width && cy < y + height; };
	if (area.empty()) return within(px, py);
	for (int cy = py - StairReach; cy <= py + StairReach; ++cy)
		for (int cx = px - StairReach; cx <= px + StairReach; ++cx)
			if (within(cx, cy) && area[static_cast<size_t>(cy - y) * width + (cx - x)]) return true;
	return false;
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
	// A flank shorter than a tile still counts where it turns a building's corner: Cott3's door
	// is two cells from its corner (a tester, 2026-10-23); a pillar has no wall running off it.
	constexpr int CornerRun = 2;
	const int passable = PassableWidth(world.bodyPattern);
	struct Face { int row = 0, from = 0, to = 0, side = 0; bool used = false; };
	std::vector<Doorway> result;
	for (const bool columns : {false, true}) {
		// `along` runs along the wall, `row` across it: rows of x for a wall along x, columns of y for one along y.
		const auto state = [&](int along, int row) {
			return columns ? world.Cell(row, along, Radar::Layer::Terrain) : world.Cell(along, row, Radar::Layer::Terrain); };
		const auto open = [&](int along, int row) { return state(along, row) == Radar::State::Open; };
		// The wall from `edge` outward, `step` at a time: a tile of it, or a corner.
		const auto flanked = [&](int edge, int step, int row) {
			int run = 0;
			while (run < Flank && !open(edge + step * run, row)) ++run;
			if (run == Flank) return true;
			if (run < CornerRun) return false;
			const int corner = edge + step * (run - 1);
			for (const int side : {-1, 1}) {
				int length = 1;
				while (length < Flank && state(corner, row + side * length) == Radar::State::Blocked) ++length;
				if (length == Flank) return true;
			}
			return false;
		};
		// The ground on `side` of a gap: open over the gap and `Widens` cells past each edge. One
		// edge may stop sooner where a wall runs off from the gap's own, a room's inner corner:
		// Cott4's door, one cell from its side wall (a tester, 2026-10-24).
		const auto widens = [&](int from, int to, int row, int side) {
			for (int at = from; at <= to; ++at)
				if (!open(at, row + side)) return false;
			bool cornered = false;
			for (const int step : {-1, 1}) {
				const int edge = step < 0 ? from : to;
				int reach = 0;
				while (reach < Widens && open(edge + step * (reach + 1), row + side)) ++reach;
				if (reach == Widens) continue;
				if (reach == 0 || cornered) return false;
				cornered = true;
				for (int length = 0; length < Flank; ++length)
					if (state(edge + step * (reach + 1), row + side * (length + 1)) != Radar::State::Blocked) return false;
			}
			return true;
		};
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
					if (!flanked(from - 1, -1, row) || !flanked(to + 1, 1, row)) continue;
					for (const int side : {-1, 1})
						if (widens(from, to, row, side)) faces.push_back({row, from, to, side});
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
			if (!known) result.push_back({at, face.to - face.from + 1, columns});
		}
	}
	return result;
}

bool Chamber(const Radar::World& world, Position position, const std::vector<Doorway>& doorways, Room& out) {
	const int px = static_cast<int>(std::floor(position.x)), py = static_cast<int>(std::floor(position.y));
	if (world.RoomAt(px, py) < 0) return false;
	int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
	for (const auto& room : world.rooms) {
		minX = std::min(minX, room.x); minY = std::min(minY, room.y);
		maxX = std::max(maxX, room.x + room.width); maxY = std::max(maxY, room.y + room.height);
	}
	const int width = maxX - minX, height = maxY - minY;
	const auto inside = [&](int x, int y) { return x >= minX && y >= minY && x < maxX && y < maxY; };
	const auto index = [&](int x, int y) { return static_cast<size_t>(y - minY) * width + (x - minX); };
	std::vector<uint8_t> shut(static_cast<size_t>(width) * height), reached(shut.size());
	for (const auto& doorway : doorways)
		GapCells(doorway, [&](int x, int y) { if (inside(x, y)) shut[index(x, y)] = 1; });
	const auto walkable = [&](int x, int y) {
		return inside(x, y) && !shut[index(x, y)] && Radar::Passable(world.Cell(x, y, Radar::Layer::Terrain)); };

	// Standing in a doorway's gap: the nearest floor beside it.
	std::optional<std::pair<int, int>> start;
	for (int reach = 0; reach <= StairReach && !start; ++reach) {
		int best = INT_MAX;
		for (int y = py - reach; y <= py + reach; ++y)
			for (int x = px - reach; x <= px + reach; ++x) {
				const int distance = (x - px) * (x - px) + (y - py) * (y - py);
				if (distance < best && walkable(x, y)) { best = distance; start = {x, y}; }
			}
	}
	if (!start) return false;
	std::vector<std::pair<int, int>> queue{*start};
	reached[index(start->first, start->second)] = 1;
	int left = start->first, top = start->second, right = left, bottom = top;
	for (size_t i = 0; i < queue.size(); ++i) {
		const auto [x, y] = queue[i];
		for (const auto& [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
			const int nx = x + dx, ny = y + dy;
			if (!walkable(nx, ny) || reached[index(nx, ny)]) continue;
			reached[index(nx, ny)] = 1;
			queue.push_back({nx, ny});
			left = std::min(left, nx); right = std::max(right, nx); top = std::min(top, ny); bottom = std::max(bottom, ny);
		}
	}
	out = {left, top, right - left + 1, bottom - top + 1, {}, {}};
	out.area.resize(static_cast<size_t>(out.width) * out.height);
	for (const auto& [x, y] : queue) out.area[static_cast<size_t>(y - top) * out.width + (x - left)] = 1;

	// A doorway leads out when the floor past its gap is the chamber's on one side only.
	for (const auto& doorway : doorways) {
		const double along = doorway.wallAlongY ? doorway.centre.y : doorway.centre.x;
		const double row = doorway.wallAlongY ? doorway.centre.x : doorway.centre.y;
		const int middle = static_cast<int>(std::floor(along));
		const auto cell = [&](int r) { return doorway.wallAlongY ? std::pair{r, middle} : std::pair{middle, r}; };
		bool ours[2]{};
		Position beyond[2]{};
		for (const int side : {0, 1}) {
			const int step = side ? 1 : -1;
			const int first = side ? static_cast<int>(std::ceil(row)) + 1 : static_cast<int>(std::floor(row)) - 1;
			const auto [x0, y0] = cell(first);
			beyond[side] = {static_cast<double>(x0), static_cast<double>(y0)};
			for (int depth = 0; depth <= DoorwayDepth; ++depth) {
				const auto [x, y] = cell(first + step * depth);
				if (!walkable(x, y)) continue;
				ours[side] = reached[index(x, y)] != 0;
				beyond[side] = {static_cast<double>(x), static_cast<double>(y)};
				break;
			}
		}
		if (ours[0] == ours[1]) continue;
		out.openings.push_back({doorway.centre, doorway.width, ours[0] ? beyond[1] : beyond[0]});
	}
	const auto distance = [&](const Opening& opening) {
		return std::hypot(opening.centre.x - position.x, opening.centre.y - position.y); };
	std::sort(out.openings.begin(), out.openings.end(),
		[&](const Opening& a, const Opening& b) { return distance(a) < distance(b); });
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
