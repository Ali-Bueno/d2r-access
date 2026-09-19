#pragma once

#include "entity_radar.h"
#include "native_rooms.h"
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
// "Explored" here means the player has stood in the room during this level
// instance. That is deliberately less than the automap knows -- the game also
// reveals rooms the player only walks past -- and it is honest about what it
// is.
//
// The record resets when the level id or the loader's sessionGeneration
// changes, so it survives leaving and re-entering a level within one game (a
// town portal back in keeps it) and is dropped when the game is. Whether
// dropping it is CORRECT is OPEN: it assumes a random level is laid out anew
// for each game rather than fixed per character, and that has not been
// measured on this build. Numbering is positional, so if the layout does
// persist then so could this record -- and if it does not, keeping it would
// name the wrong rooms. Do not change this without the measurement; the
// experiment is written up in the workspace HANDOFF.md.
//
// Pure logic over rooms and a position: no native calls, no SDK, unit-tested
// without the game.
namespace D2RAccess::RoomCatalogue {

struct Entry {
	NativeRooms::Room room;
	int               number = 0;     // 1-based, stable for the level instance
	bool              explored = false;
	Radar::Position   centre;         // subtiles, for bearing and ordering
};

class Registry {
public:
	// `rooms` must be NativeRooms::Read order -- sorted, so numbering is the
	// same on every visit. A new level or session starts the list over.
	void Observe(int level, uint64_t session, const std::vector<NativeRooms::Room>& rooms, Radar::Position player);
	void Clear();

	const std::vector<Entry>& Entries() const { return m_entries; }
	int                       Level() const { return m_level; }

private:
	int                m_level = 0;
	uint64_t           m_session = 0;
	std::vector<Entry> m_entries;
};

// One catalogue entity per room. `explored` and `unexplored` are printf
// formats taking the room number. Identity is the room's rectangle origin, so
// a selection survives the list being rebuilt.
std::vector<EntityRadar::Entity> Entities(const std::vector<Entry>& entries,
	const char* explored, const char* unexplored);

}
