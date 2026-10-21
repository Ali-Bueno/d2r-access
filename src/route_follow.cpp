#include "route_follow.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace D2RAccess::RouteFollow {
namespace {
// Policy: the next leg is said this long before its turn, the time to hear it
// and turn: about one stride of a run (TurnUnits) at a run, less at a walk, as
// a leg of the stick's ways ends on the line of a doorway. Before the pace is
// known, a run's. Never before half the leg is walked: a short leg, a step round
// a grave, was skipped at the start (a tester, 2026-10-20).
constexpr double TurnSeconds = 0.5, TurnUnits = 3, NearestTurnUnits = 1;
// Policy: the route is left when the player is further from every leg still to
// walk than this, longer than OffRouteMs. Wider than a corridor and a dodge.
constexpr double OffRouteUnits = 8;
constexpr uint64_t OffRouteMs = 2000;
// Policy: the end is reached this near the way's last cell.
constexpr double EndUnits = 3;

double Distance(Position a, Position b) { return std::hypot(a.x - b.x, a.y - b.y); }

// From `point` to the segment from `a` to `b`.
double ToSegment(Position point, Position a, Position b) {
	const double dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
	if (length <= 0) return Distance(point, a);
	const double along = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length, 0.0, 1.0);
	return Distance(point, {a.x + dx * along, a.y + dy * along});
}
}

void Follower::Start(Position from, const std::vector<RouteLegs::Leg>& legs) {
	*this = {};
	if (legs.empty()) return;
	legs_ = legs;
	corners_.push_back(from);
	for (const auto& leg : legs) corners_.push_back({corners_.back().x + leg.delta.x, corners_.back().y + leg.delta.y});
	active_ = true;
}

Step Follower::Tick(Position player, uint64_t now) {
	if (!active_) return {};
	if (timed_ && now > lastMs_) {
		const double pace = Distance(player, last_) * 1000 / static_cast<double>(now - lastMs_);
		pace_ = pace_ < 0 ? pace : (pace_ + pace) / 2;
	}
	timed_ = true;
	last_ = player;
	lastMs_ = now;
	if (Distance(player, corners_.back()) <= EndUnits) {
		Stop();
		return {Event::End};
	}
	// The leg still to walk that the player is nearest; a door is a point on the way, not a leg to be on.
	size_t nearest = at_;
	double best = std::numeric_limits<double>::infinity();
	for (size_t leg = at_; leg < legs_.size(); ++leg) {
		if (legs_[leg].door) continue;
		const double distance = ToSegment(player, corners_[leg], corners_[leg + 1]);
		if (distance < best) { best = distance; nearest = leg; }
	}
	if (best > OffRouteUnits) {
		if (!offSince_) offSince_ = std::max<uint64_t>(now, 1);
		else if (now >= offSince_ && now - offSince_ >= OffRouteMs) {
			Stop();
			return {Event::Off};
		}
		return {};
	}
	offSince_ = 0;
	size_t next = std::max(at_, nearest);
	const double lead = std::min(Distance(player, corners_[at_]),
		pace_ < 0 ? TurnUnits : std::clamp(pace_ * TurnSeconds, NearestTurnUnits, TurnUnits));
	if (at_ + 1 < legs_.size() && Distance(player, corners_[at_ + 1]) <= lead) next = std::max(next, at_ + 1);
	if (next == at_) return {};
	// A door has no length: the leg after it is walked from the same corner.
	at_ = legs_[next].door && next + 1 < legs_.size() ? next + 1 : next;
	return {Event::Turn, next};
}
}
