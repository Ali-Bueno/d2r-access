#include "native_level_exits.h"
#include "level_exit_layout.h"
#include "entity_layout.h"
#include "native_memory.h"
#include "generated/level_flags.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace D2RAccess::NativeLevelExits {
namespace {
using namespace LevelExitLayout;
using namespace EntityLayout;
using NativeMemory::Peek;
// Levels.txt DrlgType 1 is a maze of LvlPrest cells: every way out of it is a
// real warp, so its rooms never border another level on open ground.
constexpr int32_t MazeDrlgType = 1;

// A logical room's tile rectangle, bounded so the subtile conversion cannot overflow.
bool ReadRect(uintptr_t room, TileRect& rect) {
	constexpr int MaxTile = UINT16_MAX / SubtilesPerTile;
	return Peek(room + RoomTileX, rect.x) && Peek(room + RoomTileY, rect.y)
		&& Peek(room + RoomTileWidth, rect.width) && Peek(room + RoomTileHeight, rect.height)
		&& rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0
		&& rect.width <= MaxTile && rect.height <= MaxTile
		&& rect.x <= MaxTile - rect.width && rect.y <= MaxTile - rect.height;
}

bool Destination(uintptr_t room, uint32_t warp, uint32_t currentLevel, uint32_t& destination) {
	uintptr_t node = 0;
	if (!Peek(room + RoomWarpNodes, node)) return false;
	for (size_t i = 0; node && i < MaxWarpNodes; ++i) {
		uintptr_t row = 0;
		uint32_t id = 0;
		if (!Peek(node + WarpNodeRow, row) || !row || !Peek(row + LvlWarpId, id)) return false;
		if (id == warp) {
			int32_t active = 0;
			uint32_t noInteract = 0;
			uintptr_t targetRoom = 0, level = 0;
			return Peek(node + WarpNodeActive, active) && active
				&& Peek(row + LvlWarpNoInteract, noInteract) && !noInteract
				&& Peek(node + WarpNodeDestination, targetRoom) && targetRoom
				&& Peek(targetRoom + ExtendedLevel, level) && level
				&& Peek(level + LevelId, destination) && destination && destination != currentLevel;
		}
		if (!Peek(node + WarpNodeNext, node)) return false;
	}
	return false;
}
}

uint64_t Identity(Radar::Position position) {
	if (!std::isfinite(position.x) || !std::isfinite(position.y)
		|| position.x < 0 || position.y < 0 || position.x > UINT16_MAX || position.y > UINT16_MAX) return 0;
	return (static_cast<uint64_t>(position.x) << 16 | static_cast<uint64_t>(position.y)) + 1;
}

bool Read(uintptr_t activeRoom, std::vector<Exit>& output) {
	const NativeMemory::ReadScope scope;
	output.clear();
	uintptr_t room = 0, level = 0;
	uint32_t currentLevel = 0;
	if (!activeRoom || !Peek(activeRoom + RoomExtended, room) || !room
		|| !Peek(room + ExtendedLevel, level) || !level || !Peek(level + LevelId, currentLevel)
		|| !Peek(level + LevelFirstRoom, room)) return false;
	std::vector<Exit> exits;
	std::unordered_set<uintptr_t> rooms, presets;
	while (room) {
		if (rooms.size() >= MaxRooms || !rooms.insert(room).second) return false;
		uintptr_t owner = 0, preset = 0;
		TileRect rect;
		if (!Peek(room + ExtendedLevel, owner) || owner != level || !Peek(room + RoomPresets, preset)
			|| !ReadRect(room, rect)) return false;
		while (preset) {
			if (presets.size() >= MaxPresets || !presets.insert(preset).second) return false;
			uint32_t type = 0, warp = 0;
			int px = 0, py = 0;
			if (!Peek(preset + PresetType, type)) return false;
			if (type == WarpType && Peek(preset + PresetClass, warp)
				&& Peek(preset + PresetX, px) && Peek(preset + PresetY, py)
				&& px >= 0 && py >= 0 && px < rect.width * SubtilesPerTile && py < rect.height * SubtilesPerTile) {
				Exit exit{warp & WarpIdMask};
				if (Destination(room, exit.warp, currentLevel, exit.destination)) {
					exit.position = {static_cast<double>(rect.x * SubtilesPerTile + px), static_cast<double>(rect.y * SubtilesPerTile + py)};
					exits.push_back(exit);
				}
			}
			if (!Peek(preset + PresetNext, preset)) return false;
		}
		if (!Peek(room + RoomNext, room)) return false;
	}
	output = std::move(exits);
	return true;
}

