#include "navigation_policy.h"
#include <algorithm>
#include <cmath>

namespace D2RAccess::NavigationPolicy {

bool Contains(const std::vector<NativeRooms::Room>& visited, Radar::Position position) {
	if (!std::isfinite(position.x) || !std::isfinite(position.y)) return false;
	const int x = static_cast<int>(std::lround(position.x));
	const int y = static_cast<int>(std::lround(position.y));
	for (const auto& room : visited)
		if (NativeRooms::Contains(NativeRooms::Subtiles(room), x, y)) return true;
	return false;
}

Radar::World Restrict(const Radar::World& world, const std::vector<NativeRooms::Room>& visited) {
	Radar::World restricted = world;
	for (auto& grid : restricted.rooms)
		for (auto& cell : grid.cells) cell |= Radar::Collision::Blank;
	for (const auto& room : visited) {
		const auto bounds = NativeRooms::Subtiles(room);
		for (size_t i = 0; i < world.rooms.size(); ++i) {
			const auto& source = world.rooms[i];
			auto& destination = restricted.rooms[i];
			const int minX = std::max(bounds.minX, source.x);
			const int minY = std::max(bounds.minY, source.y);
			const int maxX = std::min(bounds.maxX, source.x + source.width);
			const int maxY = std::min(bounds.maxY, source.y + source.height);
			for (int y = minY; y < maxY; ++y)
				for (int x = minX; x < maxX; ++x) {
					const size_t index = static_cast<size_t>(y - source.y) * source.width + (x - source.x);
					if (index < source.cells.size()) destination.cells[index] = source.cells[index];
				}
		}
	}
	return restricted;
}

std::vector<NativeRooms::Room> Explored(std::vector<NativeRooms::Room> visited,
	const std::vector<NativeRooms::Room>* charted, const std::vector<NativeRooms::Room>& rooms, Radar::Position player) {
	const auto add = [&visited](const NativeRooms::Room& room) {
		if (std::none_of(visited.begin(), visited.end(), [&](const NativeRooms::Room& known) { return known.id == room.id; }))
			visited.push_back(room);
	};
	if (charted)
		for (const auto& room : *charted) add(room);
	// The room the player stands in is explored even before either memory records it.
	if (const size_t current = NativeRooms::Containing(rooms, player); current != NativeRooms::None) add(rooms[current]);
	return visited;
}
}
