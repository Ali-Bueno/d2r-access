#include "native_level_exits.h"
#include "level_exit_layout.h"
#include "entity_layout.h"
#include "native_memory.h"
#include "preset_map_layout.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace D2RAccess::NativeLevelExits {
namespace {
using namespace LevelExitLayout;
using namespace EntityLayout;
using NativeMemory::Peek;

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

namespace {
// A preset's level subtile position; false when it lies outside its own room.
bool InRoom(uintptr_t preset, const TileRect& rect, Radar::Position& position) {
	int px = 0, py = 0;
	if (!Peek(preset + PresetX, px) || !Peek(preset + PresetY, py) || px < 0 || py < 0
		|| px >= rect.width * SubtilesPerTile || py >= rect.height * SubtilesPerTile) return false;
	position = {static_cast<double>(rect.x * SubtilesPerTile + px), static_cast<double>(rect.y * SubtilesPerTile + py)};
	return true;
}

// Every logical room of the player's level, inactive ones included, and each
// preset in it. False when the traversal is malformed.
template<class Visit, class VisitRoom> bool WalkPresets(uintptr_t activeRoom, Visit visit, VisitRoom visitRoom) {
	uintptr_t room = 0, level = 0;
	uint32_t currentLevel = 0;
	if (!activeRoom || !Peek(activeRoom + RoomExtended, room) || !room
		|| !Peek(room + ExtendedLevel, level) || !level || !Peek(level + LevelId, currentLevel)
		|| !Peek(level + LevelFirstRoom, room)) return false;
	std::unordered_set<uintptr_t> rooms, presets;
	while (room) {
		if (rooms.size() >= MaxRooms || !rooms.insert(room).second) return false;
		uintptr_t owner = 0, preset = 0;
		TileRect rect;
		if (!Peek(room + ExtendedLevel, owner) || owner != level || !Peek(room + RoomPresets, preset)
			|| !ReadRect(room, rect)) return false;
		visitRoom(room, rect);
		while (preset) {
			if (presets.size() >= MaxPresets || !presets.insert(preset).second) return false;
			uint32_t type = 0;
			if (!Peek(preset + PresetType, type)) return false;
			visit(room, rect, currentLevel, preset, type);
			if (!Peek(preset + PresetNext, preset)) return false;
		}
		if (!Peek(room + RoomNext, room)) return false;
	}
	return true;
}

// The map a preset room was cut from; zero for any other room.
uintptr_t MapOf(uintptr_t room) {
	int32_t type = 0;
	uintptr_t data = 0, map = 0;
	return Peek(room + PresetMapLayout::RoomType, type) && type == PresetMapLayout::PresetRoomType
		&& Peek(room + PresetMapLayout::RoomData, data) && data
		&& Peek(data + PresetMapLayout::PresetRoomMap, map) ? map : 0;
}

using Room = std::pair<uintptr_t, TileRect>;

// The monster presets `map` still holds, already in level subtiles. Each goes to
// the room whose rectangle holds it, as building that room will (0x3DE420).
bool ReadUnbuilt(uintptr_t map, const std::vector<Room>& rooms, std::vector<MonsterPreset>& output) {
	std::unordered_set<uintptr_t> presets;
	uintptr_t preset = 0;
	if (!Peek(map + PresetMapLayout::MapPendingPresets, preset)) return false;
	while (preset) {
		if (presets.size() >= MaxPresets || !presets.insert(preset).second) return false;
		uint32_t type = 0, monster = 0;
		int x = 0, y = 0;
		if (!Peek(preset + PresetType, type)) return false;
		if (type == MonsterType && Peek(preset + PresetClass, monster) && Peek(preset + PresetX, x) && Peek(preset + PresetY, y)) {
			for (const auto& [room, rect] : rooms)
				if (x >= rect.x * SubtilesPerTile && y >= rect.y * SubtilesPerTile
					&& x < (rect.x + rect.width) * SubtilesPerTile && y < (rect.y + rect.height) * SubtilesPerTile) {
					output.push_back({monster, {static_cast<double>(x), static_cast<double>(y)}, room});
					break;
				}
		}
		if (!Peek(preset + PresetNext, preset)) return false;
	}
	return true;
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
		int x = 0, y = 0, width = 0, height = 0;
		if (!Peek(room + ExtendedLevel, owner) || owner != level || !Peek(room + RoomPresets, preset)
			|| !Peek(room + RoomTileX, x) || !Peek(room + RoomTileY, y)
			|| !Peek(room + RoomTileWidth, width) || !Peek(room + RoomTileHeight, height)) return false;
		constexpr int MaxTile = UINT16_MAX / SubtilesPerTile;
		if (x < 0 || y < 0 || width <= 0 || height <= 0 || width > MaxTile || height > MaxTile
			|| x > MaxTile - width || y > MaxTile - height) return false;
		while (preset) {
			if (presets.size() >= MaxPresets || !presets.insert(preset).second) return false;
			uint32_t type = 0, warp = 0;
			int px = 0, py = 0;
			if (!Peek(preset + PresetType, type)) return false;
			if (type == WarpType && Peek(preset + PresetClass, warp)
				&& Peek(preset + PresetX, px) && Peek(preset + PresetY, py)
				&& px >= 0 && py >= 0 && px < width * SubtilesPerTile && py < height * SubtilesPerTile) {
				Exit exit{warp & WarpIdMask, 0, currentLevel};
				if (Destination(room, exit.warp, currentLevel, exit.destination)) {
					exit.position = {static_cast<double>(x * SubtilesPerTile + px), static_cast<double>(y * SubtilesPerTile + py)};
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

bool ReadMonsters(uintptr_t activeRoom, std::vector<MonsterPreset>& output, bool unbuilt) {
	const NativeMemory::ReadScope scope;
	output.clear();
	std::vector<MonsterPreset> monsters;
	std::vector<Room> rooms;
	std::vector<uintptr_t> maps;
	if (!WalkPresets(activeRoom, [&](uintptr_t room, const TileRect& rect, uint32_t, uintptr_t preset, uint32_t type) {
		MonsterPreset monster{0, {}, room};
		if (type == MonsterType && Peek(preset + PresetClass, monster.monster) && InRoom(preset, rect, monster.position))
			monsters.push_back(monster);
	}, [&](uintptr_t room, const TileRect& rect) {
		rooms.push_back({room, rect});
		const uintptr_t map = unbuilt ? MapOf(room) : 0;
		if (map && std::find(maps.begin(), maps.end(), map) == maps.end()) maps.push_back(map);
	})) return false;
	std::vector<MonsterPreset> pending;
	for (const uintptr_t map : maps)
		if (!ReadUnbuilt(map, rooms, pending)) { pending.clear(); break; }
	monsters.insert(monsters.end(), pending.begin(), pending.end());
	output = std::move(monsters);
	return true;
}

bool ReadLayout(uintptr_t activeRoom, bool neighbours, Layout& output) {
	const NativeMemory::ReadScope scope;
	output = {};
	uintptr_t room = 0, level = 0;
	Layout layout;
	if (!activeRoom || !Peek(activeRoom + RoomExtended, room) || !room
		|| !Peek(room + ExtendedLevel, level) || !level || !Peek(level + LevelId, layout.level)
		|| !Peek(level + LevelTileX, layout.rect.x) || !Peek(level + LevelTileY, layout.rect.y)
		|| !Peek(level + LevelTileWidth, layout.rect.width) || !Peek(level + LevelTileHeight, layout.rect.height)
		|| !Peek(level + LevelFirstRoom, room)) return false;
	std::unordered_set<uintptr_t> rooms;
	while (room) {
		if (rooms.size() >= MaxRooms || !rooms.insert(room).second) return false;
		uintptr_t owner = 0, active = 0;
		TileRect rect;
		if (!Peek(room + ExtendedLevel, owner) || owner != level || !ReadRect(room, rect)
			|| !Peek(room + RoomActive, active)) return false;
		layout.rooms.push_back({rect, active != 0});
		if (neighbours) {
			uintptr_t list = 0;
			uint64_t count = 0;
			if (!Peek(room + RoomNearCount, count) || count > MaxRooms) return false;
			if (count && (!Peek(room + RoomNear, list) || !list)) return false;
			for (uint64_t i = 0; i < count; ++i) {
				uintptr_t nearRoom = 0, nearLevel = 0;
				NeighbourRoom neighbour;
				if (!Peek(list + i * sizeof(uintptr_t), nearRoom) || !nearRoom) return false;
				if (!Peek(nearRoom + ExtendedLevel, nearLevel) || !nearLevel || nearLevel == level
					|| !Peek(nearLevel + LevelId, neighbour.level) || neighbour.level == layout.level
					|| !ReadRect(nearRoom, neighbour.rect)) continue;
				layout.neighbours.push_back(neighbour);
			}
		}
		if (!Peek(room + RoomNext, room)) return false;
	}
	if (layout.rooms.empty()) return false;
	output = std::move(layout);
	return true;
}
}
