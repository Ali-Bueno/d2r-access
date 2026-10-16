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

int ExperiencePercent(uint64_t experience, uint64_t levelStart, uint64_t nextLevel) {
	if (nextLevel <= levelStart) return -1;
	if (experience <= levelStart) return 0;
	return static_cast<int>(std::min<uint64_t>((experience - levelStart) * 100 / (nextLevel - levelStart), 99));
}

uint64_t MercenaryLevelStart(int level, uint64_t nextLevel) {
	if (level < 1) return 0;
	const uint64_t l = static_cast<uint64_t>(level), nextFactor = (l + 1) * (l + 1) * (l + 2);
	if (nextLevel == 0 || nextLevel % nextFactor != 0) return 0;
	return nextLevel / nextFactor * l * l * (l + 1);
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

Alert Alerts::Update(int life, int mana, ManaAlerts manaAlerts) {
	Alert alert;
	if (life >= 0) {
		const int previous = life_.Previous();
		alert.life = life_.Update(life);
		alert.interrupt = alert.life >= 0 && life <= CriticalPercent && previous > life;
	}
	if (mana >= 0) {
		const int previous = mana_.Previous();
		alert.mana = mana_.Update(mana);
		const bool lowFall = mana <= ManaLowPercent && previous > mana;
		if (manaAlerts == ManaAlerts::Off || (manaAlerts == ManaAlerts::Low && !lowFall)) alert.mana = -1;
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


std::optional<Gait> GaitTracker::Observe(std::optional<Gait> moving, bool free) {
	if (!moving || !free) {
		pending_.reset();
		held_ = 0;
		return std::nullopt;
	}
	if (!known_) {
		known_ = moving;
		return std::nullopt;
	}
	if (*moving == *known_) {
		pending_.reset();
		return std::nullopt;
	}
	held_ = pending_ == moving ? held_ + 1 : 1;
	pending_ = moving;
	if (held_ < GaitConfirmSamples) return std::nullopt;
	known_ = moving;
	pending_.reset();
	return moving;
}

std::optional<Gait> GaitTracker::Toggle() {
	if (!known_) return std::nullopt;
	known_ = *known_ == Gait::Run ? Gait::Walk : Gait::Run;
	pending_.reset();
	return known_;
}

void GaitTracker::Reset() {
	known_.reset();
	pending_.reset();
	held_ = 0;
}

namespace {
// How many steps a value sits at or below, one more once it is empty.
template<size_t N> int Depth(const int (&steps)[N], int value) {
	int depth = value <= 0 ? 1 : 0;
	for (const int step : steps) depth += value <= step ? 1 : 0;
	return depth;
}
}

int StaminaMeter::Update(int percent) {
	if (percent < 0) return -1;
	const int depth = Depth(StaminaSteps, percent);
	if (depth_ < 0 || depth < depth_) {
		const int rearmed = Depth(StaminaSteps, percent - StaminaRearm);
		if (depth_ < 0 || rearmed < depth_) depth_ = depth_ < 0 ? depth : rearmed;
		return -1;
	}
	if (depth == depth_) return -1;
	depth_ = depth;
	return percent;
}

void StaminaMeter::Reset() { depth_ = -1; }

std::optional<AmmoMeter::Stack> AmmoMeter::Update(const std::vector<Stack>& inHand) {
	std::optional<Stack> fell;
	std::vector<Tracked> tracked;
	for (const auto& stack : inHand) {
		const auto known = std::find_if(tracked_.begin(), tracked_.end(), [&](const Tracked& t) { return t.id == stack.id; });
		const int depth = Depth(AmmoSteps, stack.quantity);
		if (known == tracked_.end()) {
			if (stack.quantity > 1) tracked.push_back({stack.id, depth});
			continue;
		}
		if (depth > known->depth && !fell) fell = stack;
		tracked.push_back({stack.id, depth});
	}
	tracked_ = std::move(tracked);
	return fell;
}

std::vector<AmmoMeter::Stack> AmmoMeter::Known(const std::vector<Stack>& inHand) const {
	std::vector<Stack> stacks;
	for (const auto& stack : inHand)
		if (stack.quantity > 1 || std::any_of(tracked_.begin(), tracked_.end(), [&](const Tracked& t) { return t.id == stack.id; }))
			stacks.push_back(stack);
	return stacks;
}

void AmmoMeter::Reset() { tracked_.clear(); }

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

std::vector<Breakage::Item> Equipping::Sweep(const std::vector<Breakage::Item>& worn) {
	std::vector<Breakage::Item> equipped;
	if (known_) {
		for (const auto& item : worn)
			if (std::find(worn_.begin(), worn_.end(), item.id) == worn_.end()) equipped.push_back(item);
	}
	worn_.clear();
	for (const auto& item : worn) worn_.push_back(item.id);
	known_ = true;
	return equipped;
}

void Equipping::Reset() {
	worn_.clear();
	known_ = false;
}

bool LowLife::Update(int lifePercent, int threshold, bool* started) {
	const bool was = on_;
	if (lifePercent <= 0) on_ = false;
	else if (lifePercent <= threshold) on_ = true;
	else if (lifePercent > threshold + ReleasePoints) on_ = false;
	if (started) *started = on_ && !was;
	return on_;
}

}
