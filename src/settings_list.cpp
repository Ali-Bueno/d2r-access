#include "settings_list.h"

#include <algorithm>
#include <utility>

namespace D2RAccess::SettingsList {
int Entry::Value() const { return levels > 0 ? level() : (on() ? 1 : 0); }

void Entry::Put(int value) const {
	if (levels > 0) setLevel(std::clamp(value, 0, levels - 1));
	else set(value != 0);
}

std::string Entry::Say(int value) const { return levels > 0 ? sayLevel(value) : say(value != 0); }

Menu::Menu(std::vector<Entry> entries, std::string opened, std::string closed)
	: entries_(std::move(entries)), opened_(std::move(opened)), closed_(std::move(closed)) {}

std::string Menu::Current() const {
	if (entries_.empty()) return {};
	const auto& entry = entries_[at_];
	return entry.Say(entry.Value());
}

void Menu::Preview() const {
	if (!entries_.empty() && entries_[at_].preview) entries_[at_].preview();
}

std::string Menu::Toggle(uint64_t now) {
	if (entries_.empty()) return {};
	open_ = !open_;
	usedMs_ = now;
	if (!open_) return closed_;
	Preview();
	const auto current = Current();
	return current.empty() ? opened_ : opened_ + ". " + current;
}

std::string Menu::Step(int delta, uint64_t now) {
	if (!open_ || entries_.empty() || delta == 0) return {};
	usedMs_ = now;
	at_ = delta > 0 ? (at_ + 1) % entries_.size() : (at_ + entries_.size() - 1) % entries_.size();
	Preview();
	return Current();
}

std::string Menu::Change(uint64_t now, int delta) {
	if (!open_ || entries_.empty()) return {};
	usedMs_ = now;
	const auto& entry = entries_[at_];
	if (entry.preview) {
		entry.preview();
		return {};
	}
	int next = entry.Value() == 0 ? 1 : 0;
	if (entry.levels > 0) {
		const int value = entry.Value();
		next = delta == 0 ? (value + 1) % entry.levels : std::clamp(value + (delta > 0 ? 1 : -1), 0, entry.levels - 1);
	}
	entry.Put(next);
	return entry.announces ? std::string{} : entry.Say(next);
}

std::string Menu::Expire(uint64_t now, uint64_t idleMs) {
	if (!open_ || now < usedMs_ || now - usedMs_ < idleMs) return {};
	open_ = false;
	return closed_;
}
}
