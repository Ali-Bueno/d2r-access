#include "native_rooms.h"
#include "entity_layout.h"
#include "level_exit_layout.h"
#include "native_memory.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace D2RAccess::NativeRooms {
namespace {
using namespace LevelExitLayout;
using namespace EntityLayout;
using NativeMemory::Peek;
}

Bounds Subtiles(const Room& room) {
	const int minX = room.tileX * SubtilesPerTile, minY = room.tileY * SubtilesPerTile;
	return {minX, minY, minX + room.tileWidth * SubtilesPerTile, minY + room.tileHeight * SubtilesPerTile};
}

bool Contains(const Bounds& bounds, int x, int y) {
	return x >= bounds.minX && x < bounds.maxX && y >= bounds.minY && y < bounds.maxY;
}

uint64_t Identity(int tileX, int tileY) {
	if (tileX < 0 || tileY < 0 || tileX > UINT16_MAX || tileY > UINT16_MAX) return 0;
	return (static_cast<uint64_t>(tileX) << 16 | static_cast<uint64_t>(tileY)) + 1;
}

bool Read(uintptr_t activeRoom, std::vector<Room>& output, int expectedLevel) {
	const NativeMemory::ReadScope scope;
	output.clear();
	uintptr_t room = 0, level = 0;
	int32_t levelId = 0;
	if (!activeRoom || !Peek(activeRoom + RoomExtended, room) || !room
		|| !Peek(room + ExtendedLevel, level) || !level
		|| (expectedLevel && (!Peek(level + LevelId, levelId) || levelId != expectedLevel))
		|| !Peek(level + LevelFirstRoom, room)) return false;
	std::vector<Room>              rooms;
	std::unordered_set<uintptr_t>  seen;
	while (room) {
		if (seen.size() >= MaxRooms || !seen.insert(room).second) return false;
		uintptr_t owner = 0;
		Room      entry;
		if (!Peek(room + ExtendedLevel, owner) || owner != level
			|| !Peek(room + RoomTileX, entry.tileX) || !Peek(room + RoomTileY, entry.tileY)
			|| !Peek(room + RoomTileWidth, entry.tileWidth) || !Peek(room + RoomTileHeight, entry.tileHeight)) return false;
		constexpr int MaxTile = UINT16_MAX / SubtilesPerTile;
		if (entry.tileX < 0 || entry.tileY < 0 || entry.tileWidth <= 0 || entry.tileHeight <= 0
			|| entry.tileWidth > MaxTile || entry.tileHeight > MaxTile
			|| entry.tileX > MaxTile - entry.tileWidth || entry.tileY > MaxTile - entry.tileHeight) return false;
		entry.id = Identity(entry.tileX, entry.tileY);
		if (!entry.id) return false;
		rooms.push_back(entry);
		if (!Peek(room + RoomNext, room)) return false;
	}
	std::sort(rooms.begin(), rooms.end(), [](const Room& a, const Room& b) {
		return a.tileY != b.tileY ? a.tileY < b.tileY : a.tileX < b.tileX;
	});
	output = std::move(rooms);
	return true;
}

size_t Containing(const std::vector<Room>& rooms, Radar::Position subtile) {
	if (!std::isfinite(subtile.x) || !std::isfinite(subtile.y)) return None;
	const int x = static_cast<int>(std::lround(subtile.x)), y = static_cast<int>(std::lround(subtile.y));
	for (size_t i = 0; i < rooms.size(); ++i)
		if (Contains(Subtiles(rooms[i]), x, y)) return i;
	return None;
}

}
