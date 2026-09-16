#include "radar_passage.h"
#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
namespace {
// The native footprints are one cell (pattern 0), or three cells across.
double BodySpan(int pattern) { return pattern == 0 ? 1.0 : 3.0; }
constexpr std::array<Position, 8> Steps{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
}
int PassageRadius(const Settings& settings, int bodyPattern) {
	return static_cast<int>(std::ceil(settings.openDepth + BodySpan(bodyPattern))) + 1;
}
Passage ScanPassage(const Navigation::Grid& grid, Position axis, const Settings& settings) {
	Passage result;
	// A body span of spatial hysteresis absorbs raster/edge fluctuations.
	// This is a navigation policy derived from the configured useful depth,
	// not a claimed game collision constant (defaults: open at 8, close at 5).
	result.closeDepth = std::max(settings.blockedDepth, settings.openDepth - BodySpan(grid.bodyPattern));
	if (!grid.originKnown) return result;
	const auto projection = [&](Position p) { return p.x*axis.x + p.y*axis.y; };
	const auto lateral = [&](Position p) { return -p.x*axis.y + p.y*axis.x; };
	const auto index = [&](Position p) {
		return static_cast<size_t>(grid.playerY+static_cast<int>(p.y)-grid.y)*grid.width
			+grid.playerX+static_cast<int>(p.x)-grid.x;
	};
	std::vector<Position> queue{{}};
	std::vector<bool> visited(grid.cells.size());
	visited[index({})] = true;
	bool unknown = false;
	for (size_t head = 0; head < queue.size(); ++head) {
		const auto current = queue[head];
		for (const auto step : Steps) {
			// Never label a route requiring a retreat as a forward opening.
			// Its first step must actually follow the named stick axis.
			if (projection(step) < 0 || (head == 0 && (projection(step) <= 0 || lateral(step) != 0))) continue;
			const Position next{current.x+step.x, current.y+step.y};
			if (std::abs(lateral(next)) > BodySpan(grid.bodyPattern)) continue;
			const auto* cell = grid.At(grid.playerX+static_cast<int>(next.x), grid.playerY+static_cast<int>(next.y));
			if (!cell || cell->body == State::Unknown) { unknown = true; continue; }
			if (cell->body != State::Open || visited[index(next)]) continue;
			visited[index(next)] = true;
			result.depth = std::max(result.depth, projection(next));
			if (result.depth >= settings.openDepth) {
				result.available = true; result.observed = State::Open; return result;
			}
			queue.push_back(next);
		}
	}
	// Unloaded cells may hide the rest of the route. They are never walls.
	result.available = !unknown;
	if (result.available && result.depth <= result.closeDepth) result.observed = State::Blocked;
	return result;
}
State PassageTracker::Observe(const Passage& passage, bool openingSafe, uint64_t now, uint64_t confirmationMs) {
	if (!passage.available) { *this = {}; return State::Unknown; }
	const auto observed = passage.observed;
	if (state_ == State::Unknown) {
		// Known limited/offset space starts closed to new opening cues. Its
		// later alignment can be announced without first touching a wall.
		state_ = reported_ = observed == State::Open ? State::Open : State::Blocked;
	} else if (observed == State::Unknown || observed == state_) {
		candidate_ = State::Unknown;
	} else if (candidate_ != observed) {
		candidate_ = observed; since_ = now;
	} else if (now > since_ && now-since_ >= confirmationMs) {
		state_ = observed; candidate_ = State::Unknown;
	}
	// If an occupied opening disappears before it is usable, returning to
	// the already reported closed state must not produce an orphan closing.
	if ((state_ == reported_ && !(state_ == State::Open && rearmed_)) || candidate_ != State::Unknown
		|| (state_ == State::Open && !openingSafe)) return State::Unknown;
	reported_ = state_;
	announcedOpening_ = state_ == State::Open;
	rearmed_ = false;
	return state_;
}
bool PassageTracker::Accepts(State cue, const Passage& passage, bool openingSafe) const {
	return cue != State::Unknown && cue == state_ && passage.available
		&& (passage.observed == State::Unknown || passage.observed == cue)
		&& (cue != State::Open || openingSafe);
}
void OpeningVisit::Remember(Position player, Position axis, double depth) {
	*this = {};
	known_ = valid_ = true;
	point_ = player;
	target_ = {player.x+axis.x*depth,player.y+axis.y*depth};
}
void OpeningVisit::Update(Position player, Position axis, const Probe& structure, const Probe& movement,
	bool aligned, int bodyPattern, uint64_t now, const Settings& settings) {
	ready_ = valid_ = false;
	if (!known_) return;
	const double distance = std::hypot(player.x-point_.x,player.y-point_.y);
	const double span = BodySpan(bodyPattern);
	// Spatial hysteresis is tied to the native footprint: leave its full
	// span, return within its radius. An actually moved cell losing the
	// structural approach also proves departure from a narrow mouth.
	constexpr double NativeCell = 1;
	if (distance >= span || (distance >= NativeCell && structure.end == State::Blocked
		&& structure.clearDepth < settings.openDepth)) departed_ = true;
	const double required = std::max(settings.openDepth,(target_.x-player.x)*axis.x+(target_.y-player.y)*axis.y);
	valid_ = distance <= span/2 && aligned && structure.clearDepth >= required && movement.clearDepth >= required;
	if (!valid_ || !departed_) { arriving_ = false; return; }
	// Time confirms a return; elapsed time alone never repeats a cue.
	if (!arriving_) { arriving_ = true; since_ = now; }
	ready_ = now > since_ && now-since_ >= settings.stableMs;
}
}
