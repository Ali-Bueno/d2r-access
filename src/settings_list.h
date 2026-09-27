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
// Pure logic: the switches are handed in, so it is unit-tested without the game.
namespace D2RAccess::SettingsList {
struct Entry {
	std::function<bool()> on;             // the switch as it stands
	std::function<void(bool)> set;
	std::function<std::string(bool)> say; // the phrase for a state
	bool announces = false;               // the switch says its own change when set
};

class Menu {
public:
	Menu() = default;
	Menu(std::vector<Entry> entries, std::string opened, std::string closed);
	bool Open() const { return open_; }
	// Each returns what to say, empty when there is nothing to say. Opening
	// returns to the switch the list was left on.
	std::string Toggle(uint64_t now);
	std::string Step(int delta, uint64_t now); // wraps both ways
	std::string Change(uint64_t now);
	// Closes a list left open and unused for `idleMs`; returns what to say.
	std::string Expire(uint64_t now, uint64_t idleMs);
private:
	std::string Current() const;
	std::vector<Entry> entries_;
	std::string opened_, closed_;
	size_t at_ = 0;
	bool open_ = false;
	uint64_t usedMs_ = 0;
};
}
