#include "zone_transitions.h"

#include "ground_items.h"
#include "level_exit_layout.h"
#include "rooms.h"
#include "room_graph.h"

#include <algorithm>
#include <cmath>
#include <tuple>

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
	return result;
}

bool Crosses(const EntityRadar::Entity& entity) {
	return entity.kind == EntityRadar::Kind::Exit && entity.category == EntityRadar::Category::Exit
		&& entity.destinationLevel != 0 && entity.border != EntityRadar::Entity::Border::None;
}

std::vector<NativeRooms::Bounds> ApproachRegions(const EntityRadar::Entity& entity,
	const NativeLevelExits::Layout& layout) {
	std::vector<NativeRooms::Bounds> edges;
	if (!Crosses(entity) || entity.border != EntityRadar::Entity::Border::Bearing) return edges;
	const auto bounds = [](const NativeLevelExits::TileRect& r) {
		return NativeRooms::Bounds{r.x * Scale, r.y * Scale, (r.x + r.width) * Scale, (r.y + r.height) * Scale};
	};
	// Two cells leave a centre just inside the edge with room for its footprint.
	// NearGapTiles is the game's logical-room adjacency limit (room_graph.h).
	constexpr int depth = 2, near = RoomGraph::NearGapTiles * Scale;
	for (const auto& neighbour : layout.neighbours) {
		if (neighbour.level != entity.destinationLevel) continue;
		const auto other = bounds(neighbour.rect);
		for (const auto& room : layout.rooms) {
			const auto own = bounds(room.rect);
			NativeRooms::Bounds edge;
			if (own.maxX <= other.minX && other.minX - own.maxX < near)
				edge = {std::max(own.minX, own.maxX - depth), std::max(own.minY, other.minY), own.maxX, std::min(own.maxY, other.maxY)};
			else if (other.maxX <= own.minX && own.minX - other.maxX < near)
				edge = {own.minX, std::max(own.minY, other.minY), std::min(own.maxX, own.minX + depth), std::min(own.maxY, other.maxY)};
			else if (own.maxY <= other.minY && other.minY - own.maxY < near)
				edge = {std::max(own.minX, other.minX), std::max(own.minY, own.maxY - depth), std::min(own.maxX, other.maxX), own.maxY};
			else if (other.maxY <= own.minY && own.minY - other.maxY < near)
				edge = {std::max(own.minX, other.minX), own.minY, std::min(own.maxX, other.maxX), std::min(own.maxY, own.minY + depth)};
			if (edge.minX < edge.maxX && edge.minY < edge.maxY) edges.push_back(edge);
		}
	}
	// Native near lists repeat rooms and their order depends on streaming.
	// Canonical sections also keep the route field's cache stable.
	std::sort(edges.begin(), edges.end(), [](const auto& a, const auto& b) {
		return std::tie(a.minX, a.minY, a.maxX, a.maxY) < std::tie(b.minX, b.minY, b.maxX, b.maxY);
	});
	edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
	return edges;
}

bool RestoreCrossing(EntityRadar::Entity& entity, const EntityRadar::Entity& known) {
	if (!Crosses(entity) || entity.border == EntityRadar::Entity::Border::Crossing
		|| known.border != EntityRadar::Entity::Border::Crossing
		|| entity.kind != known.kind || entity.placeId != known.placeId
		|| entity.destinationLevel != known.destinationLevel) return false;
	entity.position = known.position;
	entity.border = EntityRadar::Entity::Border::Crossing;
	return true;
}

void MergeKnown(std::vector<EntityRadar::Entity>& entities, const std::vector<EntityRadar::Entity>& known) {
	for (const auto& place : known) {
		const auto live = std::find_if(entities.begin(), entities.end(), [&](const auto& entity) {
			return entity.kind == place.kind && entity.placeId == place.placeId && (entity.placeId || entity.id == place.id);
		});
		if (live == entities.end()) entities.push_back(place);
		else RestoreCrossing(*live, place);
	}
}

bool MoveToCrossing(EntityRadar::Entity& entity, const std::vector<Radar::LevelExit>& crossings) {
	if (!Crosses(entity)) return false;
	const auto found = std::find_if(crossings.begin(), crossings.end(),
		[&](const auto& crossing) { return crossing.level == entity.destinationLevel; });
	if (found == crossings.end()) return false;
	entity.position = found->position;
	entity.border = EntityRadar::Entity::Border::Crossing;
	return true;
}

std::vector<EntityRadar::Entity> FoundAtCrossings(const std::vector<EntityRadar::Entity>& entities,
	const std::vector<Radar::LevelExit>& crossings, const Filter& found) {
	std::vector<EntityRadar::Entity> result;
	for (const auto& entity : entities) {
		auto atCrossing = entity;
		MoveToCrossing(atCrossing, crossings);
		// A rectangle midpoint is a bearing, even if the automap charts it.
		// Store the actual opening, never the point that merely names the edge.
		if (atCrossing.border == EntityRadar::Entity::Border::Bearing) continue;
		if (!found({atCrossing}).empty()) result.push_back(std::move(atCrossing));
	}
	return result;
}
}
