#include "mercenary_stats.h"

#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "speech_strings.h"
#include "tooltip_text.h"
#include "ui_text.h"

#include <cstdio>

namespace D2RAccess::MercenaryStats {
namespace {
// Every row is a <name>Title label with its value in a sibling widget
// (Controller/HirelingInventoryPanelHD.json and the layout it is based on).
// The panel's own caption is called just "Title" and is not a row.
constexpr std::string_view TitleSuffix = "Title";
// Rows whose value is not the title's stem plus Stat or Text, or the stem
// itself. Anything not listed still reads as long as the game names it in
// the usual way.
struct Row { const char* title; const char* value; };
constexpr Row Rows[] = {
	{"FireResistanceTitle", "FireText"},
	{"ColdResistanceTitle", "ColdText"},
	{"LightningResistanceTitle", "LightningText"},
	{"PoisonResistanceTitle", "PoisonText"},
};
constexpr const char* ValueSuffixes[] = {"Stat", "Text"};

// READ from the 3.3.93787 image, not measured: 0x1414F2D34 walks the
// mercenary's skills and hands each to the icon called "Skill<n>", counting up
// from Skill0; the setter at 0x1414F0F20 stores the skill id at +0x960 and its
// level at +0x964, builds the skill's description with the game's SkillDesc
// code and moves it into the engine string at +0x698 -- the text the icon's
// tooltip shows.
constexpr int    SkillIcons       = 3;
constexpr size_t SkillIdOffset    = 0x960;
constexpr size_t SkillLevelOffset = 0x964;
constexpr size_t SkillTextOffset  = 0x698;

void Append(std::string& text, std::string_view part) {
	if (part.empty() || text == part) return;
	if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
	text += part;
}

std::string ChildText(uintptr_t panel, std::string_view name) {
	return panel ? UiText::Clean(NativeWidgets::ReadText(NativeWidgets::FindChild(panel, name))) : std::string{};
}

int SiblingIndex(const UiText::Snapshot& snapshot, int parent, std::string_view name) {
	for (size_t at = 0; at < snapshot.nodes.size(); ++at)
		if (snapshot.nodes[at].parent == parent && snapshot.nodes[at].name == name) return static_cast<int>(at);
	return -1;
}

std::string SiblingText(const UiText::Snapshot& snapshot, int parent, std::string_view name) {
	const int at = SiblingIndex(snapshot, parent, name);
	return at < 0 ? std::string{} : UiText::Clean(snapshot.nodes[static_cast<size_t>(at)].text);
}

// The value of a row. Damage is the one row whose value is a container: the
// "Damage" widget holds DamageStat and, the layout's other form, DamageStatTop
// over DamageStatBottom. Whichever of them the game is showing are joined.
std::string Value(const UiText::Snapshot& snapshot, int parent, std::string_view title) {
	for (const auto& row : Rows)
		if (title == row.title) return SiblingText(snapshot, parent, row.value);
	const std::string stem(title.substr(0, title.size() - TitleSuffix.size()));
	for (const char* suffix : ValueSuffixes)
		if (auto value = SiblingText(snapshot, parent, stem + suffix); !value.empty()) return value;
	const int container = SiblingIndex(snapshot, parent, stem);
	if (container < 0) return {};
	if (auto value = UiText::Clean(snapshot.nodes[static_cast<size_t>(container)].text); !value.empty()) return value;
	std::string joined;
	for (const auto& node : snapshot.nodes) {
		if (node.parent != container) continue;
		const auto text = UiText::Clean(node.text);
		if (text.empty()) continue;
		if (!joined.empty()) joined += Strings::Current().listSeparator;
		joined += text;
	}
	return joined;
}
}

std::string Name(uintptr_t panel) { return ChildText(panel, "CharacterName"); }

std::string LevelAndType(uintptr_t panel) { return ChildText(panel, "HireTypeText"); }

std::string Experience(uintptr_t panel) {
	const auto current = ChildText(panel, "CurrentXP");
	const auto next = ChildText(panel, "NextLevelXP");
	if (current.empty() || next.empty()) return current;
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().currentOfMaximum, current.c_str(), next.c_str());
	return phrase;
}

std::vector<std::string> Lines(uintptr_t panel) {
	std::vector<std::string> lines;
	if (!panel) return lines;
	// Snapshot walks the visible tree in declaration order and omits hidden
	// subtrees, so a row the game is not showing never reaches the list.
	const auto snapshot = NativeUi::Snapshot(panel, 0);
	for (const auto& node : snapshot.nodes) {
		const std::string_view name = node.name;
		if (name.size() <= TitleSuffix.size() || !name.ends_with(TitleSuffix)) continue;
		std::string line = UiText::Clean(node.text);
		if (line.empty()) continue;
		Append(line, CurrentOfMaximum(Value(snapshot, node.parent, name)));
		lines.push_back(std::move(line));
	}
	return lines;
}

std::vector<Skill> Skills(uintptr_t panel) {
	std::vector<Skill> skills;
	if (!panel) return skills;
	for (int icon = 0; icon < SkillIcons; ++icon) {
		const uintptr_t widget = NativeWidgets::FindChild(panel, "Skill" + std::to_string(icon));
		if (!widget || !NativeUi::Visible(widget)) continue;
		skills.push_back(SkillOf(widget));
	}
	return skills;
}

std::vector<std::string> SkillLines(uintptr_t panel) {
	std::vector<std::string> lines;
	for (const auto& skill : Skills(panel)) {
		std::string line;
		for (const auto& part : TooltipText::Read(skill.text).lines) {
			if (!line.empty()) line += line.ends_with('.') || line.ends_with(':') ? " " : ". ";
			line += part;
		}
		if (!line.empty()) lines.push_back(std::move(line));
	}
	return lines;
}

Skill SkillOf(uintptr_t icon) {
	Skill skill;
	if (!icon) return skill;
	int32_t value = -1;
	if (NativeMemory::Peek(icon + SkillIdOffset, value)) skill.id = value;
	if (NativeMemory::Peek(icon + SkillLevelOffset, value)) skill.level = value;
	std::string text;
	if (NativeMemory::ReadEngineString(icon + SkillTextOffset, text) && NativeMemory::LooksLikeText(text))
		skill.text = std::move(text);
	return skill;
}

std::string CurrentOfMaximum(std::string_view text) {
	const size_t slash = text.find(" / ");
	if (slash == std::string_view::npos || slash == 0 || slash + 3 >= text.size()) return std::string(text);
	const auto digits = [](std::string_view part) {
		for (const char ch : part)
			if (ch < '0' || ch > '9') return false;
		return true;
	};
	const auto current = text.substr(0, slash), maximum = text.substr(slash + 3);
	if (!digits(current) || !digits(maximum)) return std::string(text);
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().currentOfMaximum,
		std::string(current).c_str(), std::string(maximum).c_str());
	return phrase;
}

}
