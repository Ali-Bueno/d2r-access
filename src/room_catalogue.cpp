#include "room_catalogue.h"

#include <cstdio>

namespace D2RAccess::RoomCatalogue {
namespace {

Radar::Position Centre(const NativeRooms::Room& room) {
	const auto bounds = NativeRooms::Subtiles(room);
	return {static_cast<double>(bounds.minX + bounds.maxX - 1) / 2,
		static_cast<double>(bounds.minY + bounds.maxY - 1) / 2};
}

}

void Registry::Observe(int level, uint64_t session, const std::vector<NativeRooms::Room>& rooms, Radar::Position player) {
	if (level != m_level || session != m_session) {
		Clear();
		m_level = level;
		m_session = session;
	}
	// The room list is fixed for a level instance, so it is only laid out
	// once; later calls carry the exploration forward rather than resetting
	// it. A list that changes size means a different set of rooms, and the
	// numbering has to start again or it would name the wrong ones.
	if (m_entries.size() != rooms.size()) {
		m_entries.clear();
		m_entries.reserve(rooms.size());
		for (size_t i = 0; i < rooms.size(); ++i)
			m_entries.push_back({rooms[i], static_cast<int>(i) + 1, false, Centre(rooms[i])});
	} else {
		for (size_t i = 0; i < rooms.size(); ++i) m_entries[i].room = rooms[i];
	}
	const size_t standing = NativeRooms::Containing(rooms, player);
	if (standing != NativeRooms::None) m_entries[standing].explored = true;
}

void Registry::Clear() {
	m_entries.clear();
	m_level = 0;
	m_session = 0;
}

std::vector<EntityRadar::Entity> Entities(const std::vector<Entry>& entries,
	const char* explored, const char* unexplored) {
	std::vector<EntityRadar::Entity> result;
	result.reserve(entries.size());
	for (const Entry& entry : entries) {
		const char* format = entry.explored ? explored : unexplored;
		char        name[64]{};
		if (format != nullptr) std::snprintf(name, sizeof(name), format, entry.number);
		EntityRadar::Entity entity;
		entity.id = static_cast<uint32_t>(entry.number);
		entity.kind = EntityRadar::Kind::Room;
		entity.position = entry.centre;
		entity.name = name;
		// Rooms do not move and are not native units, so identity is where
		// the room is. SameTarget compares this before anything else, which
		// keeps a lock on room 7 from following a monster with id 7.
		entity.levelExitId = entry.room.id;
		entity.category = EntityRadar::Category::Room;
		result.push_back(std::move(entity));
	}
	return result;
}

}
