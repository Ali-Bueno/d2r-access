#include "corpse_memory.h"

#include "map_survey.h"
#include "room_graph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace D2RAccess::CorpseMemory {

bool Covered(const std::vector<NativeRooms::Room>& rooms, const Navigation::Grid& grid,
	Radar::Position player, Radar::Position position) {
	const size_t here = NativeRooms::Containing(rooms, player), there = NativeRooms::Containing(rooms, position);
	if (here == NativeRooms::None || there == NativeRooms::None) return false;
	if (there != here && !RoomGraph::Touching(rooms[here], rooms[there])) return false;
	const auto* cell = grid.At(static_cast<int>(std::lround(position.x)), static_cast<int>(std::lround(position.y)));
	return cell != nullptr && cell->body != Radar::State::Unknown;
}

bool Memory::Merge(Known& entry, const std::vector<EntityRadar::Entity>& loaded) {
	const auto found = std::find_if(loaded.begin(), loaded.end(),
		[&entry](const EntityRadar::Entity& corpse) { return EntityRadar::SameTarget(corpse, entry.entity); });
	if (found == loaded.end()) return false;
	entry.entity = *found;
	return true;
}

void Memory::Add(std::vector<Known>& known, const std::vector<EntityRadar::Entity>& loaded) {
	for (const auto& corpse : loaded)
		if (std::none_of(known.begin(), known.end(), [&corpse](const Known& entry) { return EntityRadar::SameTarget(entry.entity, corpse); }))
			known.push_back({corpse, 0});
}

void Memory::Forget(std::vector<Known>& known) {
	// As the catalogue does: one read that misses it is not proof it is gone.
	const int limit = MapSurvey::Settings{}.missesBeforeForgetting;
	std::erase_if(known, [limit](const Known& entry) { return entry.misses >= limit; });
}

void Memory::Track(const std::vector<Owner>& owners, const char* nameFormat) {
	std::set<uint32_t> listed;
	for (const auto& owner : owners) listed.insert(owner.corpses.begin(), owner.corpses.end());
	for (const auto& owner : owners) {
		// Until respawn the dead member has not moved: the roster still places them where they fell.
		if (!m_tracking || !owner.partyMember || owner.life != 0 || !owner.level || !owner.located || owner.name.empty()) continue;
		for (const uint32_t id : owner.corpses) {
			if (m_listed.contains(id)) continue;
			EntityRadar::Entity corpse;
			corpse.id = id;
			corpse.kind = EntityRadar::Kind::Corpse;
			corpse.category = EntityRadar::Category::Corpse;
			corpse.position = owner.position;
			char name[128]{};
			std::snprintf(name, sizeof name, nameFormat, owner.name.c_str());
			corpse.name = name;
			Add(m_levels[static_cast<int>(owner.level)], {corpse});
		}
	}
	for (auto& [level, known] : m_levels)
		std::erase_if(known, [&](const Known& entry) { return m_listed.contains(entry.entity.id) && !listed.contains(entry.entity.id); });
	m_listed = std::move(listed);
	m_tracking = true;
}

std::vector<EntityRadar::Entity> Memory::Entities(int level) const {
	std::vector<EntityRadar::Entity> result;
	if (const auto found = m_levels.find(level); found != m_levels.end())
		for (const auto& entry : found->second) result.push_back(entry.entity);
	return result;
}

}
