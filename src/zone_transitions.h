#pragma once

#include "native_level_exits.h"
#include "radar.h"

#include <vector>

namespace D2RAccess::ZoneTransitions {
// Nearby passable crossings from this level into a different loaded level.
std::vector<Radar::LevelExit> Find(const Radar::World& world, const NativeLevelExits::Layout& layout);
}
