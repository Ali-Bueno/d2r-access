#include "radar.h"
#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
namespace {
double Dot(Position a, Position b) { return a.x*b.x + a.y*b.y; }
Position Delta(Position a, Position b) { return {a.x-b.x,a.y-b.y}; }
// Rejected native body centres have at most half a cell diagonal of
// quantization error against a fitted supporting wall line.
const double RasterBand = std::sqrt(0.5);
}
void WallMemory::Remember(const Probe& wall, Position axis) {
	point_ = wall.hit;
	normal_ = wall.surfaceNormal;
	supported_ = Dot(normal_,normal_) > 0 && std::abs(Dot(normal_,axis)) > 0;
	if (!supported_) normal_ = axis;
	if (Dot(normal_,axis) < 0) normal_ = {-normal_.x,-normal_.y};
	known_ = true;
	clearing_ = cleared_ = false;
}
bool WallMemory::Update(const Probe& wall, Position player, Position axis, bool near,
	uint64_t now, const Settings& settings, int bodyPattern) {
	requiredDepth_ = settings.openDepth;
	if (wall.end == State::Blocked && (!known_ || near)) { Remember(wall,axis); return false; }
	if (!known_) return true; // no previous barrier for a newly observed passage
	if (wall.end == State::Blocked) {
		const auto delta = Delta(wall.hit,point_);
		const bool currentSupported = Dot(wall.surfaceNormal,wall.surfaceNormal) > 0;
		const bool onCurrent = currentSupported && std::abs(Dot(delta,wall.surfaceNormal)) <= RasterBand;
		// Transport the reference along an observed wall, including when
		// retreat moves the ray hit tangentially on an angled surface.
		const bool onReference = supported_ ? std::abs(Dot(delta,normal_)) <= RasterBand
			: currentSupported ? onCurrent : std::abs(Dot(delta,axis)) <= RasterBand;
		if (onReference
			&& (!currentSupported || onCurrent)) {
			if (currentSupported) Remember(wall,axis);
			else { point_ = wall.hit; clearing_ = cleared_ = false; }
			return false;
		}
	}
	// Moving the player does not move this plane. Even after the old wall
	// leaves the local map window, eight free subtiles before it prove no exit.
	const double referenceDepth = Dot(Delta(point_,player),normal_) / Dot(axis,normal_);
	const double bodySpan = bodyPattern == 0 ? 1.0 : 3.0;
	requiredDepth_ = std::max(settings.openDepth,referenceDepth+bodySpan);
	const bool beyond = wall.clearDepth >= requiredDepth_;
	if (!beyond) { clearing_ = cleared_ = false; return false; }
	if (!clearing_) { clearing_ = true; since_ = now; }
	if (now > since_ && now-since_ >= settings.stableMs) cleared_ = true;
	return cleared_;
}
}
