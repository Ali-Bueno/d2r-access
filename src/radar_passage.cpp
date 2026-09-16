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
	observed_ = reported_ = player;
	target_ = {player.x+axis.x*depth,player.y+axis.y*depth};
	approaches_.push_back({point_,target_});
}
void OpeningVisit::Update(Position player, Position axis, const Probe& structure, const Probe& movement,
	bool aligned, int bodyPattern, uint64_t now, const Settings& settings) {
	ready_ = valid_ = false;
	observed_ = player;
	if (!known_) return;
	double distance = std::hypot(player.x-point_.x,player.y-point_.y);
	Position target = target_;
	// Opposite edges of the same proved opening are one approach region.
	// Distance to its observed segments prevents recalling the first edge
	// immediately after announcing entry through the other edge.
	for (const auto& approach : approaches_) {
		const auto& parent = approaches_[approach.parent];
		const Position delta{approach.point.x-parent.point.x,approach.point.y-parent.point.y};
		const double length2 = delta.x*delta.x+delta.y*delta.y;
		const double t = length2 ? std::clamp(((player.x-parent.point.x)*delta.x+(player.y-parent.point.y)*delta.y)/length2,0.0,1.0) : 0;
		const double candidate = std::hypot(player.x-parent.point.x-t*delta.x,player.y-parent.point.y-t*delta.y);
		if (candidate <= distance) {
			distance = candidate;
			target = {parent.target.x+t*(approach.target.x-parent.target.x),parent.target.y+t*(approach.target.y-parent.target.y)};
		}
	}
	const double span = BodySpan(bodyPattern);
	// Spatial hysteresis is tied to the native footprint: leave its full
	// span, return within its radius. An actually moved cell losing the
	// structural approach also proves departure from a narrow mouth.
	constexpr double NativeCell = 1;
	if (distance >= span || (std::hypot(player.x-reported_.x,player.y-reported_.y) >= NativeCell && structure.end == State::Blocked
		&& structure.clearDepth < settings.openDepth)) departed_ = true;
	const double required = std::max(settings.openDepth,(target.x-player.x)*axis.x+(target.y-player.y)*axis.y);
	valid_ = distance <= span/2 && aligned && structure.clearDepth >= required && movement.clearDepth >= required;
	if (!valid_ || !departed_) { arriving_ = false; return; }
	// Time confirms a return; elapsed time alone never repeats a cue.
	if (!arriving_) { arriving_ = true; since_ = now; }
	ready_ = now > since_ && now-since_ >= settings.stableMs;
}
bool OpeningVisit::Join(Position player, Position axis, double depth, const World& structure) {
	const Position target{player.x+axis.x*depth,player.y+axis.y*depth};
	const auto open=[&](Position point) {
		return structure.Body(static_cast<int>(std::round(point.x)),static_cast<int>(std::round(point.y)),Layer::Terrain)==State::Open;
	};
	if (!open(player) || !open(target)) return false;
	for (size_t parent=0;parent<approaches_.size();++parent) {
		const auto& from=approaches_[parent];
		if (!open(from.point) || !open(from.target)) continue;
		const auto interpolate=[](Position a,Position b,double t) { return Position{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; };
		// Rasterize the strip between the two full-body approaches. Every
		// connecting lane must be clear; a wall between separate doorways or
		// missing coverage prevents merging. One-cell sampling follows the
		// native destination grid, and each query checks the whole body.
		const int lanes=std::max(1,static_cast<int>(std::ceil(std::max({std::abs(player.x-from.point.x),std::abs(player.y-from.point.y),
			std::abs(target.x-from.target.x),std::abs(target.y-from.target.y)}))));
		bool connected=true;
		for (int lane=0;connected && lane<=lanes;++lane) {
			const double t=double(lane)/lanes;
			const auto begin=interpolate(from.point,player,t), end=interpolate(from.target,target,t);
			const int steps=std::max(1,static_cast<int>(std::ceil(std::max(std::abs(end.x-begin.x),std::abs(end.y-begin.y)))));
			for (int step=0;connected && step<=steps;++step) connected=open(interpolate(begin,end,double(step)/steps));
		}
		if (!connected) continue;
		approaches_.push_back({player,target,parent});
		point_=observed_=player; target_=target; valid_=true; Reported();
		return true;
	}
	return false;
}
void OpeningMemory::Remember(Position player, Position axis, double depth, const World* structure) {
	// Keep the original anchor when another observation describes the same
	// body-sized approach. Repeated passes must not walk the memory away.
	double nearest = radius_;
	size_t match = visits_.size();
	for (size_t i=0;i<visits_.size();++i) {
		const auto point = visits_[i].Point();
		const double distance = std::hypot(player.x-point.x,player.y-point.y);
		if (distance <= nearest) { nearest=distance; match=i; }
	}
	if (match == visits_.size()) {
		if (structure) for (size_t i=0;i<visits_.size();++i)
			if (visits_[i].Join(player,axis,depth,*structure)) { match=i; break; }
		if (match == visits_.size()) {
			visits_.emplace_back();
			visits_.back().Remember(player,axis,depth);
		}
	}
	announced_ = match;
	for (auto& visit : visits_) if (visit.Valid()) visit.Reported();
	ready_ = visits_.size();
}
void OpeningMemory::Update(Position player, Position axis, const Probe& structure, const Probe& movement,
	bool aligned, int bodyPattern, uint64_t now, const Settings& settings) {
	radius_ = BodySpan(bodyPattern)/2;
	ready_ = visits_.size();
	double nearest = radius_;
	for (size_t i=0;i<visits_.size();++i) {
		auto& visit = visits_[i];
		visit.Update(player,axis,structure,movement,aligned,bodyPattern,now,settings);
		const auto point = visit.Point();
		const double distance = std::hypot(player.x-point.x,player.y-point.y);
		if (visit.Ready() && (ready_ == visits_.size() || distance <= nearest)) { nearest=distance; ready_=i; }
	}
}
void OpeningMemory::Reported() {
	if (!Ready()) return;
	announced_ = ready_;
	// Overlapping approach footprints are one visit at this position. Consume
	// all valid arrivals together, so two nearby anchors cannot echo at rest.
	for (auto& visit : visits_) if (visit.Valid()) visit.Reported();
	ready_ = visits_.size();
}
void OpeningMemory::Suspend() {
	for (auto& visit : visits_) visit.Suspend();
	ready_ = visits_.size();
}
}
