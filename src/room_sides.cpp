#include "room_sides.h"

#include <cmath>

namespace D2RAccess::RoomSides {

Position MapSide(Position offset) {
	if (std::abs(offset.x) >= std::abs(offset.y)) return {offset.x < 0 ? -1.0 : 1.0, 0};
	return {0, offset.y < 0 ? -1.0 : 1.0};
}

Position Wall(const Rooms::Opening& opening) {
	return MapSide({opening.across.x - opening.centre.x, opening.across.y - opening.centre.y});
}

}
