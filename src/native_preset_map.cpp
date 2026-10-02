#include "native_preset_map.h"
#include "async_log.h"
#include "entity_layout.h"
#include "layout_check.h"
#include "native_memory.h"
#include "preset_map_layout.h"
#include <string_view>

namespace D2RAccess::NativePresetMap {
namespace {
using namespace PresetMapLayout;
using EntityLayout::RoomExtended;
using NativeMemory::Peek;
bool g_available = false;
constexpr std::string_view Ds1Suffix = ".ds1";

bool EndsWithDs1(const std::string& path) {
	if (path.size() <= Ds1Suffix.size()) return false;
	for (size_t i = 0; i < Ds1Suffix.size(); ++i) {
		const char c = path[path.size() - Ds1Suffix.size() + i];
		if ((c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c) != Ds1Suffix[i]) return false;
	}
	return true;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_available = context && LayoutCheck::Verify(context, context->exeBase, Checks, "places", "place names disabled");
	if (g_available) AsyncLog::Info(context, "[places] Verified native preset map reader.");
	return g_available;
}

bool Available() { return g_available; }

bool Read(uintptr_t activeRoom, Map& output, Trace* trace) {
	Trace local;
	Trace& at = trace ? *trace : local;
	at = {};
	if (!g_available) return false;
	at.stage = "room";
	if (!activeRoom) return false;
	const NativeMemory::ReadScope scope;
	uintptr_t room = 0, data = 0, map = 0, record = 0;
	Map       found;
	if (!Peek(activeRoom + RoomExtended, room) || !room) return false;
	at.stage = "type";
	if (!Peek(room + RoomType, at.type) || at.type != PresetRoomType) return false;
	at.stage = "map";
	if (!Peek(room + RoomData, data) || !data || !Peek(data + PresetRoomMap, map) || !map
		|| !Peek(map + MapDef, found.def) || !Peek(map + MapFile, found.file) || !Peek(map + MapRecord, record) || !record
		|| !Peek(map + MapTileX, found.tileX) || !Peek(map + MapTileY, found.tileY)
		|| !Peek(map + MapTileWidth, found.tileWidth) || !Peek(map + MapTileHeight, found.tileHeight)) return false;
	at.stage = "fields";
	if (found.file < 0 || found.file >= MaxFiles || found.tileWidth <= 0 || found.tileHeight <= 0) return false;
	at.stage = "path";
	found.path = NativeMemory::CString(record + RecordFiles + static_cast<size_t>(found.file) * RecordFileStride, RecordFileStride);
	if (!EndsWithDs1(found.path)) { at.path = found.path; return false; }
	at.stage = "ok";
	output = std::move(found);
	return true;
}
}
