#include "character_stats.h"
#include "localization.h"
#include "mercenary_stats.h"
#include "native_widgets.h"
#include "speech_strings.h"
#include "tooltip_text.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace D2RAccess::CharacterStats {
namespace {
// The panel is named the same in every variant (CharacterStatsPanel.json,
// CharacterStatsPanelHD.json and Controller/CharacterStatsPanelHD.json).
constexpr std::string_view CharacterPanel = "CharacterStatsPanel";
constexpr std::string_view HirelingPanel = "HirelingInventoryPanel";
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
constexpr Row ValueRows[] = {
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

constexpr size_t RightHand = 4, LeftHand = 5; // equipmentSlots, by the game's body locations

// A row's value by the rules above; `text` gives the words of the widget called `name`.
template <class Lookup>
std::string RowValue(std::string_view title, const Lookup& text) {
	for (const auto& row : ValueRows) {
		if (title != row.title) continue;
		const auto current = text(row.current);
		if (!row.maximum || current.empty()) return current;
		const auto maximum = text(row.maximum);
		if (maximum.empty()) return current;
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().currentOfMaximum, current.c_str(), maximum.c_str());
		return phrase;
	}
	const std::string stem(title.substr(0, title.size() - TitleSuffix.size()));
	for (const char* suffix : ValueSuffixes)
		if (auto value = text(stem + suffix); !value.empty()) return value;
	// Two weapons in hand: the game empties <stem>Stat and states one number per weapon, the right
	// hand's on top. MEASURED 2026-11-04, docs/character-panel.md.
	const auto top = text(stem + "StatTop"), bottom = text(stem + "StatBottom");
	if (top.empty() || bottom.empty()) return top + bottom;
	const auto& slots = Strings::Current().equipmentSlots;
	return std::string(slots[RightHand]) + " " + top + ", " + slots[LeftHand] + " " + bottom;
}

std::string Value(uintptr_t panel, std::string_view title, std::string_view panelName) {
	// The hireling layout uses HPStat, while the character panel splits life
	// into CurrentHP and MaxHP. Its other rows use the shared naming rules.
	if (panelName == HirelingPanel && title == "HPTitle") return ChildText(panel, "HPStat");
	return RowValue(title, [&](std::string_view name) { return ChildText(panel, name); });
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
	return RowValue(title, [&](std::string_view name) { return SiblingText(snapshot, parent, name); });
}

// The character's class and level as the panel states them. The HD layout adds
// FullClassInfo beside ClassInfo, and says the level in it ("Level 1 Paladin");
// the original layout keeps the level in a box of its own.
std::string LevelAndClass(uintptr_t panel) {
	auto result = ChildText(panel, "FullClassInfo");
	if (result.empty()) result = ChildText(panel, "ClassInfo");
	if (const auto level = ChildText(panel, "Level"); !level.empty()) {
		const auto label = Localization::Text("strchrlvl");
		Append(result, label.empty() ? level : label + " " + level);
	}
	return result;
}

// Panel name plus who the character is, so entering the panel says both once.
std::string Identity(uintptr_t panel) {
	const auto panelName = NativeWidgets::ReadName(panel);
	std::string result = Localization::Text(panelName == HirelingPanel ? "MiniPanelHireinv" : "minipanelchar");
	Append(result, ChildText(panel, "CharacterName"));
	Append(result, panelName == HirelingPanel ? ChildText(panel, "HireTypeText") : LevelAndClass(panel));
	return result;
}

bool SupportedPanel(std::string_view name) { return name == CharacterPanel || name == HirelingPanel; }
}

bool HandlesFocus(NativeUi::Focus focus, const NativeUi::Type&) {
	if (!focus.panel || !focus.widget || !SupportedPanel(NativeWidgets::ReadName(focus.panel))) return false;
	return TitleOf(focus) != 0;
}

UiText::Readout Read(NativeUi::Focus focus, const NativeUi::Type& type) {
	UiText::Readout result;
	if (!HandlesFocus(focus, type) || !NativeUi::Visible(focus.panel)) return result;
	const auto title = TitleOf(focus);
	const auto label = Text(title);
	if (label.empty()) return result; // the row has not rendered its label yet
	result.context = NativeWidgets::ReadName(focus.panel);
	result.body = Identity(focus.panel);
	result.option = label;
	// Among the row's own siblings, not anywhere under the panel: the attack
	// damage and attack rating rows exist twice in the layout, once under
	// LeftSkillStats and once under RightSkillStats, with identical widget
	// names, so a whole-panel lookup could only ever find the first of each
	// pair. Falls back to the panel when the container cannot be read, which
	// is the lookup this had before and still better than saying nothing.
	const auto container = NativeUi::Parent(title);
	Append(result.option, Value(container ? container : focus.panel, NativeWidgets::ReadName(title), result.context));
	result.focusKey = std::to_string(focus.widget);
	return result;
}

