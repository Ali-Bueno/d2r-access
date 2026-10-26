#pragma once

#include "entity_radar.h"
#include "radar.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Town NPCs listed from anywhere in town: the loaded unit where there is one,
// else a stand-in at its spawn point from the level's presets. Pure logic over
// what the native reader found. docs/entity-scan.md#town-npcs
namespace D2RAccess::TownNpcs {
// Where the level's presets spawn a MonStats row, and whether that spot lies in
// a room whose units the client holds around the player.
struct Spawn { uint32_t monster = 0; Radar::Position position; bool aroundPlayer = false; };
// A loaded NPC unit: its index among the read entities, and its MonStats row.
using Live = std::pair<size_t, uint32_t>;
// The NPC name of a MonStats row; empty for a row that is no NPC.
using NameOf = std::function<std::string(uint32_t)>;

// One identity for a row's live unit and its stand-in: the row + 1, as zero means none.
uint64_t Identity(uint32_t monster);

// A marker object preset (objects.txt row) and whether its room is around the player.
struct Marker { uint32_t object = 0; Radar::Position position; bool aroundPlayer = false; };
// Whether the game's InitFn of a marker spawns its NPC now; nothing when that cannot be told.
using SpawnTest = std::function<std::optional<bool>(int initFn)>;

// The spawns of the NPCs placed by marker objects (Generated::NpcMarkers) rather than by
// monster presets: one for each marker whose test passes; none for one that cannot be told.
std::vector<Spawn> MarkerSpawns(const std::vector<Marker>& markers, const SpawnTest& spawns);

// Gives the live units of every NPC row the presets place exactly once, and the only
// loaded unit of an NPC row they never place, that row's identity. Appends a stand-in
// for each placed row with no live unit. A spawn around the player without its unit
// has none: the NPC is not there.
void Merge(const std::vector<Spawn>& spawns, const std::vector<Live>& live, const NameOf& name,
	std::vector<EntityRadar::Entity>& entities);
}
