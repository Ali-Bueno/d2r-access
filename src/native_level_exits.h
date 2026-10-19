#pragma once
#include "radar.h"
#include <cstdint>
#include <vector>

namespace D2RAccess::NativeLevelExits {
struct Exit { uint32_t warp = 0, destination = 0, source = 0; Radar::Position position; };
// Walk existing logical rooms in the current level, including inactive rooms.
// No native generator, room loader, map reveal or memory write is involved.
// Empty/unavailable presets are skipped. A malformed traversal returns no exits.
bool Read(uintptr_t activeRoom, std::vector<Exit>& output);
// Where the level's presets spawn a MonStats row, from the same room walk.
// `room` is the logical room (RoomEx) holding the preset.
struct MonsterPreset { uint32_t monster = 0; Radar::Position position; uintptr_t room = 0; std::vector<Radar::Position> alternatives; };
// `complete`, when requested, reports tolerated unbuilt-preset failures too.
// Existing callers still receive the useful built subset on such a failure.
bool ReadMonsters(uintptr_t activeRoom, std::vector<MonsterPreset>& output, bool unbuilt = false, bool* complete = nullptr);
// Quest NPCs placed by object initialization, rather than monster presets.
// Kurast's Hratli end marker takes precedence over his arrival marker; live
// units always supersede these approach locations in NativeEntities.
bool ReadNpcMarkers(uintptr_t activeRoom, std::vector<MonsterPreset>& output, bool* complete = nullptr);
// Coordinate identity survives native warp-unit unloading/recreation. Zero is
// reserved for entities that are not fixed level exits.
uint64_t Identity(Radar::Position position);

// A logical room's tile rectangle (RoomEx +0x60..+0x6C).
struct TileRect { int x = 0, y = 0, width = 0, height = 0; };
// A logical room and whether the game currently keeps it streamed.
struct LevelRoom { TileRect rect; bool active = false; };
// A room of another, already generated level, found through the near lists.
struct NeighbourRoom { TileRect rect; uint32_t level = 0; };
// The shape of the level the player stands in, as the automap knows it.
struct Layout {
	uint32_t level = 0;
	TileRect rect; // the level's own rect (Level+0x24..0x30)
	std::vector<LevelRoom> rooms;
	std::vector<NeighbourRoom> neighbours; // empty unless `neighbours` was asked for
};
// The current level's rooms, and optionally the bordering rooms of other
// levels, for the terrain overview. Maze levels included.
bool ReadLayout(uintptr_t activeRoom, bool neighbours, Layout& output);
}
