#include "beacon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace D2RAccess::Beacon {
namespace {
// Native movement trace 0x364700 allows diagonal destination steps.
struct Step { int x, y; double length; };
const std::array<Step, 8> Steps{{{-1,0,1},{1,0,1},{0,-1,1},{0,1,1},
	{-1,-1,std::numbers::sqrt2},{1,-1,std::numbers::sqrt2},{-1,1,std::numbers::sqrt2},{1,1,std::numbers::sqrt2}}};
constexpr uint16_t Blank = 0x20; // native unknown subtile, as World::Cell reads it
constexpr int MinRadius = 16;    // always search at least the local neighbourhood

int Cell(double value) { return static_cast<int>(std::lround(value)); }
double Length(Position p) { return std::hypot(p.x, p.y); }

// A flat copy of the collision inside one box. The World keeps a list of
// rooms and scans it linearly per lookup, which a whole-box search would pay
// for on every cell; blitting each room's rectangle once costs one pass.
struct Field {
	int x = 0, y = 0, width = 0, height = 0;
	std::vector<uint8_t> open, covered; // a room covering a point owns it, as World::Cell does
	bool Inside(int cx, int cy) const { return cx >= x && cy >= y && cx < x+width && cy < y+height; }
	size_t Index(int cx, int cy) const { return static_cast<size_t>(cy-y)*width + (cx-x); }
};

Field Copy(const Radar::World& world, int left, int top, int width, int height) {
	const size_t cells = static_cast<size_t>(width) * height;
	Field field{left, top, width, height, std::vector<uint8_t>(cells, 0), std::vector<uint8_t>(cells, 0)};
	for (const auto& room : world.rooms) {
		const int x0 = std::max(left, room.x), x1 = std::min(left+width, room.x+room.width);
		const int y0 = std::max(top, room.y), y1 = std::min(top+height, room.y+room.height);
		for (int y = y0; y < y1; ++y) {
			for (int x = x0; x < x1; ++x) {
				const size_t source = static_cast<size_t>(y-room.y)*room.width + (x-room.x);
				if (source >= room.cells.size()) continue;
				const size_t index = field.Index(x, y);
				if (field.covered[index]) continue;
				field.covered[index] = 1;
				const uint16_t raw = room.cells[source];
				if (raw & Blank) continue; // unknown: never routed through, never a wall
				// Terrain layer: a monster or another player standing in a
				// doorway must not reroute the beacon.
				field.open[index] = (Radar::TerrainCollisionFlags(raw) & world.walkingMask) ? 0 : 1;
			}
		}
	}
	// The body must fit, exactly as the native footprint dispatch tests it:
	// patterns 1/3/5 are a five-cell cross, 2/4 a 3x3 square, 0 a point.
	if (world.bodyPattern != 0) {
		const bool square = world.bodyPattern % 2 == 0;
		const auto raw = field.open;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				auto& cell = field.open[static_cast<size_t>(y)*width + x];
				if (!cell) continue;
				for (int dy = -1; dy <= 1 && cell; ++dy) {
					for (int dx = -1; dx <= 1; ++dx) {
						if ((dx == 0 && dy == 0) || (!square && dx != 0 && dy != 0)) continue;
						const int nx = x+dx, ny = y+dy;
						// Outside the box the body cannot be proved to fit.
						if (nx < 0 || ny < 0 || nx >= width || ny >= height || !raw[static_cast<size_t>(ny)*width + nx]) {
							cell = 0;
							break;
						}
					}
				}
			}
		}
	}
	return field;
}
}

uint64_t PingIntervalMs(double remaining, const Settings& settings) {
	const double reach = settings.farDistance > 0 ? settings.farDistance : 1;
	const double part = std::clamp(remaining / reach, 0.0, 1.0);
	const double near = static_cast<double>(settings.nearPingMs), far = static_cast<double>(settings.farPingMs);
	return static_cast<uint64_t>(std::llround(near + part * (far - near)));
}

