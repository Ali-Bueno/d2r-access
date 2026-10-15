#include "settings_list.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace D2RAccess::SettingsList {
int Entry::Value() const { return levels > 0 ? level() : (on() ? 1 : 0); }

void Entry::Put(int value) const {
	if (levels > 0) setLevel(std::clamp(value, 0, levels - 1));
	else set(value != 0);
}

std::string Entry::Say(int value) const { return levels > 0 ? sayLevel(std::clamp(value, 0, levels - 1)) : say(value != 0); }

namespace {
bool Holds(const Group& group) {
	return !group.entries.empty() || std::any_of(group.groups.begin(), group.groups.end(), Holds);
}
}

Menu::Menu(std::vector<Entry> entries, std::string opened, std::string closed)
	: opened_(std::move(opened)), closed_(std::move(closed)) {
	(void)Add({{}, std::move(entries), {}}, 0);
}

Menu::Menu(std::vector<Group> groups, std::string opened, std::string closed)
	: opened_(std::move(opened)), closed_(std::move(closed)) {
	(void)Add({{}, {}, std::move(groups)}, 0);
}

size_t Menu::Add(Group group, size_t parent) {
	const size_t index = lists_.size();
	lists_.push_back({std::move(group.name), std::move(group.entries), {}, parent, 0});
	for (auto& child : group.groups) {
		if (!Holds(child)) continue;
		const size_t added = Add(std::move(child), index);
		lists_[index].lists.push_back(added);
	}
	return index;
}

const Entry* Menu::Landed() const {
	const auto& list = lists_[list_];
	return list.at < list.entries.size() ? &list.entries[list.at] : nullptr;
}

std::string Menu::Current() const {
	if (Empty()) return {};
	if (const auto* entry = Landed()) return entry->Say(entry->Value());
	const auto& list = lists_[list_];
	const auto& name = lists_[list.lists[list.at - list.entries.size()]].name;
	if (groupFormat_.empty()) return name;
	char phrase[200]{};
	std::snprintf(phrase, sizeof(phrase), groupFormat_.c_str(), name.c_str());
	return phrase;
}

void Menu::Preview() const {
	if (Empty()) return;
	if (const auto* entry = Landed(); entry && entry->preview) entry->preview();
}

std::string Menu::Toggle(uint64_t now) {
	if (Empty()) return {};
	open_ = !open_;
	usedMs_ = now;
	if (!open_) return closed_;
	if (restart_) {
		list_ = 0;
		for (auto& list : lists_) list.at = 0;
	}
	Preview();
	const auto current = Current();
	return current.empty() ? opened_ : opened_ + ". " + current;
}

std::string Menu::Step(int delta, uint64_t now, Ends ends) {
	edge_ = Edge::None;
	if (!open_ || Empty() || delta == 0) return {};
	usedMs_ = now;
	auto& list = lists_[list_];
	const size_t count = Count(list);
	const bool past = delta > 0 ? list.at + 1 >= count : list.at == 0;
	if (past) edge_ = ends == Ends::Wrap ? Edge::Wrapped : Edge::Stopped;
	if (!past || ends == Ends::Wrap) list.at = delta > 0 ? (list.at + 1) % count : (list.at + count - 1) % count;
	Preview();
	return Current();
}

std::string Menu::Change(uint64_t now, int delta) {
	if (!open_ || Empty()) return {};
	const auto* entry = Landed();
	if (!entry) return {};
	usedMs_ = now;
	if (entry->preview) {
		entry->preview();
		return {};
	}
	int next = entry->Value() == 0 ? 1 : 0;
	if (entry->levels > 0) {
		const int value = entry->Value();
		next = delta == 0 ? (value + 1) % entry->levels : std::clamp(value + (delta > 0 ? 1 : -1), 0, entry->levels - 1);
	}
	entry->Put(next);
	return entry->announces ? std::string{} : entry->Say(next);
}

std::string Menu::Enter(uint64_t now) {
	if (!open_ || Empty()) return {};
	if (Landed()) return Change(now);
	usedMs_ = now;
	const auto& list = lists_[list_];
	list_ = list.lists[list.at - list.entries.size()];
	Preview();
	return Current();
}

std::string Menu::Back(uint64_t now) {
	if (!open_ || Empty() || list_ == 0) return {};
	usedMs_ = now;
	list_ = lists_[list_].parent;
	return Current();
}

std::string Menu::Expire(uint64_t now, uint64_t idleMs) {
	if (!open_ || now < usedMs_ || now - usedMs_ < idleMs) return {};
	open_ = false;
	return closed_;
}
}
