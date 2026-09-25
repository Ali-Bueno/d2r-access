#pragma once

#include "entity_radar.h"
#include "ground_items.h"
#include "native_level_exits.h"
#include "radar.h"

#include <vector>

namespace D2RAccess::ZoneTransitions {
// Passable crossings from this level into a different loaded level, within
// `radius` of the player: one per level, the nearest.
std::vector<Radar::LevelExit> Find(const Radar::World& world, const NativeLevelExits::Layout& layout,
	double radius = GroundItems::AudibleRadius);
// An exit that names a neighbouring level: it may be walked across somewhere
// along the shared border rather than at the point that names it.
bool Crosses(const EntityRadar::Entity& entity);
// Moves such an exit to its crossing among `crossings`, when there is one; the
// identity stays, so it is still the same place. docs/beacon.md#area-borders
bool MoveToCrossing(EntityRadar::Entity& entity, const std::vector<Radar::LevelExit>& crossings);
}
