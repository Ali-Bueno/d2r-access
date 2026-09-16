#include "radar.h"

namespace D2RAccess::Radar {
namespace {
// Kirandur FUN_1403f5de0 compares projected WORLD hit positions, not distances
// from the player (DAT_140c1ee8c/90 = +/-1.5). The flat D2 adapter uses one
// collision subtile per query unit; this unit mapping is a porting choice.
constexpr double HitChange = 1.5;
State Difference(Boundary before, Boundary after) {
	if (before.end == State::Unknown || after.end == State::Unknown) return State::Unknown;
	if (before.end != after.end) return after.end == State::Open ? State::Open : State::Blocked;
	if (after.end != State::Blocked) return State::Unknown;
	const double change = after.along - before.along;
	return change > HitChange ? State::Open : change < -HitChange ? State::Blocked : State::Unknown;
}
}
State BoundaryTracker::Observe(const Probe& terrain, Position axis, uint64_t now, uint64_t confirmationMs, double range) {
	// A fully inspected local range is open even if distant room coverage is
	// unknown. Losing coverage inside that range is not the loss of a wall.
	current_ = {terrain.clearDepth >= range ? State::Open : terrain.end,
		terrain.hit.x * axis.x + terrain.hit.y * axis.y};
	const State change = Difference(previous_, current_);
	if (change == State::Unknown) {
		previous_ = current_;
		candidate_ = State::Unknown;
		return State::Unknown;
	}
	if (change != candidate_) { candidate_ = change; since_ = now; }
	if (now - since_ < confirmationMs) return State::Unknown;
	previous_ = current_;
	candidate_ = State::Unknown;
	return change;
}
bool BoundaryTracker::Accepts(Boundary target, State change) const {
	if (target.end == State::Unknown || current_.end == State::Unknown) return false;
	// A further change in the same direction remains meaningful. A returned
	// wall or a newly receded wall invalidates the opposite queued description.
	const State difference = Difference(target, current_);
	return difference == State::Unknown || difference == change;
}
}
