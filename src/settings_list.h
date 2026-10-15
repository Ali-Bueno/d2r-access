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
// The switches can come in named groups (a tester, 2026-10-13), and a group in
// groups of its own: the list steps through a group's switches and then its
// groups, Enter opens one and Back returns to the group above.
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

// A group's own groups come after its switches.
struct Group {
	std::string name;
	std::vector<Entry> entries;
	std::vector<Group> groups;
};

// What a step past either end of a list does (a tester, 2026-10-14): go round
// to the other end, or stay on the last entry reached.
enum class Ends { Wrap, Stop };
// What the last step met, for its sound: nothing, an end it went round, an end it stopped at.
enum class Edge { None, Wrapped, Stopped };

class Menu {
public:
	Menu() = default;
	// One list with no groups.
	Menu(std::vector<Entry> entries, std::string opened, std::string closed);
	// Groups with no entry are left out.
	Menu(std::vector<Group> groups, std::string opened, std::string closed);
	bool Open() const { return open_; }
	// How a group is said as the list lands on it ("%s sub menu"); a tester,
	// 2026-10-14: a group should say it is one. Its name alone without it.
	void SetGroupFormat(std::string format) { groupFormat_ = std::move(format); }
	// Opening starts over at the first entry of the top list instead of returning
	// to where the list was left (a tester, 2026-10-14; off by default).
	void SetRestart(bool restart) { restart_ = restart; }
	// Each returns what to say, empty when there is nothing to say. Opening
	// returns to the switch (or group) the list was left on.
	std::string Toggle(uint64_t now);
	// Within the list the player is in; at an end it wraps or stays, and then
	// says the entry it stayed on again.
	std::string Step(int delta, uint64_t now, Ends ends = Ends::Wrap);
	Edge LastEdge() const { return edge_; }
	// A switch flips whatever the direction. A setting of several values moves one
	// value down (delta < 0) or up (delta > 0) and stops at either end; delta 0
	// moves up and wraps to the first, for a single change key. Nothing on a group.
	std::string Change(uint64_t now, int delta = 0);
	// On a group opens it; on a switch is Change(now).
	std::string Enter(uint64_t now);
	// Within a group returns to the group above and names the group left; nothing at the top.
	std::string Back(uint64_t now);
	// Closes a list left open and unused for `idleMs`; returns what to say.
	std::string Expire(uint64_t now, uint64_t idleMs);
private:
	struct List {
		std::string name;
		std::vector<Entry> entries;
		std::vector<size_t> lists; // its groups, by index into lists_
		size_t parent = 0;
		size_t at = 0;             // the entry, then the group, it was left on
	};
	size_t Add(Group group, size_t parent);
	bool Empty() const { return lists_.empty() || Count(lists_.front()) == 0; }
	static size_t Count(const List& list) { return list.entries.size() + list.lists.size(); }
	const Entry* Landed() const; // the entry stood on, none on a group
	std::string Current() const;
	void Preview() const;
	std::vector<List> lists_; // the top first
	std::string opened_, closed_, groupFormat_;
	bool restart_ = false;
	size_t list_ = 0;         // the list the player is in
	Edge edge_ = Edge::None;
	bool open_ = false;
	uint64_t usedMs_ = 0;
};
}
