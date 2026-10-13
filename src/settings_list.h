#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Every switch of the mod in one spoken list, for a player who cannot type a
// console command: open it, step to a switch, change it. Stepping says the
// switch as it stands ("Radar enabled"), which names it and gives its state in
// the words the switch itself uses when changed. Nothing says where in the
// list the player is or how long the list is.
//
// The switches can come in named groups (a tester, 2026-10-13): the list then
// steps through the groups, Enter opens one and Back returns to the groups.
//
// Pure logic: the switches are handed in, so it is unit-tested without the game.
namespace D2RAccess::SettingsList {
struct Entry {
	std::function<bool()> on;             // the switch as it stands
	std::function<void(bool)> set;
	std::function<std::string(bool)> say; // the phrase for a state
	bool announces = false;               // the switch says its own change when set
	const char* key = nullptr;            // the name it is saved under between sessions (settings_store.h)
	// A sound of the glossary, not a switch: played when the list lands on it and
	// again on a change, which then says nothing over it.
	std::function<void()> preview;
	// A setting of `levels` values (0 to levels - 1) instead of a switch; `on`,
	// `set` and `say` are then unused.
	int levels = 0;
	std::function<int()> level;
	std::function<void(int)> setLevel;
	std::function<std::string(int)> sayLevel;

	// The value as saved: a switch is 0 or 1, a setting its level.
	int Value() const;
	void Put(int value) const;
	std::string Say(int value) const;
};

struct Group { std::string name; std::vector<Entry> entries; };

class Menu {
public:
	Menu() = default;
	// One list with no groups.
	Menu(std::vector<Entry> entries, std::string opened, std::string closed);
	// Groups with no entry are left out.
	Menu(std::vector<Group> groups, std::string opened, std::string closed);
	bool Open() const { return open_; }
	// Each returns what to say, empty when there is nothing to say. Opening
	// returns to the switch (or group) the list was left on.
	std::string Toggle(uint64_t now);
	std::string Step(int delta, uint64_t now); // wraps both ways, among groups or within one
	// A switch flips whatever the direction. A setting of several values moves one
	// value down (delta < 0) or up (delta > 0) and stops at either end; delta 0
	// moves up and wraps to the first, for a single change key. Nothing among groups.
	std::string Change(uint64_t now, int delta = 0);
	// Among groups opens the one the list stands on; within one is Change(now).
	std::string Enter(uint64_t now);
	// Within a group returns to the groups and names it; nothing without groups.
	std::string Back(uint64_t now);
	// Closes a list left open and unused for `idleMs`; returns what to say.
	std::string Expire(uint64_t now, uint64_t idleMs);
private:
	bool Grouped() const { return !groups_.empty() && !groups_.front().name.empty(); }
	const std::vector<Entry>& Entries() const { return groups_[group_].entries; }
	std::string Current() const;
	void Preview() const;
	std::vector<Group> groups_;
	std::string opened_, closed_;
	size_t group_ = 0;
	std::vector<size_t> at_; // the entry each group was left on
	bool inside_ = true;     // within a group (always, without groups)
	bool open_ = false;
	uint64_t usedMs_ = 0;
};
}
