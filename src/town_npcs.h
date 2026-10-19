#pragma once

#include "entity_radar.h"
#include "radar.h"

#include <cstddef>
#include <array>
#include <cstdint>
#include <functional>
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

// The two town actors with verified quest-retirement producers: Natalya's
// object init52 -> class297 and Nihlathak's init68 -> class514. The hostile
// Nihlathak boss526 and ordinary wandering NPCs are separate policies.
inline constexpr std::array<uint32_t, 2> RetiringClasses{297, 514};
inline constexpr bool CanRetire(uint32_t monster) { return monster == RetiringClasses[0] || monster == RetiringClasses[1]; }

// Owned evidence from the same native entity pass, never persistent pointers.
// `complete` permits adding observed absences; a partial pass can still expose
// real live units, which always clear an earlier absence. No quest prediction.
struct Presence {
	uint64_t session = 0;
	uint32_t player = 0, level = 0;
	bool complete = false;
	std::vector<uint64_t> absent;
};

// Gives the live units of every NPC row the presets place exactly once that row's
// identity, and appends a stand-in for each such row with no live unit. A spawn
// around the player without its unit has none: the NPC is not there.
void Merge(const std::vector<Spawn>& spawns, const std::vector<Live>& live, const NameOf& name,
	std::vector<EntityRadar::Entity>& entities);
}
