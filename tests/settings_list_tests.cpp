#include "settings_list.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;

namespace {
struct Switch { const char* name; bool on; bool announces; };
std::vector<Switch> g_switches{{"Radar", true, true}, {"Trade", true, false}, {"Doors", false, false}};

SettingsList::Menu List() {
	std::vector<SettingsList::Entry> entries;
	for (size_t i = 0; i < g_switches.size(); ++i)
		entries.push_back({[i] { return g_switches[i].on; }, [i](bool on) { g_switches[i].on = on; },
			[i](bool on) { return std::string(g_switches[i].name) + (on ? " on" : " off"); }, g_switches[i].announces});
	return SettingsList::Menu(std::move(entries), "Settings", "Settings closed");
}
bool HasDigit(const std::string& text) {
	for (const char c : text) if (c >= '0' && c <= '9') return true;
	return false;
}
}

int main() {
	auto list = List();
	Check(!list.Open(), "the list starts closed");
	Check(list.Step(1, 0).empty() && list.Change(0).empty(), "a closed list neither steps nor changes anything");
	Check(g_switches[0].on, "a closed list changed nothing");

	Check(list.Toggle(1000) == "Settings. Radar on", "opening says the list and the first switch as it stands");
	Check(list.Open(), "the list is open");
	Check(list.Step(1, 1100) == "Trade on", "a step says the next switch as it stands");
	Check(list.Step(1, 1200) == "Doors off", "and the next");
	Check(list.Step(1, 1300) == "Radar on", "past the last it wraps to the first");
	Check(list.Step(-1, 1400) == "Doors off", "a step back from the first wraps to the last");

	Check(list.Change(1500) == "Doors on" && g_switches[2].on, "a change flips the switch and says its new state");
	Check(list.Change(1600) == "Doors off" && !g_switches[2].on, "and flips it back");
	(void)list.Step(1, 1700);
	Check(list.Change(1800).empty() && !g_switches[0].on, "a switch that says its own change is changed without a second word");
	Check(list.Step(1, 1900) == "Trade on" && list.Step(-1, 2000) == "Radar off", "the list says a switch as it stands now");

	Check(list.Toggle(2100) == "Settings closed" && !list.Open(), "closing says so");
	Check(list.Toggle(2200) == "Settings. Radar off", "opening again returns to the switch the list was left on");

	Check(list.Expire(2200 + 29999, 30000).empty() && list.Open(), "a list in use is not closed");
	Check(list.Expire(2200 + 30000, 30000) == "Settings closed" && !list.Open(), "a list left open and unused closes itself and says so");
	Check(list.Expire(999999, 30000).empty(), "a closed list has nothing to close");

	// Nothing the list says gives a position or a count.
	auto numbered = List();
	bool digits = HasDigit(numbered.Toggle(0));
	for (int i = 0; i < 6; ++i) digits |= HasDigit(numbered.Step(1, 0));
	Check(!digits, "the list never says a position or a count");

	SettingsList::Menu empty;
	Check(empty.Toggle(0).empty() && !empty.Open(), "a list with no switches does not open");

	// The sound glossary rides on the list: a sound plays where the list lands on it.
	int played = 0;
	SettingsList::Entry sound;
	sound.on = [] { return false; };
	sound.set = [](bool) {};
	sound.say = [](bool) { return std::string("Sound: wall"); };
	sound.preview = [&played] { ++played; };
	SettingsList::Menu glossary({sound}, "Settings", "Settings closed");
	Check(glossary.Toggle(0) == "Settings. Sound: wall" && played == 1, "opening on a sound names it and plays it");
	Check(glossary.Step(1, 10) == "Sound: wall" && played == 2, "stepping onto a sound plays it");
	Check(glossary.Change(20).empty() && played == 3, "a change plays it again and says nothing over it");
	Passed();
	return 0;
}
