#include "native_level_areas.h"
#include "drlg_layout.h"
#include "entity_layout.h"
#include "level_exit_layout.h"
#include "native_memory.h"
#include <algorithm>
#include <unordered_set>

namespace D2RAccess::NativeLevelAreas {
namespace {
using namespace LevelExitLayout;
using DrlgLayout::DrlgFirstLevel, DrlgLayout::LevelDrlg, DrlgLayout::LevelNext;
using EntityLayout::ExtendedLevel, EntityLayout::LevelId, EntityLayout::RoomExtended;
using NativeMemory::Peek;

// An act has far fewer levels than this; the bound only stops a corrupt list
// from being walked forever.
constexpr size_t MaxLevels = 256;

// The union of one level's room rectangles. False means the walk was
// malformed, which drops the level rather than reporting half of it.
bool Footprint(uintptr_t level, Area& area) {
	uintptr_t room = 0;
	if (!Peek(level + LevelFirstRoom, room)) return false;
	std::unordered_set<uintptr_t> seen;
	int minX = 0, minY = 0, maxX = 0, maxY = 0;
	while (room) {
		if (seen.size() >= MaxRooms || !seen.insert(room).second) return false;
		int x = 0, y = 0, width = 0, height = 0;
		if (!Peek(room + RoomTileX, x) || !Peek(room + RoomTileY, y)
			|| !Peek(room + RoomTileWidth, width) || !Peek(room + RoomTileHeight, height)) return false;
		constexpr int MaxTile = UINT16_MAX / SubtilesPerTile;
		if (x < 0 || y < 0 || width <= 0 || height <= 0 || width > MaxTile || height > MaxTile
			|| x > MaxTile - width || y > MaxTile - height) return false;
		if (area.rooms++ == 0) { minX = x; minY = y; maxX = x + width; maxY = y + height; }
		else {
			minX = std::min(minX, x); minY = std::min(minY, y);
			maxX = std::max(maxX, x + width); maxY = std::max(maxY, y + height);
		}
		if (!Peek(room + RoomNext, room)) return false;
	}
	area.tileX = minX; area.tileY = minY;
	area.tileWidth = maxX - minX; area.tileHeight = maxY - minY;
	return true;
}
}

bool Read(uintptr_t activeRoom, std::vector<Area>& output) {
	const NativeMemory::ReadScope scope;
	output.clear();
	uintptr_t extended = 0, level = 0, drlg = 0;
	if (!activeRoom || !Peek(activeRoom + RoomExtended, extended) || !extended
		|| !Peek(extended + ExtendedLevel, level) || !level
		|| !Peek(level + LevelDrlg, drlg) || !drlg
		|| !Peek(drlg + DrlgFirstLevel, level)) return false;
	std::vector<Area> areas;
	std::unordered_set<uintptr_t> seen;
	while (level) {
		if (seen.size() >= MaxLevels || !seen.insert(level).second) return false;
		// The list is the act's, so every entry must point back at the DRLG it
		// was walked from. A level that does not is not this act's, and the
		// walk has left the structure it started in.
		uintptr_t owner = 0;
		Area area;
		if (!Peek(level + LevelDrlg, owner) || owner != drlg
			|| !Peek(level + LevelId, area.level)) return false;
		if (!Footprint(level, area)) return false;
		if (area.rooms) areas.push_back(area);
		if (!Peek(level + LevelNext, level)) return false;
	}
	output = std::move(areas);
	return true;
}

}
