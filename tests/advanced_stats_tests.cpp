#include "advanced_stats_reader.h"
#include "mercenary_stats.h"
#include "native_memory.h"
#include "speech_strings.h"
#include "test_support.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace D2RAccess;
// A slice of the Advanced Stats panel and of the mercenary panel behind it, as
// their layouts build them (Controller/AdvancedStatsPanelHD.json,
// Controller/HirelingInventoryPanelHD.json).
struct Widget {
	std::string name, text;
	uintptr_t parent = 0;
};
std::map<uintptr_t, Widget> widgets;
std::vector<uintptr_t> order; // declaration order, which is the order Snapshot walks
std::set<uintptr_t> hidden;
std::map<uintptr_t, int32_t> memory;       // the skill icons' id and level fields
std::map<uintptr_t, std::string> strings;  // the skill icons' description strings
std::vector<std::string> speech;
InputDirection::StickPush pending = InputDirection::StickPush::None;

constexpr uintptr_t Advanced = 1, Mercenary = 200, Character = 400;
void Add(uintptr_t id, std::string name, std::string text, uintptr_t parent) {
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
void Skill(uintptr_t id, int icon, int32_t skill, int32_t level, std::string text) {
	Add(id, "Skill" + std::to_string(icon), "", Mercenary);
	memory[id + 0x960] = skill;
	memory[id + 0x964] = level;
	strings[id + 0x698] = std::move(text);
}
std::string Push(InputDirection::StickPush push) {
	pending = push;
	speech.clear();
	const bool claimed = AdvancedStatsReader::Tick(nullptr, {Advanced, 0});
	return claimed && !speech.empty() ? speech.back() : std::string("<unclaimed>");
}
}

namespace D2RAccess::NativeUi {
Type Classify(uintptr_t) { return {}; }
// A ListWidget's rows are every child of its own row container, shown or not.
std::map<uintptr_t, uintptr_t> rowContainers;
std::vector<uintptr_t> ListRows(uintptr_t list) {
	std::vector<uintptr_t> rows;
	const auto container = rowContainers.find(list);
	if (container == rowContainers.end()) return rows;
	for (uintptr_t id : order)
		if (widgets[id].parent == container->second) rows.push_back(id);
	return rows;
}
std::string DescribeList(uintptr_t) { return {}; }
// Discovery reaches hidden widgets too; every widget in the fixture below the
// panel stands in for a text box, and the reader keeps the ones it names.
std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind) {
	std::vector<uintptr_t> found;
	for (uintptr_t id : order) if (Descends(id, panel)) found.push_back(id);
	return found;
}
bool Visible(uintptr_t widget) {
	for (auto at = widget; at != 0;) {
		const auto found = widgets.find(at);
		if (found == widgets.end() || hidden.count(at)) return false;
		at = found->second.parent;
	}
	return widget != 0;
}
uintptr_t Parent(uintptr_t widget) {
	const auto found = widgets.find(widget);
	return found == widgets.end() ? 0 : found->second.parent;
}
// The real walk visits the panel and then its visible subtree in declaration
// order, carrying each node's parent, name and text.
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t) {
	UiText::Snapshot snapshot;
	if (!Visible(panel)) return snapshot;
	snapshot.panel = panel;
	snapshot.name = widgets[panel].name;
	snapshot.nodes.push_back({panel, -1, UiText::Kind::Other, widgets[panel].name});
	for (uintptr_t id : order) {
		if (hidden.count(id)) continue;
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

namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t widget) {
	const auto found = widgets.find(widget);
	return found == widgets.end() ? std::string{} : found->second.name;
}
std::string ReadText(uintptr_t widget) {
	const auto found = widgets.find(widget);
	return found == widgets.end() ? std::string{} : found->second.text;
}
uintptr_t FindPanel(std::string_view name) {
	for (const auto& [id, widget] : widgets) if (widget.parent == 0 && widget.name == name) return id;
	return 0;
}
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	for (uintptr_t id : order)
		if (widgets[id].name == name && Descends(id, parent)) return id;
	return 0;
}
}

namespace D2RAccess::NativeMemory {
bool Copy(uintptr_t address, void* output, size_t size) noexcept {
	const auto found = memory.find(address);
	if (found == memory.end() || size != sizeof(int32_t)) return false;
	*static_cast<int32_t*>(output) = found->second;
	return true;
}
bool ReadEngineString(uintptr_t address, std::string& output) {
	const auto found = strings.find(address);
	if (found == strings.end()) return false;
	output = found->second;
	return true;
}
bool LooksLikeText(const std::string& text) { return !text.empty(); }
}