namespace {
// The subtile midpoint of the edge two tile rectangles share; false when they
// only meet at a corner or not at all.
bool SharedEdge(const TileRect& a, const TileRect& b, Radar::Position& midpoint) {
	if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) return false;
	const int ax2 = a.x + a.width, ay2 = a.y + a.height, bx2 = b.x + b.width, by2 = b.y + b.height;
	const int lowX = std::max(a.x, b.x), highX = std::min(ax2, bx2);
	const int lowY = std::max(a.y, b.y), highY = std::min(ay2, by2);
	constexpr double Half = SubtilesPerTile / 2.0;
	if (highY > lowY && (ax2 == b.x || bx2 == a.x)) {
		midpoint = {static_cast<double>((ax2 == b.x ? ax2 : a.x) * SubtilesPerTile), (lowY + highY) * Half};
		return true;
	}
	if (highX > lowX && (ay2 == b.y || by2 == a.y)) {
		midpoint = {(lowX + highX) * Half, static_cast<double>((ay2 == b.y ? ay2 : a.y) * SubtilesPerTile)};
		return true;
	}
	return false;
}
}

std::vector<BorderExit> Borders(const std::vector<BorderPair>& pairs, Radar::Position player) {
	std::vector<BorderExit> exits;
	std::vector<double> distances;
	for (const auto& pair : pairs) {
		Radar::Position midpoint;
		if (!pair.destination || !SharedEdge(pair.room, pair.neighbour, midpoint)) continue;
		const double distance = std::hypot(midpoint.x - player.x, midpoint.y - player.y);
		const auto found = std::find_if(exits.begin(), exits.end(),
			[&](const BorderExit& exit) { return exit.destination == pair.destination; });
		if (found == exits.end()) { exits.push_back({pair.destination, midpoint}); distances.push_back(distance); continue; }
		// Several of this level's rooms can touch the same neighbour: keep the nearest crossing.
		double& best = distances[static_cast<size_t>(found - exits.begin())];
		if (distance < best) { found->position = midpoint; best = distance; }
	}
	return exits;
}

bool ReadBorders(uintptr_t activeRoom, Radar::Position player, std::vector<BorderExit>& output) {
	const NativeMemory::ReadScope scope;
	output.clear();
	uintptr_t room = 0, level = 0;
	uint32_t currentLevel = 0;
	if (!activeRoom || !Peek(activeRoom + RoomExtended, room) || !room
		|| !Peek(room + ExtendedLevel, level) || !level || !Peek(level + LevelId, currentLevel)
		|| Generated::FindLevelFlags(static_cast<int32_t>(currentLevel)).drlgType == MazeDrlgType
		|| !Peek(level + LevelFirstRoom, room)) return false;
	std::vector<BorderPair> pairs;
	std::unordered_set<uintptr_t> rooms;
	while (room) {
		if (rooms.size() >= MaxRooms || !rooms.insert(room).second) return false;
		uintptr_t owner = 0, list = 0;
		uint64_t count = 0;
		TileRect rect;
		if (!Peek(room + ExtendedLevel, owner) || owner != level || !ReadRect(room, rect)
			|| !Peek(room + RoomNearCount, count) || count > MaxRooms) return false;
		// 0x3614C0 allocates before the first store, so a count without a list is corrupt.
		if (count && (!Peek(room + RoomNear, list) || !list)) return false;
		for (uint64_t i = 0; i < count; ++i) {
			uintptr_t neighbour = 0, neighbourLevel = 0;
			uint32_t destination = 0;
			TileRect across;
			if (!Peek(list + i * sizeof(uintptr_t), neighbour) || !neighbour) return false;
			// A near room owned by another level is the far side of an open border.
			if (!Peek(neighbour + ExtendedLevel, neighbourLevel) || !neighbourLevel || neighbourLevel == level
				|| !Peek(neighbourLevel + LevelId, destination) || destination == currentLevel
				|| !ReadRect(neighbour, across)) continue;
			pairs.push_back({rect, across, destination});
		}
		if (!Peek(room + RoomNext, room)) return false;
	}
	output = Borders(pairs, player);
	return true;
}
}
