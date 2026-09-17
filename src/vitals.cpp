#include "vitals.h"

#include <algorithm>
#include <cctype>

namespace D2RAccess::Vitals {
namespace {
// One side of the orb's "current/maximum" text. Any digit grouping or label
// the locale adds is ignored; only the digits carry the value.
std::optional<int> Number(std::string_view text) {
	long long value = 0;
	bool any = false;
	for (const unsigned char character : text) {
		if (std::isdigit(character) == 0) continue;
		any = true;
		value = value * 10 + (character - '0');
		if (value > 1000000000LL) return std::nullopt; // no orb holds such a number
	}
	if (!any) return std::nullopt;
	return static_cast<int>(value);
}
}

int Step(int percent) {
	for (const int step : Steps)
		if (percent >= step) return step;
	return 0;
}

int Percent(int current, int maximum) {
	if (maximum <= 0) return -1;
	if (current <= 0) return 0;
	const long long percent = static_cast<long long>(current) * 100 / maximum;
	return static_cast<int>(std::clamp<long long>(percent, 0, 100));
}

std::optional<Orb> ParseOrb(std::string_view text) {
	const auto split = text.find('/');
	if (split == std::string_view::npos) return std::nullopt;
	const auto current = Number(text.substr(0, split));
	const auto maximum = Number(text.substr(split + 1));
	if (!current || !maximum) return std::nullopt;
	return Orb{*current, *maximum};
}

int Meter::Update(int percent) {
	const int step = Step(percent);
	const bool baseline = step_ < 0;
	if (step == step_) return -1;
	step_ = step;
	return baseline ? -1 : percent;
}

void Meter::Reset() { step_ = -1; }

std::vector<int> Breakage::Sweep(const std::vector<Item>& worn) {
	std::vector<int> broke;
	std::vector<Known> current;
	current.reserve(worn.size());
	for (const auto& item : worn) {
		if (item.maximumDurability <= 0) continue; // indestructible: it never breaks
		const bool broken = item.durability <= 0;
		const auto previous = std::find_if(known_.begin(), known_.end(),
			[&item](const Known& known) { return known.id == item.id; });
		// An item worn already broken was not broken by the player just now.
		if (broken && previous != known_.end() && !previous->broken) broke.push_back(item.bodyLocation);
		current.push_back({item.id, broken});
	}
	known_ = std::move(current);
	return broke;
}

void Breakage::Reset() { known_.clear(); }

}
