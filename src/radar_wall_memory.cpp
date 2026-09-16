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
bool FollowsContour(const World& world, Position from, Position to, Position axis) {
	// Follow actual boundary cells, not an infinite supporting line. The
	// narrow strip allows the combined raster error of two body-centre hits;
	// it cannot take an arbitrary detour around a room or down a doorway.
	const auto delta = Delta(to,from);
	const double length = std::hypot(delta.x,delta.y);
	if (!length || world.Body(static_cast<int>(from.x),static_cast<int>(from.y),Layer::Terrain) != State::Blocked)
		return false;
	constexpr int CellHalo = 1;
	const int left = static_cast<int>(std::min(from.x,to.x))-CellHalo;
	const int top = static_cast<int>(std::min(from.y,to.y))-CellHalo;
	const int width = static_cast<int>(std::abs(delta.x))+2*CellHalo+1;
	const int height = static_cast<int>(std::abs(delta.y))+2*CellHalo+1;
	std::vector<bool> visited(static_cast<size_t>(width)*height);
	std::vector<Position> queue{from};
	const auto index = [&](Position p) { return static_cast<size_t>(p.y-top)*width+static_cast<size_t>(p.x-left); };
	visited[index(from)] = true;
	for (size_t cursor=0;cursor<queue.size();++cursor) {
		const auto point = queue[cursor];
		for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
			const Position next{point.x+dx,point.y+dy};
			if (next.x<left || next.y<top || next.x>=left+width || next.y>=top+height || visited[index(next)]) continue;
			visited[index(next)] = true;
			const auto offset = Delta(next,from);
			if (std::abs(offset.x*delta.y-offset.y*delta.x)/length > 2*RasterBand) continue;
			const int x = static_cast<int>(next.x), y = static_cast<int>(next.y);
			if (world.Body(x,y,Layer::Terrain) != State::Blocked) continue;
			bool boundary = false;
			for (int by=-1;by<=1 && !boundary;++by) for (int bx=-1;bx<=1;++bx)
				if (bx*axis.x+by*axis.y < 0 && world.Body(x+bx,y+by,Layer::Terrain) == State::Open) { boundary = true; break; }
			if (!boundary) continue;
			if (next.x==to.x && next.y==to.y) return true;
			queue.push_back(next);
		}
	}
	return false;
}
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
bool WallMemory::ObserveBarrier(uint64_t now, uint64_t confirmationMs) {
	if (!barrierObserved_) { barrierObserved_ = true; barrierSince_ = now; }
	barrierConfirmed_ = now > barrierSince_ && now-barrierSince_ >= confirmationMs;
	return false;
}
bool WallMemory::Update(const Probe& wall, Position player, Position axis, bool near,
	uint64_t now, const Settings& settings, const World* structure) {
	requiredDepth_ = settings.openDepth;
	barrierConfirmed_ = false;
	if (wall.end == State::Blocked && (!known_ || near)) {
		Remember(wall,axis); return ObserveBarrier(now,settings.stableMs);
	}
	if (!known_) return true; // no previous barrier for a newly observed passage
	const double referenceDepth = Dot(Delta(point_,player),normal_) / Dot(axis,normal_);
	if (wall.end == State::Blocked) {
		const auto delta = Delta(wall.hit,point_);
		const bool currentSupported = Dot(wall.surfaceNormal,wall.surfaceNormal) > 0;
		const bool onCurrent = currentSupported && std::abs(Dot(delta,wall.surfaceNormal)) <= RasterBand;
		// Transport the reference along an observed wall, including when
		// retreat moves the ray hit tangentially on an angled surface.
		const bool onReference = supported_ ? std::abs(Dot(delta,normal_)) <= RasterBand
			: currentSupported ? onCurrent : std::abs(Dot(delta,axis)) <= RasterBand;
		if ((onReference && (!currentSupported || onCurrent))
			|| (structure && FollowsContour(*structure,point_,wall.hit,axis))) {
			if (currentSupported) Remember(wall,axis);
			else { point_ = wall.hit; clearing_ = cleared_ = false; }
			return ObserveBarrier(now,settings.stableMs);
		}
		// A real nearer first obstruction supersedes the old extrapolated
		// plane. Keeping the old plane until contact would hide doorways in
		// this new wall on the first pass, then reveal them on the return.
		const double hitDepth = Dot(Delta(wall.hit,player),axis);
		if (hitDepth + RasterBand < referenceDepth) {
			Remember(wall,axis); return ObserveBarrier(now,settings.stableMs);
		}
	}
	barrierObserved_ = false;
	// Moving the player does not move this plane. Even after the old wall
	// leaves the local map window, eight free subtiles before it prove no exit.
	// Both the reference and scan already include the complete native body.
	// Prove one further native destination step, without inflating it twice.
	const double destinationStep = std::abs(axis.x) + std::abs(axis.y);
	requiredDepth_ = std::max(settings.openDepth,referenceDepth+destinationStep);
	const bool beyond = wall.clearDepth >= requiredDepth_;
	if (!beyond) { clearing_ = cleared_ = false; return false; }
	if (!clearing_) { clearing_ = true; since_ = now; }
	if (now > since_ && now-since_ >= settings.stableMs) cleared_ = true;
	return cleared_;
}
}
