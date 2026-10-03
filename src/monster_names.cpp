#include "monster_names.h"

#include <algorithm>

namespace D2RAccess::MonsterNames {
namespace {
constexpr std::string_view Bullet = "\xE2\x80\xA2"; // U+2022
}

bool Gate::Admit(std::string_view label, std::optional<double> distance, bool important,
	std::optional<double> reach, uint64_t now, const Settings& settings) {
	std::erase_if(shown_, [&](const Shown& shown) { return now < shown.at || now - shown.at > settings.repeatMs; });
	if (!important && distance && reach && *distance > *reach) { current_.clear(); return false; }
	auto shown = std::find_if(shown_.begin(), shown_.end(), [&](const Shown& entry) { return entry.label == label; });
	if (shown == shown_.end()) {
		shown_.push_back({std::string(label), now});
		current_ = label;
		return true;
	}
	// Still the display that was admitted, or the same name back too soon;
	// either way it counts as shown now, so a pack trading the target keeps
	// every one of its names quiet for as long as the trading goes on.
	shown->at = now;
	if (current_ == label) return true;
	current_.clear();
	return false;
}

std::string Speakable(std::string_view label) {
	std::string result;
	for (size_t at = 0; at < label.size();) {
		if (label.substr(at, Bullet.size()) != Bullet) { result += label[at++]; continue; }
		at += Bullet.size();
		while (!result.empty() && result.back() == ' ') result.pop_back();
		while (at < label.size() && label[at] == ' ') ++at;
		if (!result.empty() && at < label.size()) result += ", ";
	}
	return result;
}
}
