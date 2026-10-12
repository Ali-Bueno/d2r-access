#include "inventory_arrivals.h"

#include <algorithm>

namespace D2RAccess::InventoryArrivals {

std::vector<Arrival> Diff(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldBefore) {
	std::vector<Arrival> result;
	for (const auto& item : now) {
		if ((item.place != Place::Backpack && item.place != Place::Belt) || item.id == heldBefore) continue;
		const auto previous = std::find_if(before.begin(), before.end(), [&](const Item& known) { return known.id == item.id; });
		if (previous == before.end()) result.push_back({item, false});
		else if (previous->place == Place::Worn) result.push_back({item, true});
	}
	return result;
}

std::vector<Item> Departures(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldNow) {
	std::vector<Item> result;
	for (const auto& item : before) {
		if (item.place != Place::Backpack || item.id == heldNow) continue;
		if (std::none_of(now.begin(), now.end(), [&](const Item& known) { return known.id == item.id; })) result.push_back(item);
	}
	return result;
}

}
