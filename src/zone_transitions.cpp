#include "zone_transitions.h"

#include "ground_items.h"
#include "level_exit_layout.h"
#include "passages.h"
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

// Corridor tuning as the place service reads corridors (place_service.cpp).
constexpr int CorridorWidthSlack = 2, CorridorFollowWidths = 6;

Passages::Ground Ground(const Radar::World& world, int x, int y) {
	const auto state = world.Cell(x, y, Radar::Layer::Terrain);
	if (state == Radar::State::Unknown) return Passages::Ground::Unknown;
	return Radar::Passable(state) || state == Radar::State::Door ? Passages::Ground::Open : Passages::Ground::Blocked;
}
}

Radar::Position MouthOf(const Passages::Reading& reading, Radar::Position crossing, Radar::Position player) {
	if (reading.shape != Passages::Shape::Corridor) return crossing;
	const auto along = [&](double x, double y) { return reading.alongX ? x : y; };
	const double at = along(crossing.x, crossing.y);
	const auto& [low, high] = reading.ends;
	const bool crossingAtHigh = std::abs(at - along(high.x, high.y)) < std::abs(at - along(low.x, low.y));
	const auto& mouth = crossingAtHigh ? low : high;
	if (mouth.end != Passages::End::Opens) return crossing;
	const double mouthAt = along(mouth.x, mouth.y), here = along(player.x, player.y);
	// Past the mouth already, inside the corridor, the crossing is the way on.
	if ((here - mouthAt) * (here - at) < 0) return crossing;
	return {static_cast<double>(mouth.x), static_cast<double>(mouth.y)};
}

Radar::Position CorridorMouth(const Radar::World& world, Radar::Position crossing) {
	const int typical = Rooms::TypicalOpeningWidth(world);
	if (typical <= 0) return crossing;
	const Passages::Settings settings{.minWidth = typical / 2, .maxWidth = typical + CorridorWidthSlack, .minLength = typical,
		.slack = typical / 2, .reach = typical * CorridorFollowWidths};
	const auto reading = Passages::Classify([&world](int x, int y) { return Ground(world, x, y); },
		static_cast<int>(crossing.x), static_cast<int>(crossing.y), settings);
	return MouthOf(reading, crossing, world.player);
}

bool DoorAt(const Radar::World& world, Radar::Position crossing) {
	constexpr int Reach = 2; // the crossing is the door's nearest cell; a double door spans more
	for (int dy = -Reach; dy <= Reach; ++dy)
		for (int dx = -Reach; dx <= Reach; ++dx) {
			uint16_t flags = 0;
			(void)world.Cell(static_cast<int>(crossing.x) + dx, static_cast<int>(crossing.y) + dy, Radar::Layer::Movement, &flags);
			if (flags & Radar::Collision::Door) return true;
		}
	return false;
}

std::vector<Radar::LevelExit> Find(const Radar::World& world, const NativeLevelExits::Layout& layout, double radius) {
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
				if (distance >= radius
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
	// The Cathedral's doors end the colonnade walk, which reads as a corridor (a tester, 2026-10-20).
	for (auto& exit : result) if (!DoorAt(world, exit.position)) exit.position = CorridorMouth(world, exit.position);
	return result;
}

bool Crosses(const EntityRadar::Entity& entity) {
	return entity.kind == EntityRadar::Kind::Exit && entity.destinationLevel != 0;
}

bool MoveToCrossing(EntityRadar::Entity& entity, const std::vector<Radar::LevelExit>& crossings) {
	if (!Crosses(entity)) return false;
	const auto found = std::find_if(crossings.begin(), crossings.end(),
		[&](const auto& crossing) { return crossing.level == entity.destinationLevel; });
	if (found == crossings.end()) return false;
	entity.position = found->position;
	return true;
}

std::vector<EntityRadar::Entity> FoundAtCrossings(const std::vector<EntityRadar::Entity>& entities,
	const std::vector<Radar::LevelExit>& crossings, const Filter& found) {
	std::vector<EntityRadar::Entity> result;
	for (const auto& entity : entities) {
		auto atCrossing = entity;
		if (!found({entity}).empty() || (MoveToCrossing(atCrossing, crossings) && !found({atCrossing}).empty()))
			result.push_back(entity);
	}
	return result;
}
}
