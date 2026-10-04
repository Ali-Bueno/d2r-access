#pragma once

#include "native_rooms.h"
#include "radar.h"
#include <vector>

// Limits navigation to logical rooms the player has physically entered.
namespace D2RAccess::NavigationPolicy {
bool Contains(const std::vector<NativeRooms::Room>& visited, Radar::Position position);
Radar::World Restrict(const Radar::World& world, const std::vector<NativeRooms::Room>& visited);
// Exploration's known ground: `visited`, `charted` (null when the automap cannot
// be read) and the room of `rooms` holding `player`, each room once.
std::vector<NativeRooms::Room> Explored(std::vector<NativeRooms::Room> visited,
	const std::vector<NativeRooms::Room>* charted, const std::vector<NativeRooms::Room>& rooms, Radar::Position player);
// H counts physical visits/current room: automap scenery can be drawn before
// the player walks there, including outdoor jungle terrain. The charted input
// is retained for callers sharing the policy API. Beacon restrictions use Explored.
std::vector<NativeRooms::Room> ForExploration(int level, std::vector<NativeRooms::Room> visited,
	const std::vector<NativeRooms::Room>* charted, const std::vector<NativeRooms::Room>& rooms, Radar::Position player);
}
