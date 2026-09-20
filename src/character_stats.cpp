#include "character_stats.h"
#include "localization.h"
#include "native_widgets.h"
#include "speech_strings.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace D2RAccess::CharacterStats {
namespace {
// The panel is named the same in every variant (CharacterStatsPanel.json,
// CharacterStatsPanelHD.json and Controller/CharacterStatsPanelHD.json).
constexpr std::string_view Panel = "CharacterStatsPanel";
// Every row is a <name>Title label; the controller layout gives each one a
// focusable child called Tooltip, which is where the controller stops.
constexpr std::string_view TitleSuffix = "Title";
constexpr std::string_view FocusChild = "Tooltip";

// Rows whose value widget is not simply the title's stem plus a suffix, from
// the layouts above. Everything else is resolved generically, so a row that is
// not listed here still reads as long as the game names it in the usual way.
struct Row {
	const char* title;
	const char* current;
	const char* maximum; // null when the row shows a single number
};
constexpr Row Rows[] = {
	{"ArmorClassTitle", "ArmorClass", nullptr},
	{"StaminaTitle", "CurrentStamina", "MaxStamina"},
	{"HPTitle", "CurrentHP", "MaxHP"},
	{"ManaTitle", "CurrentMana", "MaxMana"},
	{"FireResistanceTitle", "FireText", nullptr},
	{"ColdResistanceTitle", "ColdText", nullptr},
	{"LightningResistanceTitle", "LightningText", nullptr},
	{"PoisonResistanceTitle", "PoisonText", nullptr},
};
// Suffixes the layouts use for a row's value widget (StrengthStat, FireText).
constexpr const char* ValueSuffixes[] = {"Stat", "Text"};
// Statistics the panel states in a single text box, with no <name>Title of
// their own. Where the box does not say what its number is, the game's own
// word for it is borrowed -- MEASURED 2026-09-20: CombinedXP renders as a
// bare "0 of 500", which says nothing at all on its own. CombinedPointsInfo
// reads "0 Stat Points Remaining" and needs no help.
struct Standalone { const char* widget; const char* labelKey; };
constexpr Standalone StandaloneLines[] = {{"CombinedXP", "strchrexp"}, {"CombinedPointsInfo", nullptr}};
void Append(std::string& text, std::string_view part) {
	if (part.empty() || text == part) return;
	if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
	text += part;
}

std::string Text(uintptr_t widget) { return UiText::Clean(NativeWidgets::ReadText(widget)); }

std::string ChildText(uintptr_t parent, std::string_view name) {
	return parent ? Text(NativeWidgets::FindChild(parent, name)) : std::string{};
}

// The <name>Title widget the focus belongs to: the focused widget itself, or
// its parent when the game stopped on the focusable Tooltip child.
uintptr_t TitleOf(NativeUi::Focus focus) {
	if (NativeWidgets::ReadName(focus.widget).ends_with(TitleSuffix)) return focus.widget;
	if (NativeWidgets::ReadName(focus.widget) != FocusChild) return 0;
	const auto parent = NativeUi::Parent(focus.widget);
	return NativeWidgets::ReadName(parent).ends_with(TitleSuffix) ? parent : 0;
}

std::string Value(uintptr_t panel, std::string_view title) {
	for (const auto& row : Rows) {
		if (title != row.title) continue;
		const auto current = ChildText(panel, row.current);
		if (!row.maximum || current.empty()) return current;
		const auto maximum = ChildText(panel, row.maximum);
		if (maximum.empty()) return current;
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().currentOfMaximum, current.c_str(), maximum.c_str());
		return phrase;
	}
	const std::string stem(title.substr(0, title.size() - TitleSuffix.size()));
	for (const char* suffix : ValueSuffixes)
		if (auto value = ChildText(panel, stem + suffix); !value.empty()) return value;
	return {};
}

