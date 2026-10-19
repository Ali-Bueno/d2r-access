#pragma once

#include "entity_radar.h"
#include "ground_items.h"
#include "native_level_exits.h"
#include "passages.h"
#include "radar.h"

#include "native_rooms.h"

#include <functional>
#include <optional>
#include <vector>

namespace D2RAccess::ZoneTransitions {
// Passable crossings from this level into a different loaded level, within
// `radius` of the player: one per level, the nearest.
std::vector<Radar::LevelExit> Find(const Radar::World& world, const NativeLevelExits::Layout& layout,
	double radius = GroundItems::AudibleRadius);
// A crossing that ends a corridor, moved to the corridor's other end: the mouth
// a player coming from this area walks into. Unchanged elsewhere.
Radar::Position MouthOf(const Passages::Reading& reading, Radar::Position crossing, Radar::Position player);
Radar::Position CorridorMouth(const Radar::World& world, Radar::Position crossing);
// Only a border created by the area reader, never stairs, portals or warps.
bool Crosses(const EntityRadar::Entity& entity);
// An unmeasured bearing names a shared edge, not a destination cell. Approach
// any floor just inside every facing room edge until collision identifies the
// opening. Separate sections stay separate on irregular levels.
std::vector<NativeRooms::Bounds> ApproachRegions(const EntityRadar::Entity& entity,
	const NativeLevelExits::Layout& layout);
// Moves such an exit to its crossing among `crossings`, when there is one; the
// identity stays, so it is still the same place. docs/beacon.md#area-borders
bool MoveToCrossing(EntityRadar::Entity& entity, const std::vector<Radar::LevelExit>& crossings);
// Retain a measured crossing when a fresh read supplies only its bearing.
bool RestoreCrossing(EntityRadar::Entity& entity, const EntityRadar::Entity& known);
// Add absent known places and restore crossings over live approximate bearings.
void MergeKnown(std::vector<EntityRadar::Entity>& entities, const std::vector<EntityRadar::Entity>& known);
// Discover measured crossings at their real positions. Bearings are not finds.
// Other landmarks retain the ordinary discovery policy.
using Filter = std::function<std::vector<EntityRadar::Entity>(const std::vector<EntityRadar::Entity>&)>;
std::vector<EntityRadar::Entity> FoundAtCrossings(const std::vector<EntityRadar::Entity>& entities,
	const std::vector<Radar::LevelExit>& crossings, const Filter& found);
}
