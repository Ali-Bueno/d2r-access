#include "settings_list.h"

#include <algorithm>
#include <utility>

namespace D2RAccess::SettingsList {
int Entry::Value() const { return levels > 0 ? level() : (on() ? 1 : 0); }

void Entry::Put(int value) const {
	if (levels > 0) setLevel(std::clamp(value, 0, levels - 1));
	else set(value != 0);
}

std::string Entry::Say(int value) const { return levels > 0 ? sayLevel(std::clamp(value, 0, levels - 1)) : say(value != 0); }

Menu::Menu(std::vector<Entry> entries, std::string opened, std::string closed)
	: opened_(std::move(opened)), closed_(std::move(closed)) {
	if (!entries.empty()) groups_.push_back({{}, std::move(entries)});
	at_.assign(groups_.size(), 0);
}

Menu::Menu(std::vector<Group> groups, std::string opened, std::string closed)
	: opened_(std::move(opened)), closed_(std::move(closed)) {
	for (auto& group : groups) if (!group.entries.empty()) groups_.push_back(std::move(group));
	at_.assign(groups_.size(), 0);
	inside_ = !Grouped();
}

std::string Menu::Current() const {
	if (groups_.empty()) return {};
	if (!inside_) return groups_[group_].name;
	const auto& entry = Entries()[at_[group_]];
	return entry.Say(entry.Value());
}

void Menu::Preview() const {
	if (!groups_.empty() && inside_ && Entries()[at_[group_]].preview) Entries()[at_[group_]].preview();
}

std::string Menu::Toggle(uint64_t now) {
	if (groups_.empty()) return {};
	open_ = !open_;
	usedMs_ = now;
	if (!open_) return closed_;
	Preview();
	const auto current = Current();
	return current.empty() ? opened_ : opened_ + ". " + current;
}

std::string Menu::Step(int delta, uint64_t now) {
	if (!open_ || groups_.empty() || delta == 0) return {};
	usedMs_ = now;
	auto& at = inside_ ? at_[group_] : group_;
	const size_t count = inside_ ? Entries().size() : groups_.size();
	at = delta > 0 ? (at + 1) % count : (at + count - 1) % count;
	Preview();
	return Current();
}

std::string Menu::Change(uint64_t now, int delta) {
	if (!open_ || groups_.empty() || !inside_) return {};
	usedMs_ = now;
	const auto& entry = Entries()[at_[group_]];
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

std::string Menu::Enter(uint64_t now) {
	if (!open_ || groups_.empty()) return {};
	if (inside_) return Change(now);
	usedMs_ = now;
	inside_ = true;
	Preview();
	return Current();
}

std::string Menu::Back(uint64_t now) {
	if (!open_ || !Grouped() || !inside_) return {};
	usedMs_ = now;
	inside_ = false;
	return Current();
}

std::string Menu::Expire(uint64_t now, uint64_t idleMs) {
	if (!open_ || now < usedMs_ || now - usedMs_ < idleMs) return {};
	open_ = false;
	return closed_;
}
}
