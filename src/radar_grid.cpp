#include "radar.h"

#include <cstdlib>

namespace D2RAccess::Radar {
uint16_t TerrainCollisionFlags(uint16_t flags) {
	// 0x341D70 restores MONSTER; 0x363CF0 stamps NO_PATH/PET alongside actors.
	// Only a live unit writes those three, so any cell with a unit bit loses them
	// all. WATER=0xC0 shares PLAYER, so PLAYER survives wherever MISSILE is set.
	constexpr uint16_t Player = 0x80, Monster = 0x100, Missile = 0x40, NoPath = 0x1000, Pet = 0x2000;
	if (flags & (Monster | Player | Pet)) flags &= ~(Monster | NoPath | Pet);
	if ((flags & Player) && !(flags & Missile)) flags &= ~Player;
	return flags;
}
int World::RoomAt(int x, int y) const {
	for (size_t i = 0; i < rooms.size(); ++i) {
		const auto& room = rooms[i];
		const int64_t dx = int64_t(x) - room.x, dy = int64_t(y) - room.y;
		if (dx >= 0 && dy >= 0 && dx < room.width && dy < room.height) return static_cast<int>(i);
	}
	return -1;
}
State World::Cell(int x, int y, Layer layer, uint16_t* rawFlags) const {
	if (rawFlags) *rawFlags = 0;
	const int found = RoomAt(x, y);
	if (found < 0) return State::Unknown;
	const auto& room = rooms[static_cast<size_t>(found)];
	const auto index = static_cast<size_t>((y - room.y) * room.width + (x - room.x));
	if (index >= room.cells.size()) return State::Unknown;
	// D2R 0x366250 returns 0x27 for an absent room. Blank subtiles (0x20)
	// are unknown to the radar, not evidence of a real wall.
	if (room.cells[index] & Collision::Blank) return State::Unknown;
	uint16_t flags = room.cells[index];
	if (rawFlags) *rawFlags = flags;
	if (layer == Layer::Terrain) flags = TerrainCollisionFlags(flags);
	return flags & walkingMask ? State::Blocked : State::Open;
}

State World::Body(int x, int y, Layer layer, uint16_t* flags) const {
	// Exact footprint dispatch recovered from D2R 0x364C70 / 0x366D40:
	// pattern 0 = point; 1/3/5 = five-cell cross; 2/4 = 3x3 square.
	// The out-parameter reports the first blocking cell, or the centre when
	// the whole body is clear.
	uint16_t centre = 0;
	State result = Cell(x, y, layer, &centre);
	if (flags) *flags = centre;
	if (result == State::Blocked) return result;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (bodyPattern == 0 || (dx == 0 && dy == 0)) continue;
			if ((bodyPattern % 2) != 0 && dx != 0 && dy != 0) continue;
			uint16_t neighbour = 0;
			const State next = Cell(x + dx, y + dy, layer, &neighbour);
			if (next == State::Blocked) { if (flags) *flags = neighbour; return next; }
			if (next == State::Unknown) result = next;
		}
	}
	return result;
}

std::vector<Position> LeftPlayerStamps(const World& world) {
	constexpr uint16_t Player = 0x80, Monster = 0x100, Missile = 0x40, NoPath = 0x1000;
	std::vector<Position> centres;
	if (world.bodyPattern != 1) return centres;
	const int px = static_cast<int>(world.player.x), py = static_cast<int>(world.player.y);
	const auto own = [&](int x, int y) { return std::abs(x - px) + std::abs(y - py) <= 1; };
	const auto stamped = [&](int x, int y, uint16_t required) {
		uint16_t flags = 0;
		if (world.Cell(x, y, Layer::Movement, &flags) == State::Unknown || (flags & (Monster | Missile))) return false;
		return (flags & required) == required || (required == Player && own(x, y));
	};
	for (int cy = -1; cy <= 1; ++cy) {
		for (int cx = -1; cx <= 1; ++cx) {
			const int x = px + cx, y = py + cy;
			if ((cx || cy) && stamped(x, y, Player | NoPath) && stamped(x - 1, y, Player) && stamped(x + 1, y, Player)
				&& stamped(x, y - 1, Player) && stamped(x, y + 1, Player)) centres.push_back({static_cast<double>(x), static_cast<double>(y)});
		}
	}
	return centres;
}
}
