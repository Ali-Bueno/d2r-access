#include "inventory_arrivals.h"

#include <algorithm>

namespace D2RAccess::InventoryArrivals {
namespace {
const Item* Find(const std::vector<Item>& items, uint32_t id) {
	const auto found = std::find_if(items.begin(), items.end(), [&](const Item& known) { return known.id == id; });
	return found == items.end() ? nullptr : &*found;
}
}

bool InStash(Place place) { return place == Place::PersonalStash || place == Place::SharedStash; }

std::vector<Arrival> Diff(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldBefore) {
	std::vector<Arrival> result;
	for (const auto& item : now) {
		if ((item.place != Place::Backpack && item.place != Place::Belt) || item.id == heldBefore) continue;
		const auto* previous = Find(before, item.id);
		if (!previous || InStash(previous->place)) result.push_back({item, false});
		else if (previous->place == Place::Worn) result.push_back({item, true});
	}
	return result;
}

std::vector<Item> Departures(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldNow) {
	std::vector<Item> result;
	for (const auto& item : before) {
		if ((item.place != Place::Backpack && !InStash(item.place)) || item.id == heldNow) continue;
		if (!Find(now, item.id)) result.push_back(item);
	}
	return result;
}

std::vector<Item> Stashed(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldBefore) {
	std::vector<Item> result;
	for (const auto& item : now) {
		if (!InStash(item.place) || item.id == heldBefore) continue;
		if (const auto* previous = Find(before, item.id); previous && previous->place == Place::Backpack) result.push_back(item);
	}
	return result;
}

std::vector<Move> Moves(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldBefore) {
	const auto carried = [](Place place) { return place == Place::Backpack || place == Place::Cube || InStash(place); };
	std::vector<Move> result;
	for (const auto& item : now) {
		if (!carried(item.place) || item.id == heldBefore) continue;
		if (const auto* previous = Find(before, item.id); previous && carried(previous->place) && previous->place != item.place)
			result.push_back({item, previous->place});
	}
	return result;
}

std::vector<Item> NewInCube(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldBefore) {
	std::vector<Item> result;
	for (const auto& item : now)
		if (item.place == Place::Cube && item.id != heldBefore && !Find(before, item.id)) result.push_back(item);
	return result;
}

int CubeConsumed(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldNow) {
	int gone = 0;
	for (const auto& item : before)
		if (item.place == Place::Cube && item.id != heldNow && !Find(now, item.id)) ++gone;
	return gone;
}

std::vector<Group> Grouped(const std::vector<Item>& items) {
	std::vector<Group> result;
	for (const auto& item : items) {
		const auto same = std::find_if(result.begin(), result.end(),
			[&](const Group& group) { return group.item.classId == item.classId && group.item.quality == item.quality; });
		if (same == result.end()) result.push_back({item, 1});
		else ++same->count;
	}
	return result;
}

}
