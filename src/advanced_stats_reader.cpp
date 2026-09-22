#include "advanced_stats_reader.h"

#include "async_log.h"
#include "character_stats_reader.h"
#include "localization.h"
#include "mercenary_stats.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tooltip_text.h"
#include "ui_text.h"

#include <string>
#include <string_view>

namespace D2RAccess::AdvancedStatsReader {
namespace {
constexpr std::string_view Panel = "AdvancedStatsPanel";
// Each row of the list is a copy of the layout's OptionTemplate, and its text
// is the StatText inside it (Controller/AdvancedStatsPanelHD.json).
constexpr std::string_view RowText = "StatText";

uintptr_t g_panel = 0;  // the opening the cursor belongs to
int g_cursor = -1;
std::string g_logged;   // the lines last written to the log, so a change is logged once

std::string ChildText(uintptr_t panel, std::string_view name) {
	return UiText::Clean(NativeWidgets::ReadText(NativeWidgets::FindChild(panel, name)));
}

// The experience line, labelled with the game's own word as the character
// panel's is -- the bare "0 of 500" says nothing on its own.
std::string Experience(std::string value) {
	if (value.empty()) return value;
	std::string line = UiText::Clean(Localization::Text("strchrexp"));
	if (!line.empty()) line += ". ";
	return line + value;
}

void Push(std::vector<std::string>& lines, std::string line) {
	if (!line.empty()) lines.push_back(std::move(line));
}

std::vector<std::string> Build(uintptr_t panel, uintptr_t mercenary, std::vector<MercenaryStats::Skill>* skillsOut) {
	std::vector<std::string> lines;
	if (!panel || !NativeUi::Visible(panel)) return lines;
	// For the character, the advanced list alone: the header above it is a
	// copy of the character panel's, which has its own reader (the user,
	// 2026-09-22: the character panel's buffer when advanced stats are not
	// active, "and advanced stats when it is active"). For a mercenary this is
	// the only place any of its statistics can be read -- its own panel shares
	// the screen with the inventory -- so everything comes first.
	if (mercenary) {
		// READ, 0x1414EA9BD: the game copies CharacterName and HireTypeText into
		// the header, and CombinedXP appears to be blanked -- so each part falls
		// back to the mercenary panel itself.
		auto name = ChildText(panel, "CharacterName");
		auto level = ChildText(panel, "FullClassInfo");
		auto experience = ChildText(panel, "CombinedXP");
		if (name.empty()) name = MercenaryStats::Name(mercenary);
		if (level.empty()) level = MercenaryStats::LevelAndType(mercenary);
		if (experience.empty()) experience = MercenaryStats::Experience(mercenary);
		Push(lines, std::move(name));
		Push(lines, std::move(level));
		Push(lines, Experience(std::move(experience)));
		for (auto& line : MercenaryStats::Lines(mercenary)) Push(lines, std::move(line));
		auto skills = MercenaryStats::Skills(mercenary);
		for (const auto& skill : skills)
			for (auto& line : TooltipText::Read(skill.text).lines) Push(lines, std::move(line));
		if (skillsOut) *skillsOut = std::move(skills);
	}
	// Snapshot omits hidden subtrees, so the row template never reads.
	for (const auto& node : NativeUi::Snapshot(panel, 0).nodes)
		if (node.name == RowText) Push(lines, UiText::Clean(node.text));
	return lines;
}

// Once per change: where the lines came from, each skill as the icon holds
// it, and every line, so one test session shows what the panel really says.
void Log(const D2RL::PluginContext* context, uintptr_t mercenary,
	const std::vector<MercenaryStats::Skill>& skills, const std::vector<std::string>& lines) {
	std::string joined;
	for (const auto& line : lines) joined += line + "\n";
	if (joined == g_logged) return;
	g_logged = std::move(joined);
	AsyncLog::InfoF(context, "[advanced] source=%s, %zu lines, %zu skill icons", mercenary ? "mercenary" : "character",
		lines.size(), skills.size());
	for (const auto& skill : skills)
		AsyncLog::InfoF(context, "[advanced] skill id=%d level=%d text=%zu bytes", skill.id, skill.level, skill.text.size());
	for (size_t at = 0; at < lines.size(); ++at)
		AsyncLog::InfoF(context, "[advanced] line %zu: \"%s\"", at + 1, lines[at].c_str());
}
}

uintptr_t Mercenary() {
	// READ, 0x1414EAA11 / 0x1414EAB51: the panel takes its header from the
	// character panel when that panel's visible flag (+0x51) is set, and
	// otherwise from the mercenary panel when its flag is -- so the panel it
	// came from stays open behind it, and the same test says which one it was.
	if (const uintptr_t character = NativeWidgets::FindPanel("CharacterStatsPanel"); character && NativeUi::Visible(character))
		return 0;
	const uintptr_t mercenary = NativeWidgets::FindPanel(MercenaryStats::Panel);
	return mercenary && NativeUi::Visible(mercenary) ? mercenary : 0;
}

std::vector<std::string> Lines(uintptr_t panel, uintptr_t mercenary) { return Build(panel, mercenary, nullptr); }

bool Tick(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	const uintptr_t panel = focus.panel && NativeWidgets::ReadName(focus.panel) == Panel
		? focus.panel : NativeWidgets::FindPanel(Panel);
	if (!panel || !NativeUi::Visible(panel)) { Reset(); return false; }
	if (panel != g_panel) { Reset(); g_panel = panel; }
	// Read the panel only once a push is pending: the walk costs a tree
	// traversal, and the stick is idle for all but a few of these ticks.
	InputDirection::StickPush push = InputDirection::StickPush::None;
	for (const auto wanted : {InputDirection::StickPush::Up, InputDirection::StickPush::Down,
	                          InputDirection::StickPush::Left, InputDirection::StickPush::Right}) {
		if (InputDirection::TakeRightStickPushIf(wanted, "advanced stats")) { push = wanted; break; }
	}
	if (push == InputDirection::StickPush::None) return false;
	const uintptr_t mercenary = Mercenary();
	std::vector<MercenaryStats::Skill> skills;
	const auto lines = Build(panel, mercenary, &skills);
	Log(context, mercenary, skills, lines);
	g_cursor = CharacterStatsReader::Advance(g_cursor, push, static_cast<int>(lines.size()));
	const std::string text = g_cursor < 0 ? Strings::Current().detailNone : lines[static_cast<size_t>(g_cursor)];
	AsyncLog::InfoF(context, "[advanced] %d of %zu -> \"%s\"", g_cursor + 1, lines.size(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

void Reset() {
	g_panel = 0;
	g_cursor = -1;
	g_logged.clear();
}

}
