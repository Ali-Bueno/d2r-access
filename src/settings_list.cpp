#include "settings_list.h"

#include <utility>

namespace D2RAccess::SettingsList {
Menu::Menu(std::vector<Entry> entries, std::string opened, std::string closed)
	: entries_(std::move(entries)), opened_(std::move(opened)), closed_(std::move(closed)) {}

std::string Menu::Current() const {
	if (entries_.empty()) return {};
	const auto& entry = entries_[at_];
	return entry.say(entry.on());
}

std::string Menu::Toggle(uint64_t now) {
	if (entries_.empty()) return {};
	open_ = !open_;
	usedMs_ = now;
	if (!open_) return closed_;
	const auto current = Current();
	return current.empty() ? opened_ : opened_ + ". " + current;
}

std::string Menu::Step(int delta, uint64_t now) {
	if (!open_ || entries_.empty() || delta == 0) return {};
	usedMs_ = now;
	at_ = delta > 0 ? (at_ + 1) % entries_.size() : (at_ + entries_.size() - 1) % entries_.size();
	return Current();
}

std::string Menu::Change(uint64_t now) {
	if (!open_ || entries_.empty()) return {};
	usedMs_ = now;
	const auto& entry = entries_[at_];
	const bool next = !entry.on();
	entry.set(next);
	return entry.announces ? std::string{} : entry.say(next);
}

std::string Menu::Expire(uint64_t now, uint64_t idleMs) {
	if (!open_ || now < usedMs_ || now - usedMs_ < idleMs) return {};
	open_ = false;
	return closed_;
}
}
