#include "zone_transitions.h"

#include "ground_items.h"
#include "level_exit_layout.h"
#include "rooms.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::ZoneTransitions {
namespace {
constexpr int Scale = LevelExitLayout::SubtilesPerTile;

bool Contains(const NativeLevelExits::TileRect& rect, Radar::Position point) {
	return point.x >= rect.x * Scale && point.x < (rect.x + rect.width) * Scale
		&& point.y >= rect.y * Scale && point.y < (rect.y + rect.height) * Scale;
}
bool SameRoom(const NativeLevelExits::TileRect& rect, const Radar::RoomGrid& room) {
	return room.x == rect.x * Scale && room.y == rect.y * Scale
		&& room.width == rect.width * Scale && room.height == rect.height * Scale;
}
}

std::vector<Radar::LevelExit> Find(const Radar::World& world, const NativeLevelExits::Layout& layout) {
	std::vector<Radar::LevelExit> result;
	if (layout.level != static_cast<uint32_t>(world.level)
		|| !std::isfinite(world.player.x) || !std::isfinite(world.player.y)) return result;
	for (const auto& grid : world.rooms) {
		if (std::none_of(layout.rooms.begin(), layout.rooms.end(), [&](const auto& own) { return SameRoom(own.rect, grid); })) continue;
		Rooms::Room room;
		if (!Rooms::Describe(world, {grid.x + 0.5, grid.y + 0.5}, room)) continue;
		for (const auto& opening : room.openings) {
			const bool horizontal = opening.centre.y == room.y || opening.centre.y == room.y + room.height - 1;
			const double first = (horizontal ? opening.centre.x : opening.centre.y) - (opening.width - 1) / 2.0;
			for (int i = 0; i < opening.width; ++i) {
				Radar::Position point = opening.centre;
				if (horizontal) point.x = first + i;
				else point.y = first + i;
				const Radar::Position across{opening.across.x + point.x - opening.centre.x,
					opening.across.y + point.y - opening.centre.y};
				const double distance = std::hypot(point.x - world.player.x, point.y - world.player.y);
				if (distance >= GroundItems::AudibleRadius
					|| world.Body(static_cast<int>(point.x), static_cast<int>(point.y), Radar::Layer::Terrain) != Radar::State::Open
					|| world.Body(static_cast<int>(across.x), static_cast<int>(across.y), Radar::Layer::Terrain) != Radar::State::Open) continue;
				for (const auto& neighbour : layout.neighbours) {
					if (neighbour.level == layout.level || !Contains(neighbour.rect, across)) continue;
					const auto found = std::find_if(result.begin(), result.end(), [&](const auto& exit) { return exit.level == neighbour.level; });
					if (found == result.end()) result.push_back({point, neighbour.level});
					else if (distance < std::hypot(found->position.x - world.player.x, found->position.y - world.player.y))
						found->position = point;
					break;
				}
			}
		}
	}
	return result;
}
}
