#include "exit_review.h"
#include "spoken_direction.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace D2RAccess::ExitReview {
namespace {
// The same way out re-derived lands on the same collision cell, so one cell of
// the grid is all the tolerance needed to recognise it again.
constexpr double SameCell = 1;

// The map's sides in Radar::Direction order (up, left, right, down): the
// world axes the screen's are turned to, 45 degrees clockwise.
constexpr std::array<Position, Radar::DirectionCount> MapAxes{{{0, -1}, {-1, 0}, {1, 0}, {0, 1}}};

// A quarter of the plane centred on `axis`; a point on a diagonal belongs to
// both neighbouring sides.
bool OnSide(Position player, Position target, Position axis) {
	const double dx = target.x - player.x, dy = target.y - player.y;
	const double length = std::hypot(dx, dy);
	if (length == 0) return true;
	return (dx * axis.x + dy * axis.y) / length >= Radar::Diagonal;
}
}

Position Axis(Radar::Direction direction, Sides sides) {
	const auto index = static_cast<size_t>(direction);
	return sides == Sides::Map ? MapAxes[index] : Radar::Axes[index];
}

Position MapSide(Position offset) {
	if (std::abs(offset.x) >= std::abs(offset.y)) return {offset.x < 0 ? -1.0 : 1.0, 0};
	return {0, offset.y < 0 ? -1.0 : 1.0};
}

Position Wall(const Rooms::Opening& opening) {
	return MapSide({opening.across.x - opening.centre.x, opening.across.y - opening.centre.y});
}

std::vector<Item> Collect(Position player, const std::vector<Rooms::Opening>& openings,
	const std::vector<EntityRadar::Entity>& exits, const Strings::Table& strings, UnexploredExit unexploredExit) {
	std::vector<Item> items;
	items.reserve(openings.size() + exits.size());
	const auto add = [&](std::string label, Position point, Position wall, const EntityRadar::Entity* entity, bool unexplored) {
		items.push_back({std::move(label), point, wall, std::hypot(point.x - player.x, point.y - player.y),
			entity != nullptr, entity ? *entity : EntityRadar::Entity{}, unexplored});
	};
	for (const auto& opening : openings) add(strings.roomOpening, opening.centre, Wall(opening), nullptr, opening.unexplored);
	for (const auto& exit : exits) add(exit.name, exit.position, {}, &exit, unexploredExit && unexploredExit(exit));
	std::stable_sort(items.begin(), items.end(),
		[](const Item& a, const Item& b) { return a.distance < b.distance; });
	return items;
}

std::vector<Item> Toward(const std::vector<Item>& items, Position player, Radar::Direction direction, Sides sides) {
	const auto axis = Axis(direction, sides);
	std::vector<Item> result;
	for (const auto& item : items) {
		// On the map's sides a passage is where its wall is, wherever the player stands in the room.
		const bool walled = sides == Sides::Map && (item.wall.x != 0 || item.wall.y != 0);
		if (walled ? item.wall.x == axis.x && item.wall.y == axis.y : OnSide(player, item.point, axis)) result.push_back(item);
	}
	return result;
}

size_t Cycle::Next(const std::vector<Item>& items) {
	if (items.empty()) { Reset(); return NoItem; }
	size_t index = 0;
	if (active_) {
		const auto reviewed = std::find_if(items.begin(), items.end(), [&](const Item& item) {
			return std::abs(item.point.x - last_.x) < SameCell && std::abs(item.point.y - last_.y) < SameCell;
		});
		if (reviewed != items.end())
			index = (static_cast<size_t>(reviewed - items.begin()) + 1) % items.size();
	}
	active_ = true;
	last_ = items[index].point;
	return index;
}

void Cycle::Reset() {
	active_ = false;
	last_ = {};
}

std::string Describe(const Item& item, const EntityRadar::Settings& scan, const Strings::Table& strings) {
	std::string label = item.label;
	if (item.unexplored) {
		char tagged[256]{};
		std::snprintf(tagged, sizeof tagged, strings.wayUnexplored, item.label.c_str());
		label = tagged;
	}
	return label + ", " + EntityRadar::DistancePhrase(item.distance, scan, strings);
}

std::string Nothing(Radar::Direction direction, const Strings::Table& strings, Sides sides) {
	char text[128]{};
	std::snprintf(text, sizeof text, strings.finderNothing, sides == Sides::Map
		? SpokenDirection::Of(Axis(direction, sides), strings) : SpokenDirection::OfSide(direction, strings));
	return text;
}
}
