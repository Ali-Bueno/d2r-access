#include "radar.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
namespace {
int CellCoordinate(double origin, double displacement) {
	// Native integer positions identify cell centres. At an exact half-cell
	// boundary, visit the cell in the scan direction, symmetrically both ways.
	constexpr double HalfCell = 0.5;
	const double position = origin + displacement;
	return static_cast<int>(displacement < 0 ? std::ceil(position - HalfCell) : std::floor(position + HalfCell));
}
}
State World::Cell(int x, int y) const {
	for (const auto& room : rooms) {
		const int64_t dx = int64_t(x) - room.x, dy = int64_t(y) - room.y;
		if (dx < 0 || dy < 0 || dx >= room.width || dy >= room.height) continue;
		const auto index = static_cast<size_t>(dy * room.width + dx);
		if (index >= room.cells.size()) return State::Unknown;
		// D2R 0x366250 returns 0x27 for an absent room. Blank subtiles (0x20)
		// are unknown to the radar, not evidence of a real wall.
		constexpr uint16_t Blank = 0x20;
		if (room.cells[index] & Blank) return State::Unknown;
		return room.cells[index] & walkingMask ? State::Blocked : State::Open;
	}
	return State::Unknown;
}

State World::Body(int x, int y) const {
	// Exact footprint dispatch recovered from D2R 0x364C70 / 0x366D40:
	// pattern 0 = point; 1/3/5 = five-cell cross; 2/4 = 3x3 square.
	State result = Cell(x, y);
	if (result == State::Blocked) return result;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (bodyPattern == 0 || (dx == 0 && dy == 0)) continue;
			if ((bodyPattern % 2) != 0 && dx != 0 && dy != 0) continue;
			const State next = Cell(x + dx, y + dy);
			if (next == State::Blocked) return next;
			if (next == State::Unknown) result = next;
		}
	}
	return result;
}

Probe Scan(const World& world, Position axis, const Settings& settings) {
	Probe probe;
	const Position side{-axis.y, axis.x};
	probe.body = State::Open;
	probe.end = State::Open;
	// Half-subtile steps include diagonal crossings; footprint checks at each
	// step keep a single thin centre ray from accepting an unusable gap.
	constexpr double Step = 0.5;
	for (double depth = Step; depth <= settings.farRange; depth += Step) {
		const int x = CellCoordinate(world.player.x, axis.x * depth);
		const int y = CellCoordinate(world.player.y, axis.y * depth);
		// Native movement trace 0x364700 tests destination cells, not the
		// origin. A diagonal half-step can still round to the player's cell;
		// testing it would prevent leaving an overlapping obstacle behind.
		if (x == static_cast<int>(world.player.x) && y == static_cast<int>(world.player.y)) continue;
		probe.end = world.Body(x, y);
		if (depth <= settings.nearRange) probe.body = probe.end;
		if (probe.end != State::Open) break;
		probe.clearDepth = depth;
		probe.bodyDepth = std::min(depth, settings.nearRange);
	}
	std::array<State, 3> ends{};
	for (size_t lane = 0; lane < probe.depths.size(); ++lane) {
		const double offset = (static_cast<double>(lane) - 1) * settings.laneSpacing;
		ends[lane] = State::Open;
		for (double depth = Step; depth <= settings.farRange; depth += Step) {
			const int x = CellCoordinate(world.player.x, axis.x * depth + side.x * offset);
			const int y = CellCoordinate(world.player.y, axis.y * depth + side.y * offset);
			ends[lane] = world.Cell(x, y);
			if (ends[lane] != State::Open) break;
			probe.depths[lane] = depth;
		}
	}
	auto ordered = probe.depths;
	std::sort(ordered.begin(), ordered.end());
	probe.medianDepth = ordered[1];
	probe.available = probe.end == State::Blocked || probe.clearDepth >= settings.openDepth;
	if (probe.body == State::Blocked || (probe.end == State::Blocked && probe.clearDepth <= settings.blockedDepth))
		probe.observed = State::Blocked;
	else if (probe.clearDepth >= settings.openDepth && probe.medianDepth >= settings.openDepth)
		probe.observed = State::Open;
	// Unknown includes the spatial dead band. Never claim an unloaded boundary
	// is open merely because a ray did not find a wall.
	return probe;
}
}
