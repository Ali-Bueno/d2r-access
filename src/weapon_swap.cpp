#include "weapon_swap.h"
#include "speech_strings.h"
#include <cstdio>

namespace D2RAccess::WeaponSwap {

std::optional<int> Tracker::Update(uint64_t now, std::optional<bool> secondSet, const Hands& hands) {
	if (!secondSet) return std::nullopt;
	std::optional<int> announce;
	if (!second_) second_ = secondSet;
	else if (*secondSet != *second_) {
		second_ = secondSet;
		const int set = *secondSet ? 2 : 1;
		// Swapped on this client the items move with the flag; from the server they may follow it.
		if (hands != hands_) { announce = set; pending_.reset(); }
		else pending_ = Pending{set, hands, now + ItemMoveGraceMs};
	} else if (pending_ && (hands != pending_->from || now >= pending_->due)) {
		announce = pending_->set;
		pending_.reset();
	}
	hands_ = hands;
	return announce;
}

void Tracker::Reset() {
	second_.reset();
	hands_ = {};
	pending_.reset();
}

std::string Phrase(int set, const std::vector<std::string>& weapons) {
	const auto& strings = Strings::Current();
	std::string list;
	for (const auto& weapon : weapons) {
		if (weapon.empty()) continue;
		if (!list.empty()) list += strings.listSeparator;
		list += weapon;
	}
	char phrase[300]{};
	std::snprintf(phrase, sizeof(phrase), strings.weaponSet, set, list.empty() ? strings.handsEmpty : list.c_str());
	return phrase;
}
}
