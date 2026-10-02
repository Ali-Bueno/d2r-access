#include "passages.h"

namespace D2RAccess::Passages {
namespace {
struct Step { int dx, dy; };
constexpr Step AlongX{1, 0}, AlongY{0, 1};

// The open run through (x, y) across `step`: its length, and whether both
// ends are walls. A run that ends on ground the game has not streamed proves
// nothing about how wide the floor is.
struct Run { int length = 0; bool walled = false; };
Run RunThrough(const Probe& probe, int x, int y, Step step, int reach) {
	Run run{1, true};
	for (int sign : {-1, 1}) {
		int i = 1;
		for (; i <= reach; ++i) {
			const Ground ground = probe(x + sign * i * step.dx, y + sign * i * step.dy);
			if (ground == Ground::Blocked) break;
			if (ground == Ground::Unknown) { run.walled = false; break; }
			++run.length;
		}
		if (i > reach) run.walled = false;
	}
	return run;
}

// Narrow across `across`: walls on both sides, as far apart as a corridor's.
bool Narrow(const Probe& probe, int x, int y, Step across, const Settings& settings) {
	const Run run = RunThrough(probe, x, y, across, settings.reach);
	return run.walled && run.length >= settings.minWidth && run.length <= settings.maxWidth;
}

Step Across(Step along) { return along.dx ? AlongY : AlongX; }

// Follows the narrow floor from (x, y) along `along` in one direction.
struct Walk { int steps = 0; Way way; };
Walk Follow(const Probe& probe, int x, int y, Step along, int sign, const Settings& settings) {
	Walk walk{0, {x, y, End::Unknown}};
	for (int i = 1; i <= settings.reach; ++i) {
		const int cx = x + sign * i * along.dx, cy = y + sign * i * along.dy;
		const Ground ground = probe(cx, cy);
		if (ground == Ground::Unknown) return walk;
		if (ground == Ground::Blocked) { walk.way.end = End::Closed; return walk; }
		if (!Narrow(probe, cx, cy, Across(along), settings)) { walk.way.end = End::Opens; return walk; }
		walk.steps = i;
		walk.way.x = cx;
		walk.way.y = cy;
	}
	return walk;
}

// A corridor cell along `along`: narrow across it, for at least minLength cells.
bool CorridorAt(const Probe& probe, int x, int y, Step along, const Settings& settings, Reading* reading = nullptr) {
	if (probe(x, y) != Ground::Open || !Narrow(probe, x, y, Across(along), settings)) return false;
	const Walk back = Follow(probe, x, y, along, -1, settings), ahead = Follow(probe, x, y, along, 1, settings);
	if (back.steps + ahead.steps + 1 < settings.minLength) return false;
	if (reading) {
		reading->shape = Shape::Corridor;
		reading->alongX = along.dx != 0;
		reading->ends = {back.way, ahead.way};
	}
	return true;
}

// The run through (x, y) along `along` of floor that is not narrow across it.
// Returns how far it reaches each way, or -1 when it is not small.
std::array<int, 2> OpenExtent(const Probe& probe, int x, int y, Step along, const Settings& settings) {
	std::array<int, 2> extent{0, 0};
	const int limit = settings.maxWidth + settings.slack;
	for (int side = 0; side < 2; ++side) {
		const int sign = side ? 1 : -1;
		for (int i = 1;; ++i) {
			const int cx = x + sign * i * along.dx, cy = y + sign * i * along.dy;
			if (probe(cx, cy) != Ground::Open || Narrow(probe, cx, cy, Across(along), settings)) break;
			if (++extent[static_cast<size_t>(side)] > limit) return {-1, -1};
		}
	}
	if (extent[0] + extent[1] + 1 > limit) return {-1, -1};
	return extent;
}
}

Reading Classify(const Probe& probe, int x, int y, const Settings& settings) {
	Reading reading;
	if (probe(x, y) != Ground::Open) return reading;
	if (CorridorAt(probe, x, y, AlongY, settings, &reading) || CorridorAt(probe, x, y, AlongX, settings, &reading))
		return reading;
	// A bend or junction is a small patch of wider floor with corridors leaving it.
	const auto alongX = OpenExtent(probe, x, y, AlongX, settings), alongY = OpenExtent(probe, x, y, AlongY, settings);
	if (alongX[0] < 0 || alongY[0] < 0) return reading;
	bool armX[2]{}, armY[2]{};
	for (int side = 0; side < 2; ++side) {
		const int sign = side ? 1 : -1;
		armX[side] = CorridorAt(probe, x + sign * (alongX[static_cast<size_t>(side)] + 1), y, AlongX, settings);
		armY[side] = CorridorAt(probe, x, y + sign * (alongY[static_cast<size_t>(side)] + 1), AlongY, settings);
	}
	const int xs = armX[0] + armX[1], ys = armY[0] + armY[1];
	reading.arms = xs + ys;
	if (reading.arms >= 3) reading.shape = Shape::Junction;
	else if (xs == 1 && ys == 1) reading.shape = Shape::Bend;
	return reading;
}
}
