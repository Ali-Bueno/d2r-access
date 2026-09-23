#include "exit_review.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace D2RAccess::ExitReview {
namespace {
constexpr int HoursOnAClock = 12;
constexpr double DegreesPerHour = 360.0 / HoursOnAClock;
constexpr double DegreesPerRadian = 180.0 / std::numbers::pi;
// The same way out re-derived lands on the same collision cell, so one cell of
// the grid is all the tolerance needed to recognise it again.
constexpr double SameCell = 1;
}

int ClockHour(Position player, Position target) {
	// Native projection 0x334DB0: screen X grows with (X-Y) and screen Y with
	// (X+Y), so ahead on screen is -(dx+dy) and right is (dx-dy).
	const double dx = target.x - player.x, dy = target.y - player.y;
	const double right = (dx - dy) * Radar::Diagonal, ahead = -(dx + dy) * Radar::Diagonal;
	if (right == 0 && ahead == 0) return HoursOnAClock;
	const long hour = std::lround(std::atan2(right, ahead) * DegreesPerRadian / DegreesPerHour) % HoursOnAClock;
	return static_cast<int>(hour <= 0 ? hour + HoursOnAClock : hour);
}

std::vector<Item> Collect(Position player, const std::vector<Rooms::Opening>& openings,
	const std::vector<EntityRadar::Entity>& exits, const Strings::Table& strings, UnexploredExit unexploredExit) {
	std::vector<Item> items;
	items.reserve(openings.size() + exits.size());
	const auto add = [&](std::string label, Position point, const EntityRadar::Entity* entity, bool unexplored) {
		items.push_back({std::move(label), point, std::hypot(point.x - player.x, point.y - player.y),
			ClockHour(player, point), entity != nullptr, entity ? *entity : EntityRadar::Entity{}, unexplored});
	};
	for (const auto& opening : openings) add(strings.roomOpening, opening.centre, nullptr, opening.unexplored);
	for (const auto& exit : exits) add(exit.name, exit.position, &exit, unexploredExit && unexploredExit(exit));
	std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
		return a.unexplored != b.unexplored ? a.unexplored : a.distance < b.distance;
	});
	return items;
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

std::string Describe(const Item& item, size_t index, size_t count, const char* reach,
	const EntityRadar::Settings& scan, const Strings::Table& strings) {
	std::string verdict;
	if (reach != nullptr && *reach != '\0') { verdict = strings.listSeparator; verdict += reach; }
	std::string label = item.label;
	if (item.unexplored) {
		char tagged[256]{};
		std::snprintf(tagged, sizeof tagged, strings.wayUnexplored, item.label.c_str());
		label = tagged;
	}
	char text[320]{};
	std::snprintf(text, sizeof text, strings.exitReview, label.c_str(),
		EntityRadar::DistancePhrase(item.distance, scan, strings).c_str(), strings.clockHours[item.hour - 1],
		verdict.c_str(), static_cast<int>(index + 1), static_cast<int>(count));
	return text;
}
}
