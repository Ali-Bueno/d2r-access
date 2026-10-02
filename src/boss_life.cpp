#include "boss_life.h"

#include <algorithm>

namespace D2RAccess::BossLife {
std::vector<Alert> Tracker::Update(const std::vector<Reading>& readings) {
	std::erase_if(followed_, [&](const Boss& boss) {
		return std::none_of(readings.begin(), readings.end(), [&](const Reading& reading) { return reading.id == boss.id; });
	});
	std::vector<Alert> alerts;
	for (const auto& reading : readings) {
		if (reading.percent < 0) continue;
		auto boss = std::find_if(followed_.begin(), followed_.end(), [&](const Boss& entry) { return entry.id == reading.id; });
		if (boss == followed_.end()) boss = followed_.insert(followed_.end(), {reading.id, reading.name, {}});
		if (!reading.name.empty()) boss->name = reading.name;
		const int previous = boss->meter.Previous();
		const int spoken = boss->meter.Update(reading.percent);
		if (spoken < 0) continue;
		alerts.push_back({boss->name, spoken, spoken <= Vitals::CriticalPercent && previous > spoken});
	}
	return alerts;
}

std::vector<Reading> Tracker::Followed() const {
	std::vector<Reading> result;
	for (const auto& boss : followed_) result.push_back({boss.id, boss.name, boss.meter.Previous()});
	return result;
}
}