// Text of the sibling called `name`, i.e. a node with the same parent. Rows
// resolve their values this way so the two AttackDamageTitle rows, which have
// the same widget names under LeftSkillStats and RightSkillStats, cannot read
// each other's numbers.
std::string SiblingText(const UiText::Snapshot& snapshot, int parent, std::string_view name) {
	for (const auto& node : snapshot.nodes)
		if (node.parent == parent && node.name == name) return UiText::Clean(node.text);
	return {};
}

// Value of a row, by the same rules as Value() above but scoped to siblings.
std::string SiblingValue(const UiText::Snapshot& snapshot, int parent, std::string_view title) {
	for (const auto& row : Rows) {
		if (title != row.title) continue;
		const auto current = SiblingText(snapshot, parent, row.current);
		if (!row.maximum || current.empty()) return current;
		const auto maximum = SiblingText(snapshot, parent, row.maximum);
		if (maximum.empty()) return current;
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().currentOfMaximum, current.c_str(), maximum.c_str());
		return phrase;
	}
	const std::string stem(title.substr(0, title.size() - TitleSuffix.size()));
	for (const char* suffix : ValueSuffixes)
		if (auto value = SiblingText(snapshot, parent, stem + suffix); !value.empty()) return value;
	return {};
}

// Panel name plus who the character is, so entering the panel says both once.
std::string Identity(uintptr_t panel) {
	std::string result = Localization::Text("minipanelchar");
	Append(result, ChildText(panel, "CharacterName"));
	// The HD layout adds FullClassInfo beside ClassInfo; either one names the class.
	auto characterClass = ChildText(panel, "FullClassInfo");
	if (characterClass.empty()) characterClass = ChildText(panel, "ClassInfo");
	Append(result, characterClass);
	if (const auto level = ChildText(panel, "Level"); !level.empty()) {
		const auto label = Localization::Text("strchrlvl");
		Append(result, label.empty() ? level : label + " " + level);
	}
	return result;
}
}

bool HandlesFocus(NativeUi::Focus focus, const NativeUi::Type&) {
	if (!focus.panel || !focus.widget || NativeWidgets::ReadName(focus.panel) != Panel) return false;
	return TitleOf(focus) != 0;
}

UiText::Readout Read(NativeUi::Focus focus, const NativeUi::Type& type) {
	UiText::Readout result;
	if (!HandlesFocus(focus, type) || !NativeUi::Visible(focus.panel)) return result;
	const auto title = TitleOf(focus);
	const auto label = Text(title);
	if (label.empty()) return result; // the row has not rendered its label yet
	result.context = Panel;
	result.body = Identity(focus.panel);
	result.option = label;
	// Among the row's own siblings, not anywhere under the panel: the attack
	// damage and attack rating rows exist twice in the layout, once under
	// LeftSkillStats and once under RightSkillStats, with identical widget
	// names, so a whole-panel lookup could only ever find the first of each
	// pair. Falls back to the panel when the container cannot be read, which
	// is the lookup this had before and still better than saying nothing.
	const auto container = NativeUi::Parent(title);
	Append(result.option, Value(container ? container : focus.panel, NativeWidgets::ReadName(title)));
	result.focusKey = std::to_string(focus.widget);
	return result;
}

std::vector<std::string> Lines(uintptr_t panel) {
	std::vector<std::string> lines;
	if (!panel || !NativeUi::Visible(panel)) return lines;
	// Snapshot walks the visible tree in declaration order and omits hidden
	// subtrees, so a row the game is not showing never reaches the list.
	const auto snapshot = NativeUi::Snapshot(panel, 0);
	for (const auto& node : snapshot.nodes) {
		const auto text = UiText::Clean(node.text);
		if (text.empty()) continue;
		if (std::string_view(node.name).ends_with(TitleSuffix)) {
			std::string line = text;
			Append(line, SiblingValue(snapshot, node.parent, node.name));
			lines.push_back(std::move(line));
			continue;
		}
		for (const auto& standalone : StandaloneLines) {
			if (node.name != standalone.widget) continue;
			std::string line = standalone.labelKey ? UiText::Clean(Localization::Text(standalone.labelKey)) : std::string{};
			Append(line, text);
			lines.push_back(std::move(line));
			break;
		}
	}
	return lines;
}

void Reset() {}

}
