#pragma once

#include "level_route.h"

#include <cstdint>
#include <optional>
#include <vector>

// A place of a level cut off on foot there, got to by way of a neighbouring
// level: an Act III river cuts its areas in two, and the bridge may be in the
// area before (docs/detours.md). Pure logic, unit-tested without the game.
namespace D2RAccess::LevelRoute {

struct Detour {
	Position leave; // the last cell of the level stood in before the other one
	Position back;  // the last cell of the level passed through before the goal's is entered for good
	int steps = 0;
};

class Detours {
public:
	// `here` is the map of the level stood in, `there` the neighbour's; the goal is in one, the
	// other is passed through. `guess`: ground of the level passed through that the game has not
	// shown yet counts as a way; the goal's always does.
	enum class Goal { Here, There };
	Detours(const Memory& here, const Memory& there, Goal goal, Position from, int bodyPattern, bool guess);
	std::optional<Detour> To(const Area& room) const;
private:
	bool Foreign(Position at) const; // the neighbour's ground
	Memory joined_;
	std::optional<Field> field_;
	std::vector<uint8_t> foreign_; // by joined_'s cells
	bool goalHere_ = true;
};
}
