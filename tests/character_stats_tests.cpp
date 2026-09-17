#include "character_stats.h"
#include "speech_strings.h"
#include "test_support.h"

#include <map>
#include <string>

namespace {
using namespace D2RAccess;
// A slice of CharacterStatsPanel as the layouts build it: every row is a
// <name>Title label whose value lives in a sibling widget, and whose focusable
// "Tooltip" child is where the controller stops.
struct Widget {
	std::string name, text;
	uintptr_t parent = 0;
};
std::map<uintptr_t, Widget> widgets;
bool panelVisible = true;

constexpr uintptr_t Panel = 1;
void Add(uintptr_t id, std::string name, std::string text = {}, uintptr_t parent = Panel) {
	widgets[id] = {std::move(name), std::move(text), parent};
}
bool Descends(uintptr_t widget, uintptr_t ancestor) {
	for (auto at = widget; at != 0;) {
		const auto found = widgets.find(at);
		if (found == widgets.end()) return false;
		if (found->second.parent == ancestor) return true;
		at = found->second.parent;
	}
	return false;
}
NativeUi::Focus On(uintptr_t widget) { return {Panel, widget}; }
}

namespace D2RAccess::NativeUi {
bool Visible(uintptr_t widget) { return widget == Panel ? panelVisible : widgets.count(widget) != 0; }
uintptr_t Parent(uintptr_t widget) {
	const auto found = widgets.find(widget);
	return found == widgets.end() ? 0 : found->second.parent;
}
}

namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t widget) {
	const auto found = widgets.find(widget);
	return found == widgets.end() ? std::string{} : found->second.name;
}
std::string ReadText(uintptr_t widget) {
	const auto found = widgets.find(widget);
	return found == widgets.end() ? std::string{} : found->second.text;
}
// The game's own child lookup searches the whole subtree by name.
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	for (const auto& [id, widget] : widgets)
		if (widget.name == name && Descends(id, parent)) return id;
	return 0;
}
}

namespace D2RAccess::Localization {
std::string Text(std::string_view key) {
	if (key == "minipanelchar") return "Personaje";
	if (key == "strchrlvl") return "Nivel";
	return {};
}
}

int main() {
	using namespace D2RAccess;
	Strings::Select(Strings::Language::esES);
	widgets[Panel] = {"CharacterStatsPanel", "", 0};
	Add(2, "CharacterName", "Ulfr");
	Add(3, "FullClassInfo", "Bárbaro");
	Add(4, "Level", "7");
	Add(10, "StrengthTitle", "Fuerza");
	Add(11, "Tooltip", "", 10);
	Add(12, "StrengthStat", "30");
	Add(20, "HPTitle", "Vida");
	Add(21, "Tooltip", "", 20);
	Add(22, "CurrentHP", "45");
	Add(23, "MaxHP", "120");
	Add(30, "FireResistanceTitle", "Resistencia al fuego");
	Add(31, "Tooltip", "", 30);
	Add(32, "FireText", "12");
	Add(40, "ArmorClassTitle", "Defensa");
	Add(41, "Tooltip", "", 40);
	Add(42, "ArmorClass", "88");

	Check(CharacterStats::HandlesFocus(On(11), {}), "the focusable child of a stat label is recognised as a row");
	Check(CharacterStats::HandlesFocus(On(10), {}), "the label itself is a row when the game focuses it directly");
	Check(!CharacterStats::HandlesFocus(On(12), {}), "a bare value widget is not a row of its own");
	Check(!CharacterStats::HandlesFocus(On(2), {}), "unrelated panel widgets are left to the other readers");

	const auto strength = CharacterStats::Read(On(11), {});
	Check(strength.option == "Fuerza. 30", "an attribute row reads its label and its live value");
	Check(strength.body == "Personaje. Ulfr. Bárbaro. Nivel 7",
		"the panel name and who the character is travel in the body, so they are spoken once on entering");
	Check(strength.focusKey == "11" && strength.context == "CharacterStatsPanel", "each row is its own focus identity");

	Check(CharacterStats::Read(On(21), {}).option == "Vida. 45 de 120", "life reads as current out of maximum");
	Check(CharacterStats::Read(On(41), {}).option == "Defensa. 88", "defence uses its own value widget name");
	Check(CharacterStats::Read(On(31), {}).option == "Resistencia al fuego. 12",
		"a resistance row uses the shorter value widget the layout gives it");
	Add(50, "AttackRatingTitle", "Índice de ataque");
	Add(51, "Tooltip", "", 50);
	Add(52, "AttackRatingStat", "214");
	Check(CharacterStats::Read(On(51), {}).option == "Índice de ataque. 214",
		"a row nobody listed still reads through the layout's own naming");

	widgets[12].text.clear();
	Check(CharacterStats::Read(On(11), {}).option == "Fuerza",
		"a value the game has not rendered yet leaves the label readable instead of nothing");
	widgets[10].text.clear();
	Check(CharacterStats::Read(On(11), {}).Empty(), "a row whose label has not rendered speaks nothing rather than a stale value");
	widgets[10].text = "Fuerza";

	panelVisible = false;
	Check(CharacterStats::Read(On(11), {}).Empty(), "a hidden panel announces nothing");
	panelVisible = true;

	widgets.erase(3);
	Add(3, "ClassInfo", "Bárbaro");
	Check(CharacterStats::Read(On(11), {}).body == "Personaje. Ulfr. Bárbaro. Nivel 7",
		"the original layout's class widget is read when the HD one is absent");
	widgets.erase(4);
	Check(CharacterStats::Read(On(11), {}).body == "Personaje. Ulfr. Bárbaro", "a missing identity part is skipped, not guessed");
	Passed();
}
