#include "native_automap.h"
#include "async_log.h"
#include "automap_layout.h"
#include "entity_layout.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "native_ground_items.h"
#include "native_memory.h"
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
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "automap", "automap reader disabled")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[automap] Verified native automap reader.");
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
