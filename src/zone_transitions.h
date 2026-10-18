#pragma once

#include "entity_radar.h"
#include "ground_items.h"
#include "native_level_exits.h"
#include "passages.h"
#include "radar.h"

#include <functional>
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
// An exit that names a neighbouring level: it may be walked across somewhere
// along the shared border rather than at the point that names it.
bool Crosses(const EntityRadar::Entity& entity);
// Moves such an exit to its crossing among `crossings`, when there is one; the
// identity stays, so it is still the same place. docs/beacon.md#area-borders
bool MoveToCrossing(EntityRadar::Entity& entity, const std::vector<Radar::LevelExit>& crossings);
// The entities `found` keeps, a border also judged at its crossing among
// `crossings`; each kept entity keeps its own position. docs/beacon.md#area-borders
using Filter = std::function<std::vector<EntityRadar::Entity>(const std::vector<EntityRadar::Entity>&)>;
std::vector<EntityRadar::Entity> FoundAtCrossings(const std::vector<EntityRadar::Entity>& entities,
	const std::vector<Radar::LevelExit>& crossings, const Filter& found);
}
