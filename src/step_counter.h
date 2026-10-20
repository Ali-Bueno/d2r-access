#pragma once

#include "ground_items.h"

#include <cstdint>
#include <optional>

// The units walked, told by a click every so many of them (a tester, 2026-10-20: to judge
// distances). A jump -- a teleport, a waypoint, a new level -- is not walked. docs/ground-items.md
namespace D2RAccess::StepCounter {
// The intervals offered, in units: subtiles, as the scanner and the route speak them.
inline constexpr int Intervals[] = {1, 5, 10};
// Farther than this between two reads is a jump: a run covers under a unit a tick, a slow tick a few.
inline constexpr double JumpUnits = 10;

class Odometer {
public:
	// The clicks owed for the move to `position`, one per `every` units walked.
	int Walk(GroundItems::Point position, uint64_t session, double every);
	void Reset();
private:
	std::optional<GroundItems::Point> last_;
	uint64_t session_ = 0;
	double walked_ = 0;
};
}
