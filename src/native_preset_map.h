#pragma once
#include <D2RLPlugin/api.h>
#include <cstdint>
#include <string>

// The LvlPrest map (one DS1 file) a room was cut from, and where it lies.
// Guarded reads only; nothing is retained. docs/places.md#the-map-under-the-player
namespace D2RAccess::NativePresetMap {

struct Map {
	int32_t     def = 0;  // LvlPrest Def
	int32_t     file = 0; // the File column the generator picked, 0-based
	std::string path;     // that column as the game keeps it, e.g. "Act2/Town/LutW.ds1"
	int         tileX = 0, tileY = 0, tileWidth = 0, tileHeight = 0; // RoomEx tile space
};

// How far a read got, for the log.
struct Trace {
	const char* stage = "unavailable";
	int32_t     type = 0; // RoomEx+0x74 as read
	std::string path;     // the File column as read, when it is not a .ds1 path
};

// Verifies preset_map_layout.h's fingerprints once; the reader stays off otherwise.
bool Initialize(const D2RL::PluginContext* context);
bool Available();
// The map of the active room `activeRoom` (a Room1); false for an outdoor
// filler cell, which no map owns, or when anything is unreadable.
bool Read(uintptr_t activeRoom, Map& output, Trace* trace = nullptr);
}
