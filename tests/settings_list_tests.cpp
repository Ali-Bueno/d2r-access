#include "settings_list.h"
#include "test_support.h"

#include <string>
#include <utility>
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

	// A setting of several values steps one value at a time.
	int level = 1;
	SettingsList::Entry strength;
	strength.levels = 3;
	strength.level = [&level] { return level; };
	strength.setLevel = [&level](int value) { level = value; };
	strength.sayLevel = [](int value) { return std::string("Pan ") + std::to_string(value); };
	SettingsList::Menu levels({strength}, "Settings", "Settings closed");
	(void)levels.Toggle(0);
	Check(levels.Change(10, 1) == "Pan 2" && level == 2, "right raises a setting by one");
	Check(levels.Change(20, 1) == "Pan 2" && level == 2, "and stops at the top");
	Check(levels.Change(30, -1) == "Pan 1" && levels.Change(40, -1) == "Pan 0" && levels.Change(50, -1) == "Pan 0",
		"left lowers it and stops at the bottom");
	Check(levels.Change(60) == "Pan 1" && levels.Change(70) == "Pan 2" && levels.Change(80) == "Pan 0",
		"the single change key raises it and wraps to the first");
	Check(strength.Value() == 0, "a setting is saved as its level");
	strength.Put(9);
	Check(level == 2, "a saved level beyond the setting is held to its last value");
	SettingsList::Entry flip{[] { return g_switches[1].on; }, [](bool on) { g_switches[1].on = on; },
		[](bool on) { return std::string(on ? "Trade on" : "Trade off"); }};
	SettingsList::Menu switches({flip}, "Settings", "Settings closed");
	(void)switches.Toggle(0);
	const bool before = g_switches[1].on;
	Check(!switches.Change(10, -1).empty() && g_switches[1].on != before, "left flips a switch as well");
	Check(switches.Back(20).empty() && switches.Enter(30) == (g_switches[1].on ? "Trade off" : "Trade on"),
		"without groups Back does nothing and Enter changes the switch");

	// Named groups: the list steps through the groups, Enter opens one, Back returns.
	g_switches = {{"Radar", true, true}, {"Trade", true, false}, {"Doors", false, false}};
	SettingsList::Entry radar{[] { return g_switches[0].on; }, [](bool on) { g_switches[0].on = on; },
		[](bool on) { return std::string(on ? "Radar on" : "Radar off"); }};
	SettingsList::Entry trade{[] { return g_switches[1].on; }, [](bool on) { g_switches[1].on = on; },
		[](bool on) { return std::string(on ? "Trade on" : "Trade off"); }};
	SettingsList::Entry doors{[] { return g_switches[2].on; }, [](bool on) { g_switches[2].on = on; },
		[](bool on) { return std::string(on ? "Doors on" : "Doors off"); }};
	SettingsList::Menu grouped({{"Navigation", {radar, doors}}, {"Empty", {}}, {"Alerts", {trade}}}, "Settings", "Settings closed");
	Check(grouped.Toggle(0) == "Settings. Navigation", "a grouped list opens on its first group");
	Check(grouped.Change(10).empty() && g_switches[0].on, "a change among the groups changes nothing");
	Check(grouped.Step(1, 20) == "Alerts" && grouped.Step(1, 30) == "Navigation", "steps go group to group, an empty group left out");
	Check(grouped.Back(40).empty(), "Back among the groups has nowhere to go");
	Check(grouped.Enter(50) == "Radar on", "Enter opens the group on its first switch");
	Check(grouped.Step(1, 60) == "Doors off" && grouped.Step(1, 70) == "Radar on", "steps wrap within the group");
	(void)grouped.Step(1, 80);
	Check(grouped.Enter(90) == "Doors on" && g_switches[2].on, "Enter within a group changes the switch");
	Check(grouped.Back(100) == "Navigation", "Back returns to the groups and names the group");
	Check(grouped.Step(1, 110) == "Alerts" && grouped.Enter(120) == "Trade on", "another group opens on its own switch");
	(void)grouped.Back(130);
	(void)grouped.Step(-1, 140);
	Check(grouped.Enter(150) == "Doors on", "a group reopens on the switch it was left on");
	Check(grouped.Toggle(160) == "Settings closed" && grouped.Toggle(170) == "Settings. Doors on",
		"the list reopens where it was left, inside a group too");

	// A group of its own inside a group comes after the group's switches.
	SettingsList::Menu nested({{"Glossary", {sound, sound}, {{"Drops", {sound}, {}}, {"Nothing", {}, {}}}}},
		"Settings", "Settings closed");
	played = 0;
	(void)nested.Toggle(0);
	Check(nested.Enter(10) == "Sound: wall" && played == 1, "the group opens on its first sound");
	(void)nested.Step(1, 20);
	Check(nested.Step(1, 30) == "Drops" && played == 2, "the group's own group follows its switches and plays nothing");
	Check(nested.Step(1, 40) == "Sound: wall", "an empty inner group is left out and the steps wrap");
	(void)nested.Step(-1, 50);
	Check(nested.Change(60).empty() && nested.Enter(70) == "Sound: wall", "a change on a group does nothing, Enter opens it");
	Check(nested.Back(80) == "Drops" && nested.Back(90) == "Glossary" && nested.Back(100).empty(),
		"Back climbs one group at a time to the top");

	// At either end a list goes round or stays, and says which.
	g_switches = {{"Radar", true, true}, {"Trade", true, false}, {"Doors", false, false}};
	auto ends = List();
	(void)ends.Toggle(0);
	Check(ends.Step(1, 10, SettingsList::Ends::Stop) == "Trade on" && ends.LastEdge() == SettingsList::Edge::None,
		"a step inside the list meets no end");
	(void)ends.Step(1, 20, SettingsList::Ends::Stop);
	Check(ends.Step(1, 30, SettingsList::Ends::Stop) == "Doors off" && ends.LastEdge() == SettingsList::Edge::Stopped,
		"stopping at the last entry says it again");
	Check(ends.Step(1, 40) == "Radar on" && ends.LastEdge() == SettingsList::Edge::Wrapped, "wrapping goes round and says so");
	Check(ends.Step(-1, 50, SettingsList::Ends::Stop) == "Radar on" && ends.LastEdge() == SettingsList::Edge::Stopped,
		"stopping at the first entry too");
	Check(ends.Step(-1, 60) == "Doors off" && ends.LastEdge() == SettingsList::Edge::Wrapped, "and wrapping back from it");
	(void)ends.Toggle(70);
	Check(ends.Step(1, 80).empty() && ends.LastEdge() == SettingsList::Edge::None, "a closed list's step meets nothing");
	Passed();
	return 0;
}
