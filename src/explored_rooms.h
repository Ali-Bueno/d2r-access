#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The logical rooms the game has streamed at least once (seen, what the
// automap draws) and the ones the player has stood in (visited), kept per
// level the way the automap keeps what it has already drawn. Serializable so
// the terrain service can keep it across sessions like the game's own .ma0.
// `docs/exploration.md`
namespace D2RAccess::ExploredRooms {
struct Room { int tileX = 0, tileY = 0; bool visited = false; };
class Memory {
public:
	// True when this room was not already known, so the caller can log changes.
	bool Mark(uint32_t level, int tileX, int tileY);  // seen: streamed by the game
	bool Visit(uint32_t level, int tileX, int tileY); // stood in; implies seen
	bool Explored(uint32_t level, int tileX, int tileY) const;
	bool Visited(uint32_t level, int tileX, int tileY) const;
	bool Known(uint32_t level) const; // at least one room of the level seen
	size_t Count(uint32_t level) const;
	size_t VisitedCount(uint32_t level) const;
	void Forget(); // a new game starts from an unexplored map
	// One room per line: `level tileX tileY s|v`, one level or all of them.
	// Merge adds to what is known.
	std::string Serialize(std::optional<uint32_t> level = std::nullopt) const;
	void Merge(std::string_view text);
	std::vector<Room> Snapshot(uint32_t level) const;
	void Merge(uint32_t level, const std::vector<Room>& rooms);
	bool Dirty() const { return dirty_; } // serialization alone does not persist changes
private:
	struct Level { std::set<std::pair<int, int>> seen, visited; };
	std::map<uint32_t, Level> levels_;
	bool dirty_ = false;
};
}
