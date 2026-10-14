#include "route_legs.h"

#include "entity_radar.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace D2RAccess::RouteLegs {
namespace {
// Policy: a leg shorter than the body is wide is a jog round a corner, not a turn to say.
constexpr double MinLegUnits = 3;
// Policy: past this many legs the rest is only counted; asked again on the way,
// the route starts from where the player stands.
constexpr size_t SpokenLegs = 6;
// Half-subtile samples visit every cell a line crosses, as the scan's sight ray does.
constexpr double SampleStep = 0.5;

int Cell(double coordinate) { return static_cast<int>(std::lround(coordinate)); }

bool Clear(Position from, Position to, const Passable& passable) {
	const double dx = to.x - from.x, dy = to.y - from.y, length = std::hypot(dx, dy);
	for (double depth = SampleStep; depth < length; depth += SampleStep)
		if (!passable(Cell(from.x + dx / length * depth), Cell(from.y + dy / length * depth))) return false;
	return true;
}

// points[first..last] as straight legs: from each corner, as far along the way
// as a clear straight line goes. The search's own cell-by-cell zigzag goes.
void Straighten(const std::vector<Position>& points, size_t first, size_t last, const Passable& passable, std::vector<Leg>& legs) {
	for (size_t at = first; at < last;) {
		size_t next = at + 1;
		while (next < last && Clear(points[at], points[next + 1], passable)) ++next;
		const Position delta{points[next].x - points[at].x, points[next].y - points[at].y};
		legs.push_back({delta, std::hypot(delta.x, delta.y)});
		at = next;
	}
}

// Neighbouring legs said in the same direction are one leg.
std::vector<Leg> Merge(const std::vector<Leg>& legs, SpokenDirection::Style style) {
	std::vector<Leg> merged;
	for (const auto& leg : legs) {
		auto* last = merged.empty() ? nullptr : &merged.back();
		if (last && !last->door && !leg.door
			&& SpokenDirection::Bucket(last->delta, style) == SpokenDirection::Bucket(leg.delta, style)) {
			last->delta = {last->delta.x + leg.delta.x, last->delta.y + leg.delta.y};
			last->length += leg.length;
		} else merged.push_back(leg);
	}
	return merged;
}

// A short leg is walked as part of the one before it, or of the one after
// when nothing to walk comes before it.
std::vector<Leg> Fold(std::vector<Leg> legs) {
	for (size_t i = 0; i < legs.size();) {
		const bool before = i > 0 && !legs[i - 1].door, after = i + 1 < legs.size() && !legs[i + 1].door;
		if (legs[i].door || legs[i].length >= MinLegUnits || (!before && !after)) { ++i; continue; }
		(before ? legs[i - 1] : legs[i + 1]).length += legs[i].length;
		legs.erase(legs.begin() + static_cast<std::ptrdiff_t>(i));
	}
	return legs;
}
}

std::vector<Leg> Describe(Position from, const std::vector<Position>& path, const std::vector<size_t>& doors,
	const Passable& passable, SpokenDirection::Style style) {
	std::vector<Leg> legs;
	if (path.empty()) return legs;
	std::vector<Position> points{{static_cast<double>(Cell(from.x)), static_cast<double>(Cell(from.y))}};
	points.insert(points.end(), path.begin(), path.end());
	size_t at = 0, lastDoor = path.size();
	for (const size_t door : doors) {
		// A door is opened at the first of its cells on the way; the rest are crossed with it.
		const bool sameDoor = lastDoor != path.size() && door == lastDoor + 1;
		lastDoor = door;
		if (sameDoor || door >= path.size() || door + 1 <= at) continue;
		Straighten(points, at, door + 1, passable, legs);
		legs.push_back({{}, 0, true});
		at = door + 1;
	}
	Straighten(points, at, points.size() - 1, passable, legs);
	return Merge(Fold(Merge(legs, style)), style);
}

double Length(const std::vector<Leg>& legs) {
	double length = 0;
	for (const auto& leg : legs) length += leg.length;
	return length;
}

std::string Speak(const std::string& name, const std::vector<Leg>& legs, bool unseen, const Strings::Table& strings) {
	std::string said;
	const size_t shown = std::min(legs.size(), SpokenLegs);
	for (size_t i = 0; i < shown; ++i) {
		if (i) said += strings.routeThen;
		if (legs[i].door) { said += strings.routeDoor; continue; }
		char leg[160]{};
		std::snprintf(leg, sizeof leg, strings.routeLeg, SpokenDirection::Of(legs[i].delta, strings),
			std::max(1, static_cast<int>(std::lround(legs[i].length))));
		said += leg;
	}
	if (legs.size() > shown) {
		char more[96]{};
		std::snprintf(more, sizeof more, strings.routeMore, static_cast<int>(legs.size() - shown));
		said += strings.routeThen;
		said += more;
	}
	const auto total = EntityRadar::DistancePhrase(Length(legs), EntityRadar::Settings{}, strings);
	std::string text(name.size() + total.size() + said.size() + 128, '\0');
	const int written = std::snprintf(text.data(), text.size(), strings.routeTo, name.c_str(), total.c_str(), said.c_str());
	text.resize(written > 0 ? std::min(static_cast<size_t>(written), text.size() - 1) : 0);
	if (unseen) {
		text += strings.terrainSeparator;
		text += strings.routeUnseen;
	}
	return text;
}
}
