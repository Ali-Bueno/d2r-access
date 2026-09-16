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
State World::Cell(int x, int y, Layer layer, uint16_t* rawFlags) const {
	if (rawFlags) *rawFlags = 0;
	for (const auto& room : rooms) {
		const int64_t dx = int64_t(x) - room.x, dy = int64_t(y) - room.y;
		if (dx < 0 || dy < 0 || dx >= room.width || dy >= room.height) continue;
		const auto index = static_cast<size_t>(dy * room.width + dx);
		if (index >= room.cells.size()) return State::Unknown;
		// D2R 0x366250 returns 0x27 for an absent room. Blank subtiles (0x20)
		// are unknown to the radar, not evidence of a real wall.
		constexpr uint16_t Blank = 0x20;
		if (room.cells[index] & Blank) return State::Unknown;
		uint16_t flags = room.cells[index];
		if (rawFlags) *rawFlags = flags;
		// 0x341D70 restores MONSTER=0x100; 0x363CF0 stamps NO_PATH/PET
		// alongside actor footprints. Player=0x80 is observed live. WATER=0xC0
		// also contains that bit: never mistake it for an actor. See evidence doc.
		constexpr uint16_t Player = 0x80, Monster = 0x100, Missile = 0x40, NoPath = 0x1000, Pet = 0x2000;
		const bool actor = (flags & Monster) || ((flags & Player) && !(flags & Missile));
		if (layer == Layer::Terrain && actor) flags &= ~(Monster | NoPath | Pet | ((flags & Missile) ? 0 : Player));
		// This estimate is only for stable geometry. A tin always requires the
		// original Movement layer too; coincident NO_PATH ownership is ambiguous.
		return flags & walkingMask ? State::Blocked : State::Open;
	}
	return State::Unknown;
}

State World::Body(int x, int y, Layer layer, uint16_t* flags) const {
	// Exact footprint dispatch recovered from D2R 0x364C70 / 0x366D40:
	// pattern 0 = point; 1/3/5 = five-cell cross; 2/4 = 3x3 square.
	State result = Cell(x, y, layer, flags);
	if (result == State::Blocked) return result;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (bodyPattern == 0 || (dx == 0 && dy == 0)) continue;
			if ((bodyPattern % 2) != 0 && dx != 0 && dy != 0) continue;
			const State next = Cell(x + dx, y + dy, layer, flags);
			if (next == State::Blocked) return next;
			if (next == State::Unknown) result = next;
		}
	}
	return result;
}

Probe SweepBody(const World& world, Position origin, Position axis, double range, double nearRange, Layer layer) {
	Probe probe;
	probe.body = State::Open;
	probe.end = State::Open;
	// Half-subtile steps include diagonal crossings; footprint checks at each
	// step keep a single thin centre ray from accepting an unusable gap.
	constexpr double Step = 0.5;
	for (double depth = Step; depth <= range; depth += Step) {
		const int x = CellCoordinate(origin.x, axis.x * depth);
		const int y = CellCoordinate(origin.y, axis.y * depth);
		// Native movement trace 0x364700 tests destination cells, not the
		// origin. A diagonal half-step can still round to the player's cell;
		// testing it would prevent leaving an overlapping obstacle behind.
		if (x == CellCoordinate(origin.x, 0) && y == CellCoordinate(origin.y, 0)) continue;
		probe.end = world.Body(x, y, layer, &probe.endFlags);
		if (depth <= nearRange) probe.body = probe.end;
		if (probe.end != State::Open) { probe.hit = {static_cast<double>(x), static_cast<double>(y)}; break; }
		probe.clearDepth = depth;
		probe.bodyDepth = std::min(depth, nearRange);
	}
	return probe;
}
Probe Scan(const World& world, Position axis, const Settings& settings, Layer layer) {
	Probe probe = SweepBody(world, world.player, axis, settings.farRange, settings.nearRange, layer);
	const Position side{-axis.y, axis.x};
	constexpr double Step = 0.5;
	std::array<State, 3> ends{};
	for (size_t lane = 0; lane < probe.depths.size(); ++lane) {
		const double offset = (static_cast<double>(lane) - 1) * settings.laneSpacing;
		ends[lane] = State::Open;
		for (double depth = Step; depth <= settings.farRange; depth += Step) {
			const int x = CellCoordinate(world.player.x, axis.x * depth + side.x * offset);
			const int y = CellCoordinate(world.player.y, axis.y * depth + side.y * offset);
			ends[lane] = world.Cell(x, y, layer);
			if (ends[lane] != State::Open) break;
			probe.depths[lane] = depth;
		}
	}
	auto ordered = probe.depths;
	std::sort(ordered.begin(), ordered.end());
	probe.medianDepth = ordered[1];
	if (layer == Layer::Terrain && probe.end == State::Blocked) {
		// AHC compares the supporting line of consecutive hit points. D2
		// supplies integer rejected body centres, so fit the line spatially in
		// this snapshot: it remains usable after sliding or a delayed poll.
		// One grid cell on each axis is the native footprint's extent.
		const Position offset{side.x < 0 ? -1.0 : side.x > 0 ? 1.0 : 0,
			side.y < 0 ? -1.0 : side.y > 0 ? 1.0 : 0};
		const auto a = SweepBody(world, {world.player.x-offset.x, world.player.y-offset.y}, axis, settings.farRange, settings.nearRange, layer);
		const auto b = SweepBody(world, {world.player.x+offset.x, world.player.y+offset.y}, axis, settings.farRange, settings.nearRange, layer);
		const Position tangent{b.hit.x-a.hit.x, b.hit.y-a.hit.y};
		const double length = std::hypot(tangent.x, tangent.y);
		if (a.end == State::Blocked && b.end == State::Blocked && length > 0) {
			const Position normal{-tangent.y/length, tangent.x/length};
			// Half of a grid cell's diagonal bounds centre quantization. A
			// corner/gap mixing different surfaces fails this collinearity test.
			const double gridBand = std::sqrt(0.5);
			bool surface = std::abs((probe.hit.x-a.hit.x)*normal.x + (probe.hit.y-a.hit.y)*normal.y) <= gridBand;
			const int steps = static_cast<int>(std::max(std::abs(tangent.x), std::abs(tangent.y)));
			const int stepX = axis.x < 0 ? -1 : axis.x > 0 ? 1 : 0;
			const int stepY = axis.y < 0 ? -1 : axis.y > 0 ? 1 : 0;
			for (int step = 1; surface && step < steps; ++step) {
				const int x = static_cast<int>(std::round(a.hit.x + tangent.x*step/steps));
				const int y = static_cast<int>(std::round(a.hit.y + tangent.y*step/steps));
				// A secant across a jamb can look almost collinear. Every part
				// of the fitted segment must be a real first-blocking boundary,
				// not empty space in the gap or the interior of a solid corner.
				surface = world.Body(x, y, layer) == State::Blocked
					&& world.Body(x-stepX, y-stepY, layer) == State::Open;
			}
			if (surface)
				probe.surfaceNormal = normal;
		}
	}
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
