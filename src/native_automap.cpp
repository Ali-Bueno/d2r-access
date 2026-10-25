#include "native_automap.h"
#include "async_log.h"
#include "automap_layout.h"
#include "entity_layout.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "native_ground_items.h"
#include "native_memory.h"
#include "level_exit_layout.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_set>

namespace D2RAccess::NativeAutomap {
namespace {
using namespace AutomapLayout;
using EntityLayout::RoomExtended, EntityLayout::ExtendedLevel, EntityLayout::LevelId, EntityLayout::PathRoom,
	EntityLayout::Objects, EntityLayout::ObjectCount, EntityLayout::ObjectStride, EntityLayout::ObjNameKey,
	EntityLayout::MaxKeyBytes;
using GroundItemLayout::UnitType, GroundItemLayout::UnitPath, GroundItemLayout::PlayerType, GroundItemLayout::MaxTableRows;
using NativeMemory::Peek;
uintptr_t g_base = 0;
constexpr AutomapCells::Limits WalkLimits{MaxNodes, MaxDepth};

// What RevealedRooms last derived, and the tree counts it was derived from.
struct RevealCache {
	bool     valid = false;
	uint32_t level = 0;
	int32_t  layer = -1;
	uint64_t floors = 0, walls = 0;
	size_t   rooms = 0;
	std::vector<NativeRooms::Room> revealed;
};
RevealCache g_reveal;

// The level the local player stands in, through the same Room1 -> RoomEx ->
// Level chain the entity reader walks.
bool PlayerLevel(uintptr_t player, uintptr_t& room, uint32_t& level) {
	uintptr_t path = 0, extended = 0, structure = 0;
	uint32_t  type = 0;
	return player && Peek(player + UnitType, type) && type == PlayerType && Peek(player + UnitPath, path) && path
		&& Peek(path + PathRoom, room) && room && Peek(room + RoomExtended, extended) && extended
		&& Peek(extended + ExtendedLevel, structure) && structure && Peek(structure + LevelId, level);
}

// LevelDefs[level].Layer, validated against the 100 save slots a layer can have.
bool ExpectedLayer(uintptr_t tables, uint32_t level, int32_t& layer) {
	uintptr_t rows = 0;
	uint64_t  count = 0;
	return Peek(tables + LevelDefs, rows) && rows && Peek(tables + LevelDefCount, count) && count <= MaxTableRows
		&& level < count && Peek(rows + level * LevelDefStride + LevelDefLayer, layer) && layer >= 0 && layer < LayerSlots;
}

// DrawableChecks passed: the AutoMap table and the rooms' tile lists may be read.
bool g_drawable = false;
const D2RL::PluginContext* g_context = nullptr;
// One level type's AutoMap rows, read once: the table is loaded with the game's data.
struct TypeCache { uintptr_t table = 0; int32_t type = -1; std::vector<AutomapCells::MapRow> rows; };
TypeCache g_typeRows;

// LevelDefs[level].LevelType, the AutoMap.txt key.
bool LevelType(uintptr_t tables, uint32_t level, int32_t& type) {
	uintptr_t rows = 0;
	uint64_t  count = 0;
	return Peek(tables + LevelDefs, rows) && rows && Peek(tables + LevelDefCount, count) && count <= MaxTableRows
		&& level < count && Peek(rows + level * LevelDefStride + LevelDefType, type);
}

// The rows 0x32BDF0 searches for a level type; none for type 0 or a type without rows.
const std::vector<AutomapCells::MapRow>* TypeRows(int32_t type) {
	uintptr_t table = 0;
	uint64_t  count = 0;
	if (!Peek(g_base + MapRows, table) || !Peek(g_base + MapRowCount, count) || count > MaxTableRows) return nullptr;
	if (g_typeRows.table == table && g_typeRows.type == type) return &g_typeRows.rows;
	std::array<int32_t, 2> range{-1, -1};
	if (type > 0 && (type >= MapLevelTypes || !NativeMemory::Copy(g_base + MapRanges + type * sizeof range, range.data(), sizeof range)))
		return nullptr;
	std::vector<AutomapCells::MapRow> rows;
	if (range[0] >= 0) {
		if (!table || range[1] < range[0] || static_cast<uint64_t>(range[1]) > count
			|| static_cast<uint64_t>(range[1] - range[0]) > MaxTypeRows) return nullptr;
		std::vector<uint8_t> raw(static_cast<size_t>(range[1] - range[0]) * MapRowStride);
		if (!raw.empty() && !NativeMemory::Copy(table + range[0] * MapRowStride, raw.data(), raw.size())) return nullptr;
		for (size_t at = 0; at < raw.size(); at += MapRowStride) {
			AutomapCells::MapRow row;
			std::memcpy(&row.orientation, &raw[at + MapRowOrientation], sizeof row.orientation);
			row.style = raw[at + MapRowStyle];
			row.firstSequence = raw[at + MapRowFirstSequence];
			row.lastSequence = raw[at + MapRowLastSequence];
			std::memcpy(&row.firstCel, &raw[at + MapRowCel], sizeof row.firstCel);
			std::memcpy(&row.cels, &raw[at + MapRowCels], sizeof row.cels);
			rows.push_back(row);
		}
	}
	g_typeRows = {table, type, std::move(rows)};
	// Once per level type: the live table against docs/automap-reader.md's expected ranges.
	if (g_context) AsyncLog::InfoF(g_context, "[automap] level type %d: AutoMap rows %d..%d of %llu", type, range[0], range[1],
		static_cast<unsigned long long>(count));
	return &g_typeRows.rows;
}

// The tiles of one list (walls or floors, at `list` with its count at `size`) that get a cell
// once revealed, where the cell decodes (TilePixel, then PixelTile).
bool AppendDrawable(uintptr_t list, uintptr_t size, bool floors, NativeAutomap::RoomTiles& room,
	const std::vector<AutomapCells::MapRow>& rows) {
	uintptr_t tiles = 0;
	uint64_t  count = 0;
	if (!Peek(list, tiles) || !Peek(size, count) || count > MaxRoomTiles) return false;
	if (count == 0) return true;
	if (!tiles) return false;
	std::vector<uint8_t> raw(static_cast<size_t>(count) * TileStride);
	if (!NativeMemory::Copy(tiles, raw.data(), raw.size())) return false;
	for (size_t at = 0; at < raw.size(); at += TileStride) {
		int32_t x = 0, y = 0, kind = 0;
		uint32_t flags = 0;
		uintptr_t library = 0;
		std::memcpy(&x, &raw[at + TileX], sizeof x);
		std::memcpy(&y, &raw[at + TileY], sizeof y);
		std::memcpy(&flags, &raw[at + TileFlags], sizeof flags);
		std::memcpy(&library, &raw[at + TileLibrary], sizeof library);
		std::memcpy(&kind, &raw[at + TileType], sizeof kind);
		if ((flags & TileHidden) || !library) continue;
		std::array<uint32_t, 3> keys{}; // orientation, style, sequence
		if (!NativeMemory::Copy(library + LibraryKeys, keys.data(), sizeof keys)) return false;
		const uint32_t layer = (flags >> TileLayerShift) & TileLayerMask;
		if (floors && (keys[0] != FloorOrientation || layer < FloorLayerFirst || layer > FloorLayerLast)) continue;
		if (!AutomapCells::HasCell(rows, keys[0], keys[1], keys[2])) continue;
		AutomapCells::Point key;
		if (AutomapCells::PixelTile(AutomapCells::TilePixel(room.tileX + x, room.tileY + y, kind), key.x, key.y)) room.tiles.push_back(key);
	}
	return true;
}

void NameObjectCells(uintptr_t tables, Snapshot& snapshot) {
	std::unordered_set<int16_t> wanted;
	for (const auto& cell : snapshot.cells)
		if (cell.tree == AutomapCells::Tree::Object || cell.tree == AutomapCells::Tree::Extra) wanted.insert(cell.number);
	uintptr_t rows = 0;
	uint64_t  count = 0;
	if (wanted.empty() || !Peek(tables + Objects, rows) || !rows || !Peek(tables + ObjectCount, count) || count > MaxTableRows) return;
	for (uint64_t i = 0; i < count; ++i) {
		const uintptr_t row = rows + i * ObjectStride;
		uint32_t cell = 0;
		if (!Peek(row + ObjectAutoMap, cell) || cell == 0 || cell > INT16_MAX || !wanted.contains(static_cast<int16_t>(cell))) continue;
		auto& entry = snapshot.objectRows[static_cast<int16_t>(cell)];
		if (entry.rows++ == 0) entry.name = NativeMemory::CString(row + ObjNameKey, MaxKeyBytes);
	}
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	g_reveal = {};
	g_drawable = false;
	g_typeRows = {};
	g_context = context;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "automap", "automap reader disabled")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[automap] Verified native automap reader.");
	g_drawable = LayoutCheck::Verify(context, context->exeBase, DrawableChecks, "automap", "drawable tiles unread, the explored share counts rooms");
	if (g_drawable) AsyncLog::Info(context, "[automap] Verified drawable tile reader.");
	return true;
}

bool Available() { return g_base != 0; }

Snapshot Read(bool cells) {
	Snapshot snapshot;
	if (!g_base) return snapshot;
	const NativeMemory::ReadScope scope;
	uintptr_t player = 0, tables = 0, room = 0, layer = 0;
	snapshot.status = Status::NoPlayer;
	if (!NativeGroundItems::RuntimeRoots(player, tables)) return snapshot;
	snapshot.status = Status::NoLevel;
	if (!PlayerLevel(player, room, snapshot.level) || !ExpectedLayer(tables, snapshot.level, snapshot.expectedLayer)) return snapshot;
	(void)Peek(g_base + RevealAll, snapshot.revealAll);
	snapshot.status = Status::Unreadable;
	if (!Peek(g_base + CurrentLayer, layer)) return snapshot;
	switch (AutomapCells::ReadLayer(NativeMemory::Copy, layer, snapshot.expectedLayer, cells, WalkLimits, snapshot.layer, snapshot.cells)) {
	case AutomapCells::LayerStatus::NoLayer: snapshot.status = Status::NoLayer; return snapshot;
	case AutomapCells::LayerStatus::Mismatch: snapshot.status = Status::NotSwitched; return snapshot;
	case AutomapCells::LayerStatus::Unreadable: return snapshot;
	case AutomapCells::LayerStatus::Ok: break;
	}
	snapshot.status = Status::Ok;
	if (cells) {
		// Without the rooms every cell still reads; only the level filter is lost.
		if (!NativeRooms::Read(room, snapshot.rooms)) snapshot.rooms.clear();
		NameObjectCells(tables, snapshot);
	}
	return snapshot;
}

std::optional<std::vector<NativeRooms::Room>> RevealedRooms(const std::vector<NativeRooms::Room>& rooms) {
	const auto tiles = [](const Snapshot& s) {
		return s.layer.trees[0] == AutomapCells::TreeStatus::Ok && s.layer.trees[1] == AutomapCells::TreeStatus::Ok;
	};
	const Snapshot head = Read(false);
	if (head.status != Status::Ok || !tiles(head)) return std::nullopt;
	const auto& counts = head.layer.counts;
	if (g_reveal.valid && g_reveal.level == head.level && g_reveal.layer == head.layer.number
		&& g_reveal.floors == counts[0] && g_reveal.walls == counts[1] && g_reveal.rooms == rooms.size()) return g_reveal.revealed;
	const Snapshot full = Read(true);
	if (full.status != Status::Ok || !tiles(full)) return std::nullopt;
	g_reveal = {true, full.level, full.layer.number, full.layer.counts[0], full.layer.counts[1], rooms.size(),
		AutomapCells::RevealedRooms(full.cells, rooms)};
	return g_reveal.revealed;
}

std::optional<std::vector<size_t>> DrawnTiles(const std::vector<std::vector<AutomapCells::Point>>& rooms) {
	const Snapshot full = Read(true);
	if (full.status != Status::Ok || full.layer.trees[0] != AutomapCells::TreeStatus::Ok || full.layer.trees[1] != AutomapCells::TreeStatus::Ok)
		return std::nullopt;
	return AutomapCells::DrawnTiles(full.cells, rooms);
}

std::optional<RoomTiles> DrawableTiles(uintptr_t room, const std::function<bool(const RoomTiles&)>& wanted) {
	if (!g_drawable || !room) return std::nullopt;
	const NativeMemory::ReadScope scope;
	uintptr_t player = 0, tables = 0, extended = 0, structure = 0, block = 0;
	RoomTiles result;
	int32_t type = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !Peek(room + RoomExtended, extended) || !extended
		|| !Peek(extended + ExtendedLevel, structure) || !structure || !Peek(structure + LevelId, result.level)
		|| !Peek(extended + LevelExitLayout::RoomTileX, result.tileX) || !Peek(extended + LevelExitLayout::RoomTileY, result.tileY)
		|| (wanted && !wanted(result)) || !LevelType(tables, result.level, type) || !Peek(room + RoomTileBlock, block) || !block)
		return std::nullopt;
	const auto* rows = TypeRows(type);
	if (!rows || !AppendDrawable(block + BlockWalls, block + BlockWallCount, false, result, *rows)
		|| !AppendDrawable(block + BlockFloors, block + BlockFloorCount, true, result, *rows)) return std::nullopt;
	// A floor and a wall on one tile share its place.
	std::sort(result.tiles.begin(), result.tiles.end(), [](const auto& a, const auto& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
	result.tiles.erase(std::unique(result.tiles.begin(), result.tiles.end(), [](const auto& a, const auto& b) { return a.x == b.x && a.y == b.y; }),
		result.tiles.end());
	return result;
}

const char* StatusName(Status status) {
	switch (status) {
	case Status::Unavailable: return "unavailable";
	case Status::NoPlayer: return "no-player";
	case Status::NoLevel: return "no-level";
	case Status::NoLayer: return "no-layer";
	case Status::NotSwitched: return "not-switched";
	case Status::Unreadable: return "unreadable";
	case Status::Ok: return "ok";
	}
	return "?";
}

}