namespace D2RAccess::Localization {
std::string Text(std::string_view key) {
	if (key == "strchrexp") return "Experience";
	if (key == "strchrlvl") return "Level";
	return {};
}
}
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool) { speech.emplace_back(text); }
}
namespace D2RAccess::AsyncLog {
void Info(const D2RL::PluginContext*, const char*) noexcept {}
void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {}
}
namespace D2RAccess::InputDirection {
bool TakeRightStickPushIf(StickPush wanted, const char*) {
	if (pending != wanted) return false;
	pending = StickPush::None;
	return true;
}
}

int main() {
	using namespace D2RAccess;
	using InputDirection::StickPush;
	Strings::Select(Strings::Language::enUS);

	Check(MercenaryStats::CurrentOfMaximum("120 / 150") == "120 of 150", "the panel's life pair reads as current of maximum");
	Check(MercenaryStats::CurrentOfMaximum("4-12") == "4-12", "a damage range is left as the game wrote it");
	Check(MercenaryStats::CurrentOfMaximum("a / b") == "a / b", "only a pair of numbers is rewritten");

	// The mercenary panel, open behind the Advanced Stats panel.
	widgets[Mercenary] = {"HirelingInventoryPanel", "", 0};
	Add(201, "Title", "Mercenary", Mercenary);
	Add(202, "CharacterName", "Kyoko", Mercenary);
	Add(203, "HPTitle", "Life", Mercenary);
	Add(204, "HPStat", "120 / 150", Mercenary);
	Add(205, "HireTypeText", "Level 12 Rogue Scout", Mercenary);
	Add(206, "CurrentXP", "1200", Mercenary);
	Add(207, "NextLevelXP", "4000", Mercenary);
	Add(208, "StrengthTitle", "Strength", Mercenary);
	Add(209, "StrengthStat", "35", Mercenary);
	Add(210, "DexTitle", "Dexterity", Mercenary);
	Add(211, "DexStat", "52", Mercenary);
	Add(212, "DamageTitle", "Damage", Mercenary);
	Add(213, "Damage", "", Mercenary);
	Add(214, "DamageStat", "4-12", 213);
	Add(215, "DamageStatTop", "", 213);
	Add(216, "DamageStatBottom", "", 213);
	Add(217, "ArmorClassTitle", "Defense", Mercenary);
	Add(218, "ArmorClass", "40", Mercenary);
	Add(219, "FireResistanceTitle", "Fire Resistance", Mercenary);
	Add(220, "FireText", "18%", Mercenary);
	// The engine keeps a tooltip bottom-up: the name is its last line.
	Skill(230, 0, 8, 3, "Current Skill Level: 3\nFires a flaming arrow\nFire Arrow");
	Skill(231, 1, 17, 1, "Current Skill Level: 1\nInner Sight");
	Skill(232, 2, 0, 0, "Unused");
	hidden.insert(232);

	const auto rows = MercenaryStats::Lines(Mercenary);
	Check(rows.size() == 6, "every labelled row is a line, and the panel's own caption is not one");
	Check(rows[0] == "Life. 120 of 150" && rows[1] == "Strength. 35" && rows[2] == "Dexterity. 52",
		"rows keep the order the layout declares, each with its sibling value");
	Check(rows[3] == "Damage. 4-12", "damage reads the value inside its container");
	Check(rows[4] == "Defense. 40" && rows[5] == "Fire Resistance. 18%",
		"defense and the resistances use the value widgets the layout gives them");
	hidden.insert(214);
	widgets[215].text = "4-12";
	widgets[216].text = "6-18";
	Check(MercenaryStats::Lines(Mercenary)[3] == "Damage. 4-12, 6-18", "the two-line damage form joins whatever is shown");
	hidden.erase(214);
	widgets[215].text.clear();
	widgets[216].text.clear();

	const auto skills = MercenaryStats::Skills(Mercenary);
	Check(skills.size() == 2, "a hidden skill icon is left out");
	Check(skills[0].id == 8 && skills[0].level == 3 && skills[1].id == 17, "each icon's skill id and level are read");
	Check(MercenaryStats::Experience(Mercenary) == "1200 of 4000", "experience is current of next level");

	// The Advanced Stats panel, as the game fills it for a mercenary.
	widgets[Advanced] = {"AdvancedStatsPanel", "", 0};
	Add(2, "Title", "Advanced Stats", Advanced);
	Add(3, "CharacterName", "Kyoko", Advanced);
	Add(4, "FullClassInfo", "Level 12 Rogue Scout", Advanced);
	Add(5, "CombinedXP", "", Advanced);
	Add(6, "ListContainer", "", Advanced);
	Add(7, "OptionTemplate", "", 6);
	Add(8, "StatText", "Template", 7);
	hidden.insert(7);
	// The rows sit in the list's container, under the list's View (READ,
	// 0x140892F80 / 0x140885C38). Hidden here because the walk never reached
	// one live (MEASURED); the reader must not depend on that flag.
	Add(16, "View", "", 6);
	Add(9, "Container", "", 16);
	NativeUi::rowContainers[6] = 9;
	Add(10, "ListItem0", "", 9);
	Add(11, "StatText", "Faster Hit Recovery: 0%", 10);
	Add(12, "ListItem1", "", 9);
	Add(13, "StatText", "Magic Find: 0%", 12);
	hidden.insert(10);
	hidden.insert(12);
	bool walked = false;
	for (const auto& node : NativeUi::Snapshot(Advanced, 0).nodes) walked = walked || node.name == "StatText";
	Check(!walked, "the rows are not in the panel's walk, as measured");

	Check(AdvancedStatsReader::Mercenary() == Mercenary, "the open mercenary panel is the source");
	auto lines = AdvancedStatsReader::Lines(Advanced, Mercenary);
	const std::vector<std::string> expected = {
		"Kyoko", "Level 12 Rogue Scout", "Experience. 1200 of 4000",
		"Life. 120 of 150", "Strength. 35", "Dexterity. 52", "Damage. 4-12", "Defense. 40", "Fire Resistance. 18%",
		"Fire Arrow", "Fires a flaming arrow", "Current Skill Level: 3", "Inner Sight", "Current Skill Level: 1",
		"Faster Hit Recovery: 0%", "Magic Find: 0%",
	};
	Check(lines == expected, "header, the mercenary's statistics, its skills top first, then the advanced list");
	NativeUi::rowContainers.clear();
	Check(AdvancedStatsReader::Lines(Advanced, Mercenary) == expected,
		"with no row container the rows are found by searching the panel, and the template is left out");
	NativeUi::rowContainers[6] = 9;

	widgets[3].text.clear();
	widgets[4].text.clear();
	lines = AdvancedStatsReader::Lines(Advanced, Mercenary);
	Check(lines[0] == "Kyoko" && lines[1] == "Level 12 Rogue Scout", "a header the game left blank falls back to the mercenary panel");
	widgets[3].text = "Kyoko";
	widgets[4].text = "Level 12 Rogue Scout";

	// The right stick walks the lines.
	Check(Push(StickPush::Down) == "Kyoko", "the first push down reads the first line");
	Check(Push(StickPush::Down) == "Level 12 Rogue Scout", "down moves to the next line");
	Check(Push(StickPush::Up) == "Kyoko", "up moves back");
	Check(Push(StickPush::Up) == "Kyoko", "up at the top re-reads the first line");
	Check(Push(StickPush::Right) == "Magic Find: 0%", "right jumps to the last line");
	Check(Push(StickPush::Down) == "Magic Find: 0%", "down at the bottom re-reads the last line");
	Check(Push(StickPush::Left) == "Kyoko", "left jumps to the first line");
	pending = StickPush::None;
	speech.clear();
	Check(AdvancedStatsReader::Tick(nullptr, {Advanced, 0}) && speech.empty(),
		"without a push the focused panel holds the tick and says nothing");
	Check(!AdvancedStatsReader::Tick(nullptr, {}), "with focus elsewhere an idle tick is left to the other readers");
	Push(StickPush::Right);
	hidden.insert(Advanced);
	Check(!AdvancedStatsReader::Tick(nullptr, {Advanced, 0}), "a closed panel claims nothing");
	hidden.erase(Advanced);
	Check(Push(StickPush::Down) == "Kyoko", "reopening the panel starts again from the top");

	// Opened from the character panel instead: no mercenary part.
	hidden.insert(Mercenary);
	Check(AdvancedStatsReader::Mercenary() == 0, "a closed mercenary panel is not the source");
	hidden.erase(Mercenary);
	widgets[Character] = {"CharacterStatsPanel", "", 0};
	Check(AdvancedStatsReader::Mercenary() == 0, "the character panel wins when both are open, as it does for the game");
	widgets[3].text = "Ulfr";
	widgets[4].text = "Level 7 Barbarian";
	widgets[5].text = "900 of 1500";
	lines = AdvancedStatsReader::Lines(Advanced, AdvancedStatsReader::Mercenary());
	Check(lines == std::vector<std::string>{"Faster Hit Recovery: 0%", "Magic Find: 0%"},
		"from the character panel only the advanced list is read; the header is the character panel's own");

	Passed();
	return 0;
}
