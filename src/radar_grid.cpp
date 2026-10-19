#include "radar.h"

#include <algorithm>
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
	// The out-parameter reports the first blocking cell, or the centre when
	// the whole body is clear.
	uint16_t centre = 0;
	State result = Cell(x, y, layer, &centre);
	if (flags) *flags = centre;
	if (result == State::Blocked) return result;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (!InBody(bodyPattern, dx, dy)) continue;
			uint16_t neighbour = 0;
			const State next = Cell(x + dx, y + dy, layer, &neighbour);
			if (next == State::Blocked) { if (flags) *flags = neighbour; return next; }
			if (next == State::Unknown) result = next;
		}
	}
	return result;
}

namespace {
constexpr uint16_t Player = 0x80, Monster = 0x100, Missile = 0x40, NoPath = 0x1000, Pet = 0x2000;
constexpr size_t MaxLeftMarkers = 32; // a bound on the work, far past anything seen
}

std::vector<LeftMarker> LeftMarkers(const World& world, std::span<const Position> units, bool beside) {
	std::vector<LeftMarker> left;
	const int px = static_cast<int>(world.player.x), py = static_cast<int>(world.player.y);
	for (size_t index = 0; index < world.rooms.size() && left.size() < MaxLeftMarkers; ++index) {
		const auto& room = world.rooms[index];
		const size_t count = std::min(room.cells.size(), static_cast<size_t>(room.width) * static_cast<size_t>(room.height));
		for (size_t cell = 0; cell < count && left.size() < MaxLeftMarkers; ++cell) {
			const uint16_t flags = room.cells[cell];
			if (!(flags & NoPath) || (flags & (Collision::Blank | Monster | Pet))) continue;
			if ((flags & Player) && (flags & Missile)) continue; // 0xC0 is water, not a player
			const int x = room.x + static_cast<int>(cell % static_cast<size_t>(room.width));
			const int y = room.y + static_cast<int>(cell / static_cast<size_t>(room.width));
			const int dx = std::abs(x - px), dy = std::abs(y - py);
			if ((dx == 0 && dy == 0) || (!beside && dx <= 1 && dy <= 1)) continue;
			if (world.RoomAt(x, y) != static_cast<int>(index)) continue; // another room's copy of the cell is the one read
			bool owned = false;
			for (const auto& unit : units) {
				if (std::abs(x - static_cast<int>(unit.x)) <= 1 && std::abs(y - static_cast<int>(unit.y)) <= 1) { owned = true; break; }
			}
			if (!owned) left.push_back({x, y, flags});
		}
	}
	return left;
}

std::vector<MarkerCell> MarkerCells(const World& world, const LeftMarker& marker) {
	std::vector<MarkerCell> cells{{marker.x, marker.y, static_cast<uint16_t>(marker.flags & (Player | NoPath))}};
	if (!(marker.flags & Player)) return cells;
	const int px = static_cast<int>(world.player.x), py = static_cast<int>(world.player.y);
	constexpr int Arms[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	for (const auto& arm : Arms) {
		const int x = marker.x + arm[0], y = marker.y + arm[1];
		uint16_t flags = 0;
		if (world.Cell(x, y, Layer::Movement, &flags) == State::Unknown) continue;
		// An arm that is another stamp's centre, or water, or under the player's own cross, stays.
		if (!(flags & Player) || (flags & (NoPath | Missile)) || std::abs(x - px) + std::abs(y - py) <= 1) continue;
		cells.push_back({x, y, Player});
	}
	return cells;
}
}
