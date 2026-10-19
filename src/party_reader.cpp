#include "party_reader.h"

#include "async_log.h"
#include "character_stats_reader.h"
#include "input_direction.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "ui_text.h"

#include <string_view>

namespace D2RAccess::PartyReader {
namespace {
// PartyPanelHD.json: a heading and a value for the game, then a row per player,
// each a copy of the list's FriendsListTemplate (NativeUi::ListRows).
constexpr std::string_view Panel = "PartyPanel";
constexpr std::string_view List = "ListContainer";
constexpr std::string_view RowParts[] = {"PlayerName", "PlayerInfo", "PartyInfoText"};
constexpr std::string_view Headings[][2] = {{"GameNameHeadingText", "GameNameText"}, {"PasswordHeadingText", "PasswordText"}};

uintptr_t g_panel = 0;
int g_cursor = -1;
bool g_logged = false;

std::string ChildText(uintptr_t parent, std::string_view name) {
	const uintptr_t child = NativeWidgets::FindChild(parent, name);
	return child && NativeUi::Visible(child) ? UiText::Clean(NativeWidgets::ReadText(child)) : std::string{};
}

void Join(std::string& line, const std::string& part) {
	if (part.empty()) return;
	if (!line.empty()) line += ". ";
	line += part;
}
}

std::vector<std::string> Lines(uintptr_t panel) {
	std::vector<std::string> lines;
	if (!panel || !NativeUi::Visible(panel)) return lines;
	for (const auto& [heading, value] : Headings) {
		std::string line = ChildText(panel, heading);
		const std::string text = ChildText(panel, value);
		if (text.empty()) continue;
		Join(line, text);
		lines.push_back(std::move(line));
	}
	for (const uintptr_t row : NativeUi::ListRows(NativeWidgets::FindChild(panel, List))) {
		if (!NativeUi::Visible(row)) continue;
		std::string line;
		for (const auto part : RowParts) Join(line, ChildText(row, part));
		if (!line.empty()) lines.push_back(std::move(line));
	}
	return lines;
}

bool Tick(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	const uintptr_t panel = focus.panel && NativeWidgets::ReadName(focus.panel) == Panel
		? focus.panel : NativeWidgets::FindPanel(Panel);
	if (!panel || !NativeUi::Visible(panel)) { Reset(); return false; }
	if (panel != g_panel) { Reset(); g_panel = panel; }
	// Read the panel only once a push is pending: the walk costs a tree traversal.
	using InputDirection::StickPush;
	StickPush push = StickPush::None;
	for (const auto wanted : {StickPush::Up, StickPush::Down, StickPush::Left, StickPush::Right})
		if (InputDirection::TakeRightStickPushIf(wanted, "party")) { push = wanted; break; }
	if (push == StickPush::None) return false;
	const auto lines = Lines(panel);
	if (!g_logged) {
		g_logged = true;
		AsyncLog::InfoF(context, "[party] %zu lines, first \"%s\"", lines.size(), lines.empty() ? "" : lines.front().c_str());
	}
	g_cursor = CharacterStatsReader::Advance(g_cursor, push, static_cast<int>(lines.size()));
	const std::string text = g_cursor < 0 ? Strings::Current().detailNone : lines[static_cast<size_t>(g_cursor)];
	AsyncLog::InfoF(context, "[party] %d of %zu -> \"%s\"", g_cursor + 1, lines.size(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

void Reset() {
	g_panel = 0;
	g_cursor = -1;
	g_logged = false;
}

}
