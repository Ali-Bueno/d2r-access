#include "character_stats_reader.h"

#include "async_log.h"
#include "character_stats.h"
#include "localization.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace D2RAccess::CharacterStatsReader {
namespace {
constexpr std::string_view CharacterPanel = "CharacterStatsPanel";
constexpr std::string_view HirelingPanel = "HirelingInventoryPanel";
constexpr std::string_view AdvancedPanel = "AdvancedStatsPanel";
constexpr std::string_view AdvancedScroll = "ScrollBar";

uintptr_t g_panel = 0; // the panel the cursor belongs to
int g_cursor = -1;
bool g_named = false;  // the subject's name has been said for this opening
std::vector<uintptr_t> g_advancedRowIds;

void Append(std::string& text, std::string_view part) {
	if (part.empty()) return;
	if (!text.empty()) text += ". ";
	text += part;
}

uintptr_t VisiblePanel(std::string_view name) {
	const auto panel = NativeWidgets::FindPanel(name);
	return panel && NativeUi::Visible(panel) ? panel : 0;
}
}

int Advance(int cursor, InputDirection::StickPush push, int count) {
	if (count <= 0) return -1;
	switch (push) {
	case InputDirection::StickPush::Up:    return std::max(cursor - 1, 0);
	case InputDirection::StickPush::Down:  return std::min(cursor + 1, count - 1);
	case InputDirection::StickPush::Left:  return 0;
	case InputDirection::StickPush::Right: return count - 1;
	default:                               return cursor;
	}
}

bool Tick(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	// MEASURED 2026-09-20: the focus manager reports no focused widget at all
	// while this panel is up -- "[focus] no focused widget while
	// CharacterStatsPanel is active", with the focus indicator matching no
	// widget either. Waiting for focus meant the stick fell through to the
	// level scanner for the whole session. The panel being open and visible is
	// what says the player is on it.
	const auto focusedPanelName = NativeWidgets::ReadName(focus.panel);
	uintptr_t panel = 0;
	if (focusedPanelName == CharacterPanel || focusedPanelName == HirelingPanel || focusedPanelName == AdvancedPanel) panel = focus.panel;
	else if (!focus.widget) {
		panel = VisiblePanel(AdvancedPanel);
		if (!panel) panel = VisiblePanel(CharacterPanel);
		if (!panel) panel = VisiblePanel(HirelingPanel);
	}
	if (!panel || !NativeUi::Visible(panel)) { Reset(); return false; }
	const bool advanced = NativeWidgets::ReadName(panel) == AdvancedPanel;
	if (advanced && focus.widget && !OwnsFocus(focus)) return false;
	// A focused hireling equipment slot keeps the stick's normal item-detail
	// and navigation behavior. The stat cursor is for the observed no-focus
	// controller state of this panel.
	if (NativeWidgets::ReadName(panel) == HirelingPanel && focus.panel == panel && focus.widget) return false;
	if (panel != g_panel) { g_panel = panel; g_cursor = -1; g_named = false; g_advancedRowIds.clear(); }
	if (advanced && !g_named) {
		const auto rows = CharacterStats::AdvancedRows(panel);
		if (!rows.empty()) {
			for (const auto& row : rows) g_advancedRowIds.push_back(row.id);
			g_cursor = 0;
			g_named = true;
			std::string first = Localization::Text("AdvancedStats");
			if (first.empty()) first = NativeWidgets::ReadText(NativeWidgets::FindChild(panel, "Title"));
			Append(first, CharacterStats::Name(panel));
			Append(first, rows.front().text);
			AsyncLog::InfoF(context, "[stats] first -> \"%s\"", first.c_str());
			ScreenReader::Speak(first, true);
			return true;
		}
		return true;
	}
	// The window announcer has already said the panel's title; the name
	// queues behind it. It is read until the game has rendered it -- MEASURED
	// 2026-09-21, the first opening of a session shows the labels a moment
	// before the values.
	if (!advanced && !g_named) {
		if (const auto name = CharacterStats::Name(panel); !name.empty()) {
			g_named = true;
			D2RAccess::AsyncLog::InfoF(context, "[stats] name -> \"%s\"", name.c_str());
			ScreenReader::Speak(name, false);
		}
	}
	// Read the panel only once a push is pending: the walk costs a tree
	// traversal, and the stick is idle for all but a few of these ticks.
	InputDirection::StickPush push = InputDirection::StickPush::None;
	for (const auto wanted : {InputDirection::StickPush::Up, InputDirection::StickPush::Down,
	                          InputDirection::StickPush::Left, InputDirection::StickPush::Right}) {
		if (InputDirection::TakeRightStickPushIf(wanted, "stats")) { push = wanted; break; }
	}
	if (push == InputDirection::StickPush::None) return advanced;
	std::vector<std::string> lines;
	if (advanced) {
		const auto rows = CharacterStats::AdvancedRows(panel);
		std::vector<uintptr_t> ids;
		for (const auto& row : rows) { ids.push_back(row.id); lines.push_back(row.text); }
		if (ids != g_advancedRowIds) { g_cursor = -1; g_advancedRowIds = std::move(ids); }
	} else lines = CharacterStats::Lines(panel);
	g_cursor = Advance(g_cursor, push, static_cast<int>(lines.size()));
	const std::string text = g_cursor < 0 ? Strings::Current().detailNone : lines[static_cast<size_t>(g_cursor)];
	D2RAccess::AsyncLog::InfoF(context, "[stats] %d of %zu -> \"%s\"", g_cursor + 1, lines.size(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

bool OwnsFocus(NativeUi::Focus focus) {
	return NativeWidgets::ReadName(focus.panel) == AdvancedPanel
		&& NativeWidgets::ReadName(focus.widget) == AdvancedScroll;
}

bool AnnouncesOpening(std::string_view panel) { return panel == AdvancedPanel; }

bool Owns(std::string_view panel) { return panel == CharacterPanel || panel == HirelingPanel || panel == AdvancedPanel; }

void Reset() { g_panel = 0; g_cursor = -1; g_named = false; g_advancedRowIds.clear(); }

}
