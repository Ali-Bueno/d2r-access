#include "explored_rooms.h"

namespace D2RAccess::ExploredRooms {

bool Memory::Mark(uint32_t level, int tileX, int tileY) {
	return levels_[level].insert({tileX, tileY}).second;
}

bool Memory::Explored(uint32_t level, int tileX, int tileY) const {
	const auto found = levels_.find(level);
	return found != levels_.end() && found->second.count({tileX, tileY}) != 0;
}

size_t Memory::Count(uint32_t level) const {
	const auto found = levels_.find(level);
	return found == levels_.end() ? 0 : found->second.size();
}

void Memory::Forget() { levels_.clear(); }

}
