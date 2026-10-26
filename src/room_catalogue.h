#pragma once

#include "entity_radar.h"
#include "native_rooms.h"
#include "navigation_grid.h"
#include "radar.h"

#include <cstdint>
#include <vector>

// The level's rooms as catalogue entries, so a player with no map can still
// ask where there is left to go.
//
// A sighted player glances at the automap constantly; wall and passage pings
// are not an equivalent channel. Listing the rooms restores the missing
// channel without handing over anything the map does not show: an entry is a
// room and its number, never what is inside it. Contents stay with the
// scanners that already report them, and only once the player is there.
//
// "Explored" here means the player has stood in the room. That is deliberately
// less than the automap knows -- the game also reveals rooms the player only
// walks past -- and it is honest about what it is.
//
// The record is kept for as long as the level it describes is the same level.
// MEASURED: a random level's layout is a pure function of (map seed, level id),
// and the map seed lives in the character's save file and never changes, so a
// character's dungeon is the same dungeon in every game. The record therefore
// survives leaving the level, and survives the game ending; it is dropped only
// when the level is genuinely a different one. Derivation and evidence:
// `re/dungeon-randomization.md` in the workspace.
//
// Two things say "different level", and neither is the game session:
//   * the map seed, when it can be read (NativeDrlg::MapSeed);
//   * the room rectangles themselves, always. A regenerated level lays its
//     rooms out differently, and that is directly observable rather than
//     inferred, so the record stays correct even where the seed is unreadable
//     or genuinely changes per game, as online.
//
// Pure logic over rooms and a position: no native calls, no SDK, unit-tested
// without the game.
namespace D2RAccess::RoomCatalogue {

struct Entry {
	NativeRooms::Room room;
	int               number = 0;     // 1-based, stable for the layout
	bool              explored = false;
	Radar::Position   centre;         // subtiles, for bearing and ordering
};

// How many levels keep their exploration while the player is somewhere else.
// An act is well under this, so a normal session never forgets one it is still
// using; the bound only stops an unbounded walk from growing without limit.
inline constexpr size_t MaxRemembered = 64;

class Registry {
public:
	// `rooms` must be NativeRooms::Read order -- sorted, so numbering is the
	// same on every visit. `mapSeed` is NativeDrlg::MapSeed, or 0 when it
	// could not be read: it sharpens the identity but is never required, and
	// 0 is not treated as "forget everything".
	void Observe(int level, uint32_t mapSeed, const std::vector<NativeRooms::Room>& rooms, Radar::Position player);
	// Forgets the current level *and* everything remembered about the others.
	void Clear();

	const std::vector<Entry>& Entries() const { return m_entries; }
	int                       Level() const { return m_level; }
	uint32_t                  MapSeed() const { return m_seed; }
	// Distinct levels whose exploration is being carried; diagnostics only.
	size_t                    Remembered() const { return m_remembered.size(); }

private:
	// What makes two visits the same level. `layout` is a digest of the room
	// rectangles, so it changes exactly when the rooms do.
	struct Key {
		int      level = 0;
		uint32_t seed = 0;
		uint64_t layout = 0;
		bool operator==(const Key&) const = default;
	};
	struct Record { Key key; std::vector<bool> explored; };

	void Stash();

	Key                 m_key;
	int                 m_level = 0;
	uint32_t            m_seed = 0;
	std::vector<Entry>  m_entries;
	std::vector<Record> m_remembered; // most recently left last
};

// Exploration mode's rooms: those stood in (`visited`, or the entry's own flag)
// and the unexplored ones sharing an edge with `explored` ground that `grid`, when
// given, does not prove walled off -- the edge of the known map; with `everyUnexplored`,
// every room not explored yet. `docs/entity-scan.md`
std::vector<Entry> Exploration(const std::vector<Entry>& entries,
	const std::vector<NativeRooms::Room>& visited, const std::vector<NativeRooms::Room>& explored,
	const Navigation::Grid* grid = nullptr, bool everyUnexplored = false);

// One catalogue entity per room. `explored` and `unexplored` are printf
// formats taking the room number; unexplored rooms get their own category.
// Identity is the room's rectangle origin, so a selection survives the list
// being rebuilt.
std::vector<EntityRadar::Entity> Entities(const std::vector<Entry>& entries,
	const char* explored, const char* unexplored);

}