namespace {
// The tooltip a row titled `title` carries, in the skill block `block` (its grandparent) when it has one.
Chance ChanceOf(std::string_view panel, std::string_view title, std::string_view block) {
	if (panel != CharacterPanel) return Chance::None; // the hireling's would be the player's numbers
	if (title == "ArmorClassTitle") return Chance::Defense;
	if (title != "AttackRatingTitle") return Chance::None;
	return block == "LeftSkillStats" ? Chance::LeftAttack : block == "RightSkillStats" ? Chance::RightAttack : Chance::None;
}
}

std::vector<std::string> Lines(uintptr_t panel) {
	std::vector<std::string> lines;
	for (auto& row : Rows(panel)) lines.push_back(std::move(row.text));
	return lines;
}

std::vector<Line> Rows(uintptr_t panel) {
	std::vector<Line> lines;
	if (!panel || !NativeUi::Visible(panel)) return lines;
	// Who the character is leads the list: the name, then the level and class.
	if (auto name = Name(panel); !name.empty()) lines.push_back({std::move(name)});
	const auto panelName = NativeWidgets::ReadName(panel);
	if (auto level = panelName == HirelingPanel ? ChildText(panel, "HireTypeText") : LevelAndClass(panel); !level.empty())
		lines.push_back({std::move(level)});
	if (panelName == HirelingPanel) {
		if (auto experience = MercenaryStats::Experience(panel); !experience.empty()) lines.push_back({std::move(experience)});
		for (auto& skill : MercenaryStats::SkillLines(panel)) lines.push_back({std::move(skill)});
	}
	// Snapshot walks the visible tree in declaration order and omits hidden
	// subtrees, so a row the game is not showing never reaches the list.
	const auto snapshot = NativeUi::Snapshot(panel, 0);
	for (const auto& node : snapshot.nodes) {
		const auto text = UiText::Clean(node.text);
		if (text.empty()) continue;
		if (std::string_view(node.name).ends_with(TitleSuffix)) {
			std::string line = text;
			const auto value = panelName == HirelingPanel
				? Value(panel, node.name, panelName)
				: SiblingValue(snapshot, node.parent, node.name);
			Append(line, value);
			const auto grandparent = [&]() -> std::string_view {
				if (node.parent < 0) return {};
				const int up = snapshot.nodes[static_cast<size_t>(node.parent)].parent;
				return up < 0 ? std::string_view{} : std::string_view(snapshot.nodes[static_cast<size_t>(up)].name);
			};
			lines.push_back({std::move(line), ChanceOf(panelName, node.name, grandparent())});
			continue;
		}
		for (const auto& standalone : StandaloneLines) {
			if (node.name != standalone.widget) continue;
			std::string line = standalone.labelKey ? UiText::Clean(Localization::Text(standalone.labelKey)) : std::string{};
			Append(line, text);
			lines.push_back({std::move(line)});
			break;
		}
	}
	return lines;
}

std::string Name(uintptr_t panel) { return ChildText(panel, "CharacterName"); }

std::string Points(uintptr_t panel) { return ChildText(panel, "CombinedPointsInfo"); }
std::string SkillChoices(uintptr_t panel) { return ChildText(panel, "SkillCounter"); }

Attribute AttributeOf(uintptr_t panel, std::string_view button) {
	constexpr std::string_view ButtonSuffix = "PointButton"; // the layouts' <stat>PointButton
	if (!button.ends_with(ButtonSuffix)) return {};
	const std::string stem(button.substr(0, button.size() - ButtonSuffix.size()));
	return {ChildText(panel, stem + std::string(TitleSuffix)), ChildText(panel, stem + "Stat")};
}

bool HasPoints(std::string_view points) {
	const auto first = points.find_first_of("0123456789");
	if (first == std::string_view::npos) return false;
	const auto last = points.find_first_not_of("0123456789", first);
	const auto digits = points.substr(first, last == std::string_view::npos ? last : last - first);
	return digits.find_first_not_of('0') != std::string_view::npos;
}

std::string HoverWords(std::string_view body) {
	std::string words;
	for (const auto& line : TooltipText::Read(body).lines) {
		// A heading line ending in a colon runs on into its value ("Average chance to hit: Fallen: 92%").
		if (!words.empty()) words += words.back() == ':' ? " " : ". ";
		words += line;
	}
	return words;
}

void Reset() {}

}
