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
	previous_ = percent;
	if (step == step_) return -1;
	step_ = step;
	return baseline ? -1 : percent;
}

void Meter::Reset() { step_ = previous_ = -1; }

Alert Alerts::Update(int life, int mana) {
	Alert alert;
	if (life >= 0) {
		const int previous = life_.Previous();
		alert.life = life_.Update(life);
		alert.interrupt = alert.life >= 0 && life <= CriticalPercent && previous > life;
	}
	if (mana >= 0) {
		const int previous = mana_.Previous();
		alert.mana = mana_.Update(mana);
		alert.interrupt = alert.interrupt
			|| (alert.mana >= 0 && mana <= CriticalPercent && previous > mana);
	}
	return alert;
}

void Alerts::Reset() { life_.Reset(); mana_.Reset(); }

unsigned OrbAvailability::Update(bool lifeReadable, bool manaReadable) {
	const bool readable[] = {lifeReadable, manaReadable};
	unsigned missing = 0;
	for (int index = 0; index < 2; ++index) {
		missing_[index] = readable[index] ? 0
			: std::min(missing_[index] + 1, MissingConfirmationSamples);
		if (missing_[index] == MissingConfirmationSamples) missing |= 1U << index;
	}
	return missing;
}

void OrbAvailability::Reset() { missing_[0] = missing_[1] = 0; }

std::optional<bool> Condition::Update(std::optional<bool> active) {
	if (!active) return std::nullopt;
	const auto previous = previous_;
	previous_ = active;
	if (previous && *previous != *active) return active;
	return std::nullopt;
}

void Condition::Reset() { previous_.reset(); }

MercenaryMeter::Alert MercenaryMeter::Update(std::optional<uint32_t> id, int percent, int level) {
	Alert alert;
	if (!id) return alert;
	if (id_ != id) { id_ = id; life_.Reset(); level_ = -1; }
	if (percent >= 0) {
		const int previous = life_.Previous();
		alert.life = life_.Update(percent);
		alert.interrupt = alert.life >= 0 && percent <= CriticalPercent && previous > percent;
	}
	if (level > 0) {
		if (level_ > 0 && level > level_) alert.level = level;
		if (level > level_) level_ = level;
	}
	return alert;
}

void MercenaryMeter::Reset() { id_.reset(); life_.Reset(); level_ = -1; }

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
