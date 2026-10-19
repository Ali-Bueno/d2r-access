#pragma once

#include "radar.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The game's list of every player's pets (summons, the hireling, traps, bound
// demons), the one its party portraits read, and the PetType rows that say how
// a portrait shows them. docs/summons-and-corpses-re.md
namespace D2RAccess::NativePets {

bool Initialize(const D2RL::PluginContext* context);

struct Pet {
	uint32_t id = 0, owner = 0; // unit ids
	uint16_t classId = 0;       // MonStats row
	int type = -1;              // PetType row; 7 is the hireling
	int life = -1;              // percent as the portrait's bar shows it
	bool deadHireling = false;  // the local player's dead hireling keeps its entry
};
// Every player's pets, newest first; absent when the list cannot be read.
std::optional<std::vector<Pet>> All();

// The game's other ownership rule (hostility 0x971E0): a monster whose owner is
// a player by its own flags, for one the list does not hold.
bool PlayerOwned(uintptr_t unit);

// How the portraits show a pet type: 1 one portrait, 2 one with a count,
// 3 one per pet, 0 none; the life bar only with `drawHp`.
struct Kind {
	int iconType = 0;
	bool drawHp = false;
	std::string name; // the portrait's generic name ("Revived"), in the game's language
};
std::optional<Kind> KindOf(int type);

// A MonStats row's own name in the game's language ("Oak Sage"), empty when unreadable.
std::string ClassName(uint16_t classId);

// Where a loaded unit stands, in subtiles, by the game's own lookup by id (0x9A5D0):
// `type` 0 for a player, 1 for a monster. Absent when it is not loaded. UI thread only.
std::optional<Radar::Position> PositionOf(uint32_t id, uint32_t type);

}
