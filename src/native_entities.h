#pragma once
#include "corpse_memory.h"
#include "entity_radar.h"
#include "native_level_exits.h"
#include <D2RLPlugin/api.h>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Live monsters / NPCs / other players / world objects in the local player's room and its
// neighbours, with the game's own localized names. Guarded reads only; the
// native name resolver is never called because it allocates.
namespace D2RAccess::NativeEntities {
bool Initialize(const D2RL::PluginContext* context);
// How many gems, runes or jewels sit in an item's sockets; unknown when unverified or unreadable.
std::optional<uint32_t> FilledSockets(uintptr_t unit);
// Distant presets are opt-in for the directional scan/beacon. Proximity speech
// and spatial loops keep the nearby, live-unit-only snapshot.
//
// `levelBearings` is a second, narrower opt-in: the boundaries with the areas
// of the act that touch this one (native_level_areas.h). Those are LEVEL-WIDE
// by nature -- the Blood Moor is 77 steps from the middle of the encampment --
// so they belong to the catalogue and to a beacon that has locked one, and not
// to the short-range directional scan, which answers "what is near me".
//
// `wholeLevel` reads every loaded room of the player's level, not only the
// player's room and its neighbours: the scanner's reach, and the reach of what
// follows a thing the scanner named (the user, 2026-09-28).
bool Read(uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output, bool distantExits = false, bool levelBearings = false, bool wholeLevel = false, bool townNpcs = false);
// Ids of the monsters every Read since the last call found dying or dead.
std::vector<uint32_t> TakeDeadMonsters();
// The monster corpses a Necromancer's skills can take that the last Read found
// in the player's room and its neighbours (a tester, 2026-10-14); none for any
// other class or when the rule could not be verified.
struct UsableCorpses {
	bool necromancer = false;
	uint32_t playerId = 0;
	uint64_t readMs = 0; // when, as GetTickCount64 counts: a failed read leaves the last one
	Radar::Position player;
	std::vector<Radar::Position> corpses;
};
UsableCorpses LastUsableCorpses();
// The player's charstats.txt row (7 the Warlock) as the last Read found it; nothing before one.
std::optional<uint32_t> LastPlayerClass();
// Factored for tests over constructed native memory.
// `roster` is the first party roster entry; zero skips distant party members.
bool ReadPlayer(uintptr_t player, uintptr_t tables, uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output, bool distantExits = false, bool levelBearings = false, bool includeCorpse = true, bool includePlayers = true, uintptr_t roster = 0, bool wholeLevel = false, bool townNpcs = false);
// Every party roster entry with the corpse ids the client lists for it; false
// when unreadable or unverified. The second form is factored for tests.
bool ReadRosterCorpses(uint32_t playerId, std::vector<CorpseMemory::Owner>& owners);
bool ReadRosterCorpses(uintptr_t head, uint32_t playerId, std::vector<CorpseMemory::Owner>& owners);
// Every roster entry's town portal ids, readable ones only; false when none or unverified (docs/portals.md).
bool ReadRosterPortals(std::vector<EntityRadar::RosterPortals>& roster);
bool ReadRosterPortals(uintptr_t head, std::vector<EntityRadar::RosterPortals>& roster);
// The current level's room layout around the verified local player, for the
// terrain overview; `neighbours` also reads the bordering rooms of other levels.
bool ReadLayout(uint32_t playerId, bool neighbours, NativeLevelExits::Layout& layout);
// The game's own localized name of a level id; empty when the table is unreadable.
std::string LevelName(uint32_t levelId);
// The NPC presets of the town last read, " row x count" each, for the survey's log.
std::string TownPresets();
// A game string key in the language being played, cleaned as entity names are.
std::string KeyText(std::string_view key);
// The patch tripwire: how many objects.txt rows the container predicate selects
// live, against EntityLayout::ContainerRowCount from the offline derivation.
bool CountContainerRows(size_t& rows, size_t& total);
bool CountContainerRowsAt(uintptr_t tables, size_t& rows, size_t& total);
}
