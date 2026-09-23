#pragma once

#include "explored_rooms.h"
#include "native_level_exits.h"

#include <optional>

namespace D2RAccess::ExplorationCues {
struct Room {
	NativeLevelExits::TileRect rect;
	Radar::Position position;
};
struct Selection {
	std::optional<Room> nearest;
	std::optional<Room> entered;
};

// Select cues from active maze rooms; the caller records an entered room after selection.
Selection Select(const NativeLevelExits::Layout& layout, Radar::Position player,
	const ExploredRooms::Memory& memory, bool maze);
}
