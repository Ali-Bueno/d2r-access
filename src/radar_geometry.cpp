#include "radar.h"
#include <algorithm>
#include <cmath>

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
State BoundaryTracker::Observe(const Probe& terrain, Position axis, uint64_t now, uint64_t confirmationMs, double range, bool confirm) {
	// A fully inspected local range is open even if distant room coverage is
	// unknown. Losing coverage inside that range is not the loss of a wall.
	// Integer D2 collision origins can jump by one cell on both axes. Keep a
	// detected wall through that projected quantization band at the range edge.
	const double releaseRange = range + (previous_.end == State::Blocked ? std::abs(axis.x) + std::abs(axis.y) : 0);
	current_ = {terrain.clearDepth >= releaseRange ? State::Open : terrain.end,
		terrain.hit.x * axis.x + terrain.hit.y * axis.y, terrain.clearDepth};
	const double hit = current_.along;
	if (previous_.end == State::Blocked && current_.end == State::Blocked && std::abs(hit - lastHit_) <= HitChange)
		drift_ += hit - lastHit_;
	lastHit_ = hit;
	// Transport pending/played boundary identities along a continuous wall.
	// Otherwise ordinary tangent travel during confirmation or audio queuing
	// would cancel a genuine recess or generate its opposite description.
	current_.along -= drift_;
	if (previous_.end == State::Unknown || current_.end == State::Unknown) {
		candidate_ = State::Unknown;
		drift_ = 0;
		current_.along = hit;
		previous_ = current_;
		return State::Unknown;
	}
	if (candidate_ == State::Unknown) {
		candidate_ = Difference(previous_, current_);
		candidateOrigin_ = previous_;
		since_ = now;
	} else {
		const auto change = Difference(candidateOrigin_, current_);
		if (change != candidate_) { candidate_ = change; since_ = now; }
	}
	// Always compare adjacent spatial samples. Holding the old hit during
	// confirmation accumulates a smooth wall's slope into a fictitious edge.
	previous_ = current_;
	if (!confirm || candidate_ == State::Unknown || now - since_ < confirmationMs) return State::Unknown;
	// A gap crossed entirely between polls can leave a projected displacement
	// after the ray has already returned to its old clearance. Confirm finite
	// edges against the current body depth as well, never that residual alone.
	if (candidateOrigin_.end == State::Blocked && current_.end == State::Blocked) {
		const double depthChange = current_.depth - candidateOrigin_.depth;
		if ((candidate_ == State::Open && depthChange <= HitChange)
			|| (candidate_ == State::Blocked && depthChange >= -HitChange)) {
			candidate_ = State::Unknown;
			return State::Unknown;
		}
	}
	const auto change = candidate_;
	changeOrigin_ = candidateOrigin_;
	candidate_ = State::Unknown;
	return change;
}
State TrackBoundary(BoundaryTracker& tracker, const World& world, Position previous,
	const Probe& terrain, Position axis, const Settings& settings, uint64_t now) {
	// A late poll must not turn several ordinary collision steps along an
	// oblique wall into one >1.5-unit discontinuity. Inspect the intervening
	// integer origins in the same snapshot; do not generate intermediate cues.
	const Position delta{world.player.x - previous.x, world.player.y - previous.y};
	const int steps = static_cast<int>(std::ceil(std::max(std::abs(delta.x), std::abs(delta.y))));
	if (steps < 2) return tracker.Observe(terrain, axis, now, settings.stableMs, settings.boundaryRange);
	const auto origin = [&](int step) {
		return Position{std::round(previous.x + delta.x * step / steps), std::round(previous.y + delta.y * step / steps)};
	};
	bool traversable = true;
	for (int step = 1; step < steps; ++step) {
		const auto p = origin(step);
		if (world.Body(static_cast<int>(p.x), static_cast<int>(p.y), Layer::Terrain) != State::Open) { traversable = false; break; }
	}
	// Collision sliding may take another path between observations. Never
	// invent intermediate origins inside obstacles or unloaded rooms.
	if (traversable) for (int step = 1; step < steps; ++step) {
		const auto probe = SweepBody(world, origin(step), axis,
			settings.boundaryRange + std::abs(axis.x) + std::abs(axis.y), settings.nearRange, Layer::Terrain);
		(void)tracker.Observe(probe, axis, now, settings.stableMs, settings.boundaryRange, false);
	}
	return tracker.Observe(terrain, axis, now, settings.stableMs, settings.boundaryRange);
}
bool BoundaryTracker::Accepts(Boundary target, State change) const {
	if (target.end == State::Unknown || current_.end == State::Unknown) return false;
	// A further change in the same direction remains meaningful. A returned
	// wall or a newly receded wall invalidates the opposite queued description.
	const State difference = Difference(target, current_);
	return difference == State::Unknown || difference == change;
}
}
