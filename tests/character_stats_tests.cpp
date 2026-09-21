#include "character_stats.h"
#include "character_stats_reader.h"
#include "speech_strings.h"
#include "test_support.h"

#include <map>
#include <string>
#include <vector>

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
std::vector<uintptr_t> order; // declaration order, which is the order Snapshot walks
bool panelVisible = true;
std::vector<std::string> speech;
std::vector<bool> interrupts;
InputDirection::StickPush pending = InputDirection::StickPush::None;

constexpr uintptr_t Panel = 1;
void Add(uintptr_t id, std::string name, std::string text = {}, uintptr_t parent = Panel) {
	if (!widgets.count(id)) order.push_back(id);
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
// The real walk visits the panel and then its subtree in declaration order,
// carrying each node's parent, name and text. Nothing else here reads a node.
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t) {
	UiText::Snapshot snapshot;
	if (!Visible(panel)) return snapshot;
	snapshot.panel = panel;
	snapshot.name = widgets[panel].name;
	snapshot.nodes.push_back({panel, -1, UiText::Kind::Other, widgets[panel].name});
	for (uintptr_t id : order) {
		const auto& widget = widgets[id];
		int parent = -1;
		for (size_t at = 0; at < snapshot.nodes.size(); ++at)
			if (snapshot.nodes[at].id == widget.parent) parent = static_cast<int>(at);
		if (parent < 0) continue;
		snapshot.nodes.push_back({id, parent, UiText::Kind::Text, widget.name, "TextBoxWidget", widget.text});
	}
	return snapshot;
}
}

namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool interrupt) { speech.emplace_back(text); interrupts.push_back(interrupt); }
}
namespace D2RAccess::AsyncLog {
void Info(const D2RL::PluginContext*, const char*) {}
void InfoF(const D2RL::PluginContext*, const char*, ...) {}
}
namespace D2RAccess::InputDirection {
bool TakeRightStickPushIf(StickPush wanted, const char*) {
	if (pending != wanted) return false;
	pending = StickPush::None;
	return true;
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
// The panel the game would hand back for a name, whether or not anything on
// it has focus -- which on this panel nothing does.
uintptr_t FindPanel(std::string_view name) {
	for (const auto& [id, widget] : widgets) if (widget.name == name) return id;
	return 0;
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
	if (key == "strchrexp") return "Experiencia";
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

	// Enumeration: every row of the panel, not just the focused one.
	widgets[12].text = "30"; // the value the row-level checks above left cleared
	Add(60, "CombinedXP", "0 de 500"); // as the game renders it: a bare pair of numbers
	Add(70, "LeftSkillStats");
	Add(71, "Damage", "", 70);
	Add(72, "AttackDamageTitle", "Daño", 71);
	Add(73, "AttackDamageStat", "12 a 30", 71);
	Add(80, "RightSkillStats");
	Add(81, "Damage", "", 80);
	Add(82, "AttackDamageTitle", "Daño", 81);
	Add(83, "AttackDamageStat", "5 a 9", 81);
	auto lines = CharacterStats::Lines(Panel);
	Check(lines.size() == 8, "every rendered row is a line, and nothing else on the panel is");
	Check(lines[0] == "Fuerza. 30" && lines[1] == "Vida. 45 de 120", "lines keep the order the layout declares");
	Check(lines[5] == "Experiencia. 0 de 500",
		"a box that states a statistic without naming it borrows the game's own word for it");
	Check(lines[6] == "Daño. 12 a 30" && lines[7] == "Daño. 5 a 9",
		"the two identically named attack rows read their own siblings, not each other's");
	widgets[12].text.clear();
	Check(CharacterStats::Lines(Panel)[0] == "Fuerza", "a row whose value has not rendered still offers its label");
	widgets[12].text = "30";
	panelVisible = false;
	Check(CharacterStats::Lines(Panel).empty(), "a hidden panel enumerates nothing");
	panelVisible = true;

	// The same pair through the focused-row reader, which searched the whole
	// panel and so could only ever find the first of the two.
	Add(74, "Tooltip", "", 72);
	Add(84, "Tooltip", "", 82);
	Check(CharacterStats::Read(On(74), {}).option == "Daño. 12 a 30", "a focused attack row reads the value beside it");
	Check(CharacterStats::Read(On(84), {}).option == "Daño. 5 a 9", "and the identically named row elsewhere reads its own, not the first one's");

	// The cursor the right stick walks: it never leaves the list.
	using Push = InputDirection::StickPush;
	Check(CharacterStatsReader::Advance(-1, Push::Down, 8) == 0, "the first push down reads the first line");
	Check(CharacterStatsReader::Advance(-1, Push::Up, 8) == 0, "a push up before anything was read starts at the first line too");
	Check(CharacterStatsReader::Advance(7, Push::Down, 8) == 7, "a push down at the last line re-reads it rather than wrapping");
	Check(CharacterStatsReader::Advance(0, Push::Up, 8) == 0, "a push up at the first line re-reads it rather than wrapping");
	Check(CharacterStatsReader::Advance(3, Push::Left, 8) == 0 && CharacterStatsReader::Advance(3, Push::Right, 8) == 7,
		"left and right jump to the ends");
	Check(CharacterStatsReader::Advance(3, Push::None, 8) == 3, "no push leaves the cursor where it is");
	Check(CharacterStatsReader::Advance(3, Push::Down, 0) == -1, "a panel with nothing to read has no cursor");

	// The gesture itself, and who it is claimed from.
	// Opening the panel names the character once, behind the panel's title.
	widgets[2].text.clear();
	CharacterStatsReader::Reset();
	Check(!CharacterStatsReader::Tick(nullptr, On(11)), "an idle stick is left for the entity scan and the pathfinder");
	Check(speech.empty(), "a name the game has not rendered yet is not spoken");
	widgets[2].text = "Ulfr";
	CharacterStatsReader::Tick(nullptr, On(11));
	Check(speech == std::vector<std::string>{"Ulfr"} && !interrupts.back(),
		"the name is spoken once it renders, queued behind the title instead of cutting it off");
	CharacterStatsReader::Tick(nullptr, On(11));
	Check(speech.size() == 1, "and only once while the panel stays open");
	speech.clear();
	pending = Push::Down;
	Check(CharacterStatsReader::Tick(nullptr, On(11)) && speech.size() == 1 && speech.back() == "Fuerza. 30",
		"a push down on the character panel reads the first statistic");
	pending = Push::Right;
	Check(CharacterStatsReader::Tick(nullptr, On(11)) && speech.back() == "Daño. 5 a 9", "right reads the last statistic");
	pending = Push::Up;
	Check(CharacterStatsReader::Tick(nullptr, On(11)) && speech.back() == "Daño. 12 a 30", "up steps back one statistic");
	pending = Push::Up;
	widgets[Panel].name = "PlayerInventoryExpansionLayout";
	Check(!CharacterStatsReader::Tick(nullptr, On(11)) && pending == Push::Up,
		"another panel keeps its own use of the stick, push and all");
	widgets[Panel].name = "CharacterStatsPanel";
	pending = Push::Down;
	Check(CharacterStatsReader::Tick(nullptr, On(11)) && speech.back() == "Fuerza. 30",
		"leaving the panel forgets where the cursor was");

	// MEASURED 2026-09-20: the focus manager reports no focused widget at all
	// while this panel is up, so waiting for one left the stick to the entity
	// scan for a whole session. The panel being open is what counts.
	CharacterStatsReader::Reset();
	pending = Push::Down;
	Check(CharacterStatsReader::Tick(nullptr, {}) && speech.back() == "Fuerza. 30",
		"the open panel claims the stick even when nothing on it has focus");
	pending = Push::Down;
	panelVisible = false;
	Check(!CharacterStatsReader::Tick(nullptr, {}) && pending == Push::Down,
		"and a panel that is not showing leaves the stick alone, push and all");
	panelVisible = true;
	Passed();
}
