#pragma once

#include "entity_radar.h"
#include "native_rooms.h"
#include "navigation_grid.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

// Player corpses seen this game, per level. A corpse lies where its owner fell
// until they take it back, so it is listed from anywhere on the level until the
// entity read covers its place again without it, or it leaves the party roster.
// Pure logic. docs/player-corpse-re.md
namespace D2RAccess::CorpseMemory {

// Whether the entity read would have seen a corpse at `position`: the player's
// room and the rooms touching it (all inside the near list the read walks),
// where collision is loaded.
bool Covered(const std::vector<NativeRooms::Room>& rooms, const Navigation::Grid& grid,
	Radar::Position player, Radar::Position position);

// One party roster entry and the corpse ids the client lists for it.
struct Owner {
	uint32_t id = 0;
	std::string name;
	bool partyMember = false; // the server sends only party members' positions
	uint32_t level = 0;
	bool located = false;     // `position` has been received
	Radar::Position position;
	uint32_t life = 0;        // percent; zero while dead, before respawning
	std::vector<uint32_t> corpses;
};

class Memory {
public:
	// `loaded` are the corpses read now; `covered` marks each remembered one the read could have seen.
	template <typename Covers>
	void Observe(int level, const std::vector<EntityRadar::Entity>& loaded, Covers covered) {
		auto& known = m_levels[level];
		for (auto& entry : known) {
			const bool present = Merge(entry, loaded);
			entry.misses = present || !covered(entry.entity.position) ? 0 : entry.misses + 1;
		}
		Add(known, loaded);
		Forget(known);
	}
	// A party member's corpse seen being added is placed where the roster puts the
	// dead member; a corpse that leaves every list has been taken back.
	// `nameFormat` takes the owner's name.
	void Track(const std::vector<Owner>& owners, const char* nameFormat);
	std::vector<EntityRadar::Entity> Entities(int level) const;
	void Clear() { *this = {}; }

private:
	struct Known { EntityRadar::Entity entity; int misses = 0; };
	static bool Merge(Known& entry, const std::vector<EntityRadar::Entity>& loaded);
	static void Add(std::vector<Known>& known, const std::vector<EntityRadar::Entity>& loaded);
	static void Forget(std::vector<Known>& known);
	std::map<int, std::vector<Known>> m_levels;
	std::set<uint32_t> m_listed; // corpse ids in the roster at the last Track
	bool m_tracking = false;     // m_listed is a real baseline, not the empty start
};

}