Route FindRoute(const Radar::World& world, Position target, const Settings& settings) {
	Route route;
	const Position straight{target.x - world.player.x, target.y - world.player.y};
	const int radius = static_cast<int>(std::clamp(std::ceil(Length(straight)) + settings.lookAhead,
		static_cast<double>(MinRadius), settings.searchRadius));
	const int startX = Cell(world.player.x), startY = Cell(world.player.y);
	const int side = 2*radius + 1;
	const Field field = Copy(world, startX - radius, startY - radius, side, side);
	const int goalX = Cell(target.x), goalY = Cell(target.y);
	const size_t start = field.Index(startX, startY);
	std::vector<int32_t> parent(field.open.size(), -1);
	std::vector<double> length(field.open.size(), 0);
	std::vector<size_t> queue{start};
	// The native trace never retests the origin: a body overlapping its own
	// cell must still be able to leave it.
	parent[start] = static_cast<int32_t>(start);
	size_t best = start;
	double bestDistance = std::hypot(static_cast<double>(startX-goalX), static_cast<double>(startY-goalY));
	for (size_t head = 0; head < queue.size(); ++head) {
		const size_t index = queue[head];
		const int x = field.x + static_cast<int>(index % field.width);
		const int y = field.y + static_cast<int>(index / field.width);
		for (const auto step : Steps) {
			const int nx = x + step.x, ny = y + step.y;
			if (!field.Inside(nx, ny)) continue;
			const size_t next = field.Index(nx, ny);
			if (parent[next] >= 0 || !field.open[next]) continue;
			parent[next] = static_cast<int32_t>(index);
			length[next] = length[index] + step.length;
			const double distance = std::hypot(static_cast<double>(nx-goalX), static_cast<double>(ny-goalY));
			if (distance < bestDistance) { bestDistance = distance; best = next; }
			if (nx == goalX && ny == goalY) { route.reachable = true; best = next; head = queue.size(); break; }
			queue.push_back(next);
		}
	}
	if (best == start) return route; // nothing reachable leads toward the target
	// Walk the parents back to the player, then read the route forwards.
	for (size_t index = best; index != start; index = static_cast<size_t>(parent[index]))
		route.path.push_back({static_cast<double>(field.x + static_cast<int>(index % field.width)),
			static_cast<double>(field.y + static_cast<int>(index / field.width))});
	std::reverse(route.path.begin(), route.path.end());
	route.length = length[best];
	// Aim where the route will be in a few cells, not at the adjacent cell:
	// that is what turns a corner early instead of hugging it.
	Position aim = route.path.back();
	double travelled = 0;
	Position previous{static_cast<double>(startX), static_cast<double>(startY)};
	for (const auto point : route.path) {
		travelled += Length({point.x - previous.x, point.y - previous.y});
		previous = point;
		if (travelled >= settings.lookAhead) { aim = point; break; }
	}
	const Position heading{aim.x - world.player.x, aim.y - world.player.y};
	const double distance = Length(heading);
	if (distance <= 0) return route;
	route.heading = {heading.x / distance, heading.y / distance};
	route.known = true;
	return route;
}

void Tracker::Lock(uint64_t now) {
	*this = {};
	locked_ = true;
	nextPingMs_ = now;
	first_ = true;
}
void Tracker::Clear() { *this = {}; }

bool Reaches(const Radar::World& world, const Route& route, Position target, const Settings& settings) {
	if (Length({target.x - world.player.x, target.y - world.player.y}) <= settings.arrivalDistance) return true;
	if (!route.known || route.path.empty()) return false;
	const auto end = route.path.back();
	return Length({target.x - end.x, target.y - end.y}) <= settings.arrivalDistance;
}

Update Tracker::Tick(const Radar::World& world, Position target, uint64_t now, const Settings& settings,
	const Position* waypoint) {
	Update update;
	if (!locked_) return update;
	update.valid = true;
	const Position straight{target.x - world.player.x, target.y - world.player.y};
	update.distance = Length(straight);
	update.arrived = update.distance <= settings.arrivalDistance;
	if (first_) { first_ = false; nextPingMs_ = now + settings.lockDelayMs; } // room for the spoken name
	// Work per second stays bounded as the lead gets longer: the search box
	// grows with the distance, so a level-wide route is recomputed in
	// proportion less often instead of at the near-target cadence.
	const double stretch = settings.farDistance > 0
		? std::max(1.0, update.distance / settings.farDistance) : 1.0;
	const double rerouteAt = settings.reroute * stretch;
	const auto intervalMs = static_cast<uint64_t>(std::llround(static_cast<double>(settings.routeIntervalMs) * stretch));
	const Position destination = waypoint != nullptr ? *waypoint : target;
	const bool moved = Length({world.player.x - routeOrigin_.x, world.player.y - routeOrigin_.y}) >= rerouteAt
		|| Length({destination.x - routeTarget_.x, destination.y - routeTarget_.y}) >= rerouteAt;
	if (!routeMs_ || moved || now < routeMs_ || now - routeMs_ >= intervalMs) {
		route_ = FindRoute(world, destination, settings);
		routeOrigin_ = world.player;
		routeTarget_ = destination;
		routeMs_ = std::max<uint64_t>(now, 1);
	}
	if (settings.requireKnownRoute && !update.arrived && !Reaches(world, route_, destination, settings)) {
		update.routeUnavailable = true;
		return update;
	}
	routed_ = route_.known;
	update.routed = route_.known;
	update.heading = route_.known ? route_.heading : Position{};
	if (!update.routed && update.distance > 0) update.heading = {straight.x / update.distance, straight.y / update.distance};
	// Reaching a waypoint is not reaching the target, so the cadence keeps
	// speaking of the whole journey exactly as it does for any target the
	// collision cannot reach.
	update.remaining = route_.reachable && waypoint == nullptr ? route_.length : update.distance;
	if (now >= nextPingMs_) {
		update.ping = true;
		nextPingMs_ = now + PingIntervalMs(update.remaining, settings);
	}
	return update;
}
}
