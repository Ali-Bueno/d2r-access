#include "character_stats_reader.h"

#include "async_log.h"
#include "character_stats.h"
#include "native_skill_navigation.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <string_view>

namespace D2RAccess::CharacterStatsReader {
namespace {
constexpr std::string_view CharacterPanel = "CharacterStatsPanel";
constexpr std::string_view HirelingPanel = "HirelingInventoryPanel";
constexpr std::string_view StrengthButton = "StrengthPointButton";

std::atomic<bool> g_focusPoints { true };
// Policy: the points text renders with the values, well within PointsWaitMs; the
// Strength button's own announcement interrupts, so the points wait to follow it.
constexpr uint64_t PointsWaitMs = 2000, PointsAfterFocusMs = 600;

uintptr_t g_panel = 0; // the panel the cursor belongs to
int g_cursor = -1;
bool g_named = false;  // the subject's name has been said for this opening
bool g_pointsDone = false; // the points left have been offered for this opening
uint64_t g_openedAt = 0, g_pointsDueAt = 0;
std::string g_pendingPoints;

uintptr_t VisiblePanel(std::string_view name) {
	const auto panel = NativeWidgets::FindPanel(name);
	return panel && NativeUi::Visible(panel) ? panel : 0;
}

// The menu switcher's tab strip sits above every panel and has no stick
// gesture; the left stick lands there from the top row (log, 2026-09-24).
bool OnTabStrip(NativeUi::Focus focus) {
	const auto kind = NativeUi::Classify(focus.widget).kind;
	return kind == UiText::Kind::Tabs || kind == UiText::Kind::TabButton;
}

// Once per opening: the focus onto the first "+" button, then the points left.
void OfferPoints(const D2RL::PluginContext* context, uintptr_t panel, uint64_t now) {
	const auto points = CharacterStats::Points(panel);
	if (points.empty()) { // not rendered yet
		g_pointsDone = now - g_openedAt >= PointsWaitMs;
		return;
	}
	g_pointsDone = true;
	if (!CharacterStats::HasPoints(points)) return;
	D2RAccess::AsyncLog::InfoF(context, "[stats] points available -> \"%s\"", points.c_str());
	const auto button = g_focusPoints.load() ? NativeWidgets::FindChild(panel, StrengthButton) : 0;
	if (button && NativeSkillNavigation::Select({panel, button})) {
		g_pendingPoints = points;
		g_pointsDueAt = now + PointsAfterFocusMs;
		return;
	}
	if (g_focusPoints.load()) D2RAccess::AsyncLog::InfoF(context, "[stats] could not focus %s", std::string(StrengthButton).c_str());
	ScreenReader::Speak(points, false);
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
	const bool onTabs = focus.widget && OnTabStrip(focus);
	uintptr_t panel = 0;
	if (focusedPanelName == CharacterPanel || focusedPanelName == HirelingPanel) panel = focus.panel;
	else if (!focus.widget || onTabs) {
		panel = VisiblePanel(CharacterPanel);
		if (!panel) panel = VisiblePanel(HirelingPanel);
	}
	if (!panel || !NativeUi::Visible(panel)) { Reset(); return false; }
	// A focused hireling equipment slot keeps the stick's normal item-detail
	// and navigation behavior. The stat cursor is for the observed no-focus
	// controller state of this panel.
	if (NativeWidgets::ReadName(panel) == HirelingPanel && focus.panel == panel && focus.widget && !onTabs) return false;
	const uint64_t now = GetTickCount64();
	if (panel != g_panel) {
		g_panel = panel; g_cursor = -1; g_named = false;
		g_pointsDone = NativeWidgets::ReadName(panel) != CharacterPanel; // a mercenary has no points to spend
		g_openedAt = now; g_pointsDueAt = 0; g_pendingPoints.clear();
	}
	// The window announcer has already said the panel's title; the name
	// queues behind it. It is read until the game has rendered it -- MEASURED
	// 2026-09-21, the first opening of a session shows the labels a moment
	// before the values.
	if (!g_named) {
		if (const auto name = CharacterStats::Name(panel); !name.empty()) {
			g_named = true;
			D2RAccess::AsyncLog::InfoF(context, "[stats] name -> \"%s\"", name.c_str());
			ScreenReader::Speak(name, false);
		}
	}
	if (g_named && !g_pointsDone) OfferPoints(context, panel, now);
	if (g_pointsDueAt && now >= g_pointsDueAt) {
		g_pointsDueAt = 0;
		ScreenReader::Speak(g_pendingPoints, false);
	}
	// While a point button has focus the stick belongs to the point readout --
	// up for the stat's explanation, down for its controls -- except right,
	// which reads the points left to spend (the user, 2026-09-21: "right stick
	// right can be used to read remaining stat points"). BACK stays the game's
	// View Advanced Stats.
	if (focus.widget && NativeUi::Classify(focus.widget).kind == UiText::Kind::StatButton) {
		if (!InputDirection::TakeRightStickPushIf(InputDirection::StickPush::Right, "stats")) return false;
		const auto points = CharacterStats::Points(panel);
		D2RAccess::AsyncLog::InfoF(context, "[stats] points -> \"%s\"", points.c_str());
		ScreenReader::Speak(points.empty() ? Strings::Current().detailNone : points, true);
		return true;
	}
	// Read the panel only once a push is pending: the walk costs a tree
	// traversal, and the stick is idle for all but a few of these ticks.
	InputDirection::StickPush push = InputDirection::StickPush::None;
	for (const auto wanted : {InputDirection::StickPush::Up, InputDirection::StickPush::Down,
	                          InputDirection::StickPush::Left, InputDirection::StickPush::Right}) {
		if (InputDirection::TakeRightStickPushIf(wanted, "stats")) { push = wanted; break; }
	}
	if (push == InputDirection::StickPush::None) return false;
	const auto lines = CharacterStats::Lines(panel);
	g_cursor = Advance(g_cursor, push, static_cast<int>(lines.size()));
	const std::string text = g_cursor < 0 ? Strings::Current().detailNone : lines[static_cast<size_t>(g_cursor)];
	D2RAccess::AsyncLog::InfoF(context, "[stats] %d of %zu -> \"%s\"", g_cursor + 1, lines.size(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

bool Owns(std::string_view panel) { return panel == CharacterPanel || panel == HirelingPanel; }

void Reset() { g_panel = 0; g_cursor = -1; g_named = false; g_pointsDone = false; g_pointsDueAt = 0; g_pendingPoints.clear(); }

bool FocusPoints() { return g_focusPoints.load(); }
void SetFocusPoints(bool enabled) { g_focusPoints.store(enabled); }

}
