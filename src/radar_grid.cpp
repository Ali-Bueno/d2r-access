#include "radar.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
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
	State body = State::Open;
	// Half-subtile steps include diagonal crossings; footprint checks at each
	// step keep a single thin centre ray from accepting an unusable gap.
	constexpr double Step = 0.5;
	for (double depth = Step; depth <= settings.nearRange; depth += Step) {
		body = world.Body(static_cast<int>(std::floor(world.player.x + axis.x * depth)),
			static_cast<int>(std::floor(world.player.y + axis.y * depth)));
		if (body != State::Open) break;
		probe.bodyDepth = depth;
	}
	std::array<State, 3> ends{};
	for (size_t lane = 0; lane < probe.depths.size(); ++lane) {
		const double offset = (static_cast<double>(lane) - 1) * settings.laneSpacing;
		ends[lane] = State::Open;
		for (double depth = Step; depth <= settings.farRange; depth += Step) {
			const int x = static_cast<int>(std::floor(world.player.x + axis.x * depth + side.x * offset));
			const int y = static_cast<int>(std::floor(world.player.y + axis.y * depth + side.y * offset));
			ends[lane] = world.Cell(x, y);
			if (ends[lane] != State::Open) break;
			probe.depths[lane] = depth;
		}
	}
	auto ordered = probe.depths;
	std::sort(ordered.begin(), ordered.end());
	probe.medianDepth = ordered[1];
	probe.available = body == State::Blocked || (body == State::Open
		&& (ends[1] == State::Blocked || (probe.depths[1] >= settings.openDepth && probe.medianDepth >= settings.openDepth)));
	if (body == State::Blocked || (ends[1] == State::Blocked && probe.depths[1] <= settings.blockedDepth))
		probe.observed = State::Blocked;
	else if (body == State::Open && probe.depths[1] >= settings.openDepth && probe.medianDepth >= settings.openDepth)
		probe.observed = State::Open;
	// Unknown includes the spatial dead band. Never claim an unloaded boundary
	// is open merely because a ray did not find a wall.
	return probe;
}
}
