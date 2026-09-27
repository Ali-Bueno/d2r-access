#include "auto_walk.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::AutoWalk {
namespace {
double Distance(Position a, Position b) { return std::hypot(a.x - b.x, a.y - b.y); }
int Cell(double value) { return static_cast<int>(std::lround(value)); }
}

bool Straight(const Radar::World& world, Position from, Position to) {
	const int x0 = Cell(from.x), y0 = Cell(from.y), x1 = Cell(to.x), y1 = Cell(to.y);
	const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
	int previousX = x0, previousY = y0;
	for (int i = 1; i <= steps; ++i) {
		const int x = x0 + static_cast<int>(std::lround(static_cast<double>(x1 - x0) * i / steps));
		const int y = y0 + static_cast<int>(std::lround(static_cast<double>(y1 - y0) * i / steps));
		if (world.Body(x, y, Radar::Layer::Terrain) != Radar::State::Open) return false;
		// A diagonal step between two walls' corners is no way through for a body.
		if (x != previousX && y != previousY
			&& world.Body(x, previousY, Radar::Layer::Terrain) != Radar::State::Open
			&& world.Body(previousX, y, Radar::Layer::Terrain) != Radar::State::Open) return false;
		previousX = x;
		previousY = y;
	}
	return true;
}

Position Leg(const Radar::World& world, const std::vector<Position>& path, const Settings& settings) {
	Position leg = path.front();
	for (const auto point : path) {
		if (Distance(world.player, point) > settings.reach) break;
		if (Straight(world, world.player, point)) leg = point;
	}
	return leg;
}

void Walker::Start() {
	*this = {};
	active_ = true;
}

Step Walker::Tick(const Radar::World& world, const std::vector<Position>& path, uint64_t now, const Settings& settings) {
	Step step;
	if (!active_ || path.empty()) return step;
	if (!ordered_ || Distance(world.player, anchor_) >= settings.stallDistance) {
		// Ground covered since the anchor: the walk is alive.
		if (ordered_) stalls_ = 0;
		anchor_ = world.player;
		anchorMs_ = now;
	}
	const Position leg = Leg(world, path, settings);
	const bool due = !ordered_ || now < orderMs_ || now - orderMs_ >= settings.orderIntervalMs;
	const bool stalled = ordered_ && now >= anchorMs_ && now - anchorMs_ >= settings.stallMs;
	if (stalled) {
		if (++stalls_ >= settings.stallOrders) {
			step.event = Event::Blocked;
			return step;
		}
		anchorMs_ = now; // the next stall is counted from this order
	} else if (!due || (ordered_ && Distance(world.player, order_) > settings.renew
		&& Distance(leg, order_) <= settings.renew)) {
		// Still on the way to an order that is still the right one.
		return step;
	}
	ordered_ = true;
	order_ = leg;
	orderMs_ = now;
	step.event = Event::Order;
	step.order = leg;
	return step;
}
}
