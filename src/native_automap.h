#pragma once
#include "automap_cells.h"
#include "native_rooms.h"
#include <D2RLPlugin/api.h>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// What the stock automap has drawn for the level the local player is in, read
// from the game's own automap layer. Read-only: no automap function is called,
// so reading can neither reveal nor save anything. See docs/automap-reader.md.
namespace D2RAccess::NativeAutomap {

// Verifies automap_layout.h's fingerprints once; the reader stays off otherwise.
bool Initialize(const D2RL::PluginContext* context);
bool Available();

enum class Status : uint8_t { Unavailable, NoPlayer, NoLevel, NoLayer, NotSwitched, Unreadable, Ok };

// objects.txt rows drawn with one AutoMap cell: the first row's name key and how many share it.
struct ObjectRows { std::string name; size_t rows = 0; };

struct Snapshot {
	Status   status = Status::Unavailable;
	uint32_t level = 0;
	int32_t  expectedLayer = -1; // LevelDefs[level].Layer
	uint8_t  revealAll = 0;      // nonzero: the reveal cheat bypasses discovery
	AutomapCells::Layer             layer;
	std::vector<AutomapCells::Cell> cells;
	std::vector<NativeRooms::Room>  rooms; // the current level's rooms, read with the cells
	std::unordered_map<int16_t, ObjectRows> objectRows;
};

// Game thread only: the layer switch frees every node. `cells` walks the four
// trees; otherwise only the layer and tree headers are read.
Snapshot Read(bool cells);
const char* StatusName(Status status);

// Of `rooms` (the current level's), those the automap has drawn in; nothing when
// the automap cannot be read. Game thread only; cached until the floors or walls grow.
std::optional<std::vector<NativeRooms::Room>> RevealedRooms(const std::vector<NativeRooms::Room>& rooms);

}
