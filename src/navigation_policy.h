#pragma once

#include "native_rooms.h"
#include "radar.h"
#include <vector>

// Limits navigation to logical rooms the player has physically entered.
namespace D2RAccess::NavigationPolicy {
bool Contains(const std::vector<NativeRooms::Room>& visited, Radar::Position position);
Radar::World Restrict(const Radar::World& world, const std::vector<NativeRooms::Room>& visited);
}
