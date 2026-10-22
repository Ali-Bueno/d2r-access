#pragma once

#include "entity_radar.h"
#include "rooms.h"

#include <cstdint>
#include <vector>

// Doorways that are no door object, where a sighted player sees them: the halls of the crypt
// tile set, Tristram, and the Act I buildings (cottages, the Tower's ruin). They creak and are
// listed with the doors. docs/ground-items.md#doorways-2026-10-20
namespace D2RAccess::Doorways {

// A building's map piece in collision subtiles, max exclusive.
struct Box { int minX = 0, minY = 0, maxX = 0, maxY = 0; };

// The doorways that count on a level of LvlTypes row `levelType`: every one on a tile set built
// of doorways, elsewhere those inside a building; none within a tile of a door object (`doors`).
std::vector<Rooms::Doorway> Select(const std::vector<Rooms::Doorway>& found, int32_t levelType,
	const std::vector<Box>& buildings, const std::vector<Radar::Position>& doors);

// The doorways in the loaded walls of the world's level; `entities` as NativeEntities reads them.
std::vector<Rooms::Doorway> Find(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities);

// A doorway as a scanner entry: a place with no unit, routed to like a door.
EntityRadar::Entity Listed(const Rooms::Doorway& doorway);

}
