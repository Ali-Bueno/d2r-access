#include "room_catalogue.h"

#include "room_graph.h"

#include <algorithm>
#include <cstdio>

namespace D2RAccess::RoomCatalogue {
namespace {

Radar::Position Centre(const NativeRooms::Room& room) {
	const auto bounds = NativeRooms::Subtiles(room);
	return {static_cast<double>(bounds.minX + bounds.maxX - 1) / 2,
		static_cast<double>(bounds.minY + bounds.maxY - 1) / 2};
}

// A digest of the room rectangles, in the order the reader sorts them. Equal
// digests mean the same rooms in the same places, which is what makes a
// remembered exploration still apply. FNV-1a over the four tile fields.
uint64_t Layout(const std::vector<NativeRooms::Room>& rooms) {
	constexpr uint64_t Prime = 1099511628211ULL;
	uint64_t           hash = 14695981039346656037ULL;
	const auto         mix = [&hash](int value) {
        for (int shift = 0; shift < 32; shift += 8) hash = (hash ^ static_cast<uint8_t>(value >> shift)) * Prime;
	};
	for (const NativeRooms::Room& room : rooms) {
		mix(room.tileX); mix(room.tileY); mix(room.tileWidth); mix(room.tileHeight);
	}
	return hash;
}

}

void Registry::Stash() {
	if (m_entries.empty() || m_key.layout == 0) return;
	std::vector<bool> explored;
	explored.reserve(m_entries.size());
	for (const Entry& entry : m_entries) explored.push_back(entry.explored);
	std::erase_if(m_remembered, [&](const Record& record) { return record.key == m_key; });
	if (m_remembered.size() >= MaxRemembered) m_remembered.erase(m_remembered.begin());
	m_remembered.push_back({m_key, std::move(explored)});
}

void Registry::Observe(int level, uint32_t mapSeed, const std::vector<NativeRooms::Room>& rooms, Radar::Position player) {
	const Key key{level, mapSeed, rooms.empty() ? 0 : Layout(rooms)};
	if (key != m_key) {
		// Put the level being left where it can be found again, then take back
		// whatever was recorded for the one being entered. A level regenerated
		// under the same id has a different layout digest, so it gets a clean
		// record instead of the old one's room numbers.
		Stash();
		m_key = key;
		m_level = level;
		m_seed = mapSeed;
		m_entries.clear();
		m_entries.reserve(rooms.size());
		for (size_t i = 0; i < rooms.size(); ++i)
			m_entries.push_back({rooms[i], static_cast<int>(i) + 1, false, Centre(rooms[i])});
		const auto found = std::find_if(m_remembered.begin(), m_remembered.end(),
			[&](const Record& record) { return record.key == key; });
		if (found != m_remembered.end()) {
			if (found->explored.size() == m_entries.size())
				for (size_t i = 0; i < m_entries.size(); ++i) m_entries[i].explored = found->explored[i];
			// This level is the live one again; it is stashed afresh when left.
			m_remembered.erase(found);
		}
	} else {
		// Same layout: the rectangles are identical by construction, so this
		// only refreshes the room handles and carries the exploration forward.
		for (size_t i = 0; i < rooms.size() && i < m_entries.size(); ++i) m_entries[i].room = rooms[i];
	}
	const size_t standing = NativeRooms::Containing(rooms, player);
	if (standing != NativeRooms::None && standing < m_entries.size()) m_entries[standing].explored = true;
}

void Registry::Clear() {
	m_entries.clear();
	m_remembered.clear();
	m_key = {};
	m_level = 0;
	m_seed = 0;
}

std::vector<Entry> Exploration(const std::vector<Entry>& entries,
	const std::vector<NativeRooms::Room>& visited, const std::vector<NativeRooms::Room>& explored,
	const Navigation::Grid* grid, bool everyUnexplored) {
	const auto listed = [](const std::vector<NativeRooms::Room>& rooms, const NativeRooms::Room& room) {
		return std::any_of(rooms.begin(), rooms.end(), [&](const NativeRooms::Room& other) { return other.id == room.id; });
	};
	std::vector<Entry> stood = entries;
	std::vector<NativeRooms::Room> known = explored;
	for (Entry& entry : stood) {
		entry.explored = entry.explored || listed(visited, entry.room);
		if (entry.explored && !listed(known, entry.room)) known.push_back(entry.room);
	}
	std::vector<Entry> result;
	for (const Entry& entry : stood) {
		const bool frontier = !listed(known, entry.room) && (everyUnexplored || std::any_of(known.begin(), known.end(),
			[&](const NativeRooms::Room& room) {
				return RoomGraph::Touching(room, entry.room)
					&& (!grid || RoomGraph::Border(*grid, room, entry.room) != RoomGraph::Passage::Closed);
			}));
		if (entry.explored || frontier) result.push_back(entry);
	}
	return result;
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
		entity.placeId = entry.room.id;
		entity.category = entry.explored ? EntityRadar::Category::Room : EntityRadar::Category::UnexploredRoom;
		result.push_back(std::move(entity));
	}
	return result;
}

}
