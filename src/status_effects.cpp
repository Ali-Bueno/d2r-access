#include "status_effects.h"

#include <algorithm>

namespace D2RAccess::StatusEffects {

Change Tracker::Update(const std::vector<size_t>& active, uint64_t now) {
	Change change;
	for (size_t i = 0; i < Effects.size(); ++i) {
		const bool on = std::find(active.begin(), active.end(), i) != active.end();
		if (on && !on_[i]) {
			if (endAt_[i]) endAt_[i] = 0; // back before its end was said
			else if (baseline_) change.started.push_back(i);
		} else if (!on && on_[i]) {
			endAt_[i] = baseline_ ? now + EndHoldMs : 0;
		}
		on_[i] = on;
		if (!on && endAt_[i] && now >= endAt_[i]) {
			endAt_[i] = 0;
			change.ended.push_back(i);
		}
	}
	baseline_ = true;
	return change;
}

std::vector<size_t> Tracker::Active() const {
	std::vector<size_t> active;
	for (size_t i = 0; i < Effects.size(); ++i) if (on_[i]) active.push_back(i);
	return active;
}

void Tracker::Reset() {
	on_ = {};
	endAt_ = {};
	baseline_ = false;
}
}
