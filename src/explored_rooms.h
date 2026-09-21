#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <utility>

// The logical rooms the game has streamed at least once, kept per level for
// the whole session the way the automap keeps what it has already drawn, so
// walking back into a level finds it as explored as it was left.
// `docs/terrain-overview.md`
namespace D2RAccess::ExploredRooms {
class Memory {
public:
	// True when this room was not already known, so the caller can log changes.
	bool Mark(uint32_t level, int tileX, int tileY);
	bool Explored(uint32_t level, int tileX, int tileY) const;
	size_t Count(uint32_t level) const;
	void Forget(); // a new game starts from an unexplored map
private:
	std::map<uint32_t, std::set<std::pair<int, int>>> levels_;
};
}
