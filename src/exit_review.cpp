#include "exit_review.h"

#include "npc_proximity.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace D2RAccess::ExitReview {
namespace {
// The same way out re-derived lands on the same collision cell, so one cell of
// the grid is all the tolerance needed to recognise it again.
constexpr double SameCell = 1;
}

std::vector<Item> Collect(Position player, const std::vector<Rooms::Opening>& openings,
	const std::vector<EntityRadar::Entity>& exits, const Strings::Table& strings) {
	std::vector<Item> items;
	items.reserve(openings.size() + exits.size());
	const auto add = [&](std::string label, Position point, const EntityRadar::Entity* entity) {
		items.push_back({std::move(label), point, std::hypot(point.x - player.x, point.y - player.y),
			NpcProximity::DirectionOf(player, point), entity != nullptr, entity ? *entity : EntityRadar::Entity{}});
	};
	for (const auto& opening : openings) add(strings.roomOpening, opening.centre, nullptr);
	for (const auto& exit : exits) add(exit.name, exit.position, &exit);
	std::stable_sort(items.begin(), items.end(),
		[](const Item& a, const Item& b) { return a.distance < b.distance; });
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

std::string Describe(const Item& item, const EntityRadar::Settings& scan, const Strings::Table& strings) {
	char text[320]{};
	std::snprintf(text, sizeof text, strings.surveySteps, item.label.c_str(),
		EntityRadar::DistancePhrase(item.distance, scan, strings).c_str(),
		strings.scanDirections[static_cast<size_t>(item.direction)]);
	return text;
}
}
