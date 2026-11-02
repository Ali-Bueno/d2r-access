#include "character_stats_reader.h"

#include "async_log.h"
#include "character_stats.h"
#include "localization.h"
#include "native_hit_chance.h"
#include "native_memory.h"
#include "native_skill_navigation.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <span>
#include <string>
#include <string_view>

namespace D2RAccess::CharacterStatsReader {
namespace {
constexpr std::string_view CharacterPanel = "CharacterStatsPanel";
constexpr std::string_view HirelingPanel = "HirelingInventoryPanel";
constexpr std::string_view StrengthButton = "StrengthPointButton";
constexpr std::string_view SkillsPanel = "SkillsTreePanel";
constexpr std::string_view PlayerInventory = "PlayerInventoryExpansionLayout";
// The game's "%d ... Remaining" lines (ui.json 26401, 26399), said with a zero.
constexpr std::string_view NoStatPointsKey = "CharacterSheetStatPointsRemainingPlural";
constexpr std::string_view NoSkillChoicesKey = "SkillTreeSkillChoicesRemainingPlural";

std::atomic<bool> g_focusPoints { true };
std::atomic<bool> g_zeroPoints { true };
// Policy: the points text renders with the values, well within PointsWaitMs. The
// Strength button's announcement carries them (PointsFor), re-announced too
// within PointsWaitMs; alone only if no announcement came by then.
constexpr uint64_t PointsWaitMs = 2000;

// The panel's own hover text, the hit chances the game shows over Attack Rating and Defense
// (docs/character-panel.md, "Hit-chance tooltips"): said when the game's focus or pointer brings it
// up. The object is the factory 0x14E5820's (vtable).
constexpr uintptr_t StatsPanelVtable = 0x1FDCEF0;
constexpr size_t HoverData = 0x198, HoverSize = 0x1A0, MaxHoverBytes = 512;
std::string g_hover;

uintptr_t g_panel = 0; // the panel the cursor belongs to
int g_cursor = -1;
bool g_named = false;  // the subject's name has been said for this opening
bool g_pointsDone = false; // the points left have been offered for this opening
uint64_t g_openedAt = 0, g_pointsDueAt = 0, g_pointsAttachedAt = 0;
std::string g_pendingPoints;
uintptr_t g_pointsWidget = 0; // the button whose announcement carries them

// The "+" that last had focus and its attribute's value then. The last point
// spent hides every "+" and the focus with them, so that value is watched from
// here (a tester, 2026-10-13). Policy: the game's answer renders well within
// SpentWaitMs of the focus leaving the button.
constexpr uint64_t SpentWaitMs = 2000;
std::string g_spentButton, g_spentValue;
uint64_t g_spentLeftAt = 0;

// The skill tree's opening, watched apart from the character panel's.
uintptr_t g_skills = 0;
uint64_t g_skillsOpenedAt = 0;
bool g_skillsDone = true;

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

// Nothing to spend: the line the game shows, or its own words with a zero when
// it shows none.
void SayNoPoints(const D2RL::PluginContext* context, std::string_view key, const std::string& shown) {
	if (!g_zeroPoints.load()) return;
	std::string text = shown;
	if (text.empty()) {
		text = Localization::Text(key);
		if (const auto at = text.find("%d"); at != std::string::npos) text.replace(at, 2, "0");
		else text.clear();
	}
	D2RAccess::AsyncLog::InfoF(context, "[stats] nothing to spend, shown \"%s\" -> \"%s\"", shown.c_str(), text.c_str());
	if (!text.empty()) ScreenReader::Speak(text, false);
}

// Once per opening: the focus onto the first "+" button, then the points left.
// A line still missing after PointsWaitMs is a sheet with nothing to spend.
void OfferPoints(const D2RL::PluginContext* context, uintptr_t panel, uint64_t now) {
	const auto points = CharacterStats::Points(panel);
	if (points.empty()) { // not rendered yet
		g_pointsDone = now - g_openedAt >= PointsWaitMs;
		if (g_pointsDone) SayNoPoints(context, NoStatPointsKey, points);
		return;
	}
	g_pointsDone = true;
	if (!CharacterStats::HasPoints(points)) {
		SayNoPoints(context, NoStatPointsKey, points);
		return;
	}
	D2RAccess::AsyncLog::InfoF(context, "[stats] points available -> \"%s\"", points.c_str());
	const auto button = g_focusPoints.load() ? NativeWidgets::FindChild(panel, StrengthButton) : 0;
	if (button && NativeSkillNavigation::Select({panel, button})) {
		g_pendingPoints = points;
		g_pointsWidget = button;
		g_pointsAttachedAt = 0;
		g_pointsDueAt = now + PointsWaitMs;
		return;
	}
	if (g_focusPoints.load()) D2RAccess::AsyncLog::InfoF(context, "[stats] could not focus %s", std::string(StrengthButton).c_str());
	ScreenReader::Speak(points, false);
}

void SayHover(const D2RL::PluginContext* context, uintptr_t panel) {
	uintptr_t vtable = 0, data = 0;
	uint64_t size = 0;
	if (!NativeMemory::Peek(panel, vtable) || vtable != context->exeBase + StatsPanelVtable || !NativeMemory::Peek(panel + HoverData, data)
		|| !NativeMemory::Peek(panel + HoverSize, size) || size > MaxHoverBytes) return;
	std::string text(static_cast<size_t>(size), '\0');
	if (size && (!data || !NativeMemory::Copy(data, text.data(), text.size()))) return;
	if (text == g_hover) return;
	g_hover = text;
	const auto spoken = CharacterStats::HoverWords(text);
	D2RAccess::AsyncLog::InfoF(context, "[stats] hover text \"%s\"", spoken.c_str());
	if (!spoken.empty()) ScreenReader::Speak(spoken, false);
}

// The game's own hit chance over a row: the stick reads the rows without moving the cursor that
// shows it (a tester, 2026-10-25).
std::string WithChance(std::string text, CharacterStats::Chance chance) {
	using CharacterStats::Chance;
	const auto said = chance == Chance::Defense ? NativeHitChance::Defense()
		: chance == Chance::None ? std::string{} : NativeHitChance::Attack(chance == Chance::LeftAttack);
	if (said.empty()) return text;
	// Some voices make no stop at a period after a number: "602. Fallen" ran on (a tester, 2026-10-26).
	if (!text.empty()) text += std::isdigit(static_cast<unsigned char>(text.back())) ? ", " : text.ends_with('.') ? " " : ". ";
	return text + said;
}

void ForgetSpent() { g_spentButton.clear(); g_spentValue.clear(); g_spentLeftAt = 0; }

// A value that changes under a showing "+" is the interaction reader's to say;
// one that changes after its "+" went away is said here, with its label.
void WatchSpent(const D2RL::PluginContext* context, uintptr_t panel, NativeUi::Focus focus, uint64_t now) {
	if (focus.widget && NativeUi::Visible(focus.widget) && NativeUi::Classify(focus.widget).kind == UiText::Kind::StatButton) {
		g_spentButton = NativeWidgets::ReadName(focus.widget);
		g_spentValue = CharacterStats::AttributeOf(panel, g_spentButton).value;
		g_spentLeftAt = 0;
		return;
	}
	if (g_spentButton.empty()) return;
	if (!g_spentLeftAt) g_spentLeftAt = now;
	const auto attribute = CharacterStats::AttributeOf(panel, g_spentButton);
	if (!attribute.value.empty() && attribute.value != g_spentValue) {
		std::string text = attribute.label;
		if (!text.empty()) text += ". ";
		text += attribute.value;
		D2RAccess::AsyncLog::InfoF(context, "[stats] value after its button went -> \"%s\"", text.c_str());
		ScreenReader::Speak(text, true);
		ForgetSpent();
	} else if (now - g_spentLeftAt >= SpentWaitMs) ForgetSpent();
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

// The skill tree says its choices left with the branch (interaction_reader.cpp);
// a tree with none says so once per opening, as the character panel does.
void WatchSkillTree(const D2RL::PluginContext* context, uint64_t now) {
	const auto panel = VisiblePanel(SkillsPanel);
	if (panel != g_skills) {
		g_skills = panel;
		g_skillsOpenedAt = now;
		g_skillsDone = !panel;
	}
	if (g_skillsDone) return;
	const auto choices = CharacterStats::SkillChoices(panel);
	if (choices.empty() && now - g_skillsOpenedAt < PointsWaitMs) return;
	g_skillsDone = true;
	if (!CharacterStats::HasPoints(choices)) SayNoPoints(context, NoSkillChoicesKey, choices);
}

bool Tick(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	WatchSkillTree(context, GetTickCount64());
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
	// An item focused on the mercenary's sheet or in the backpack beside it (a tester,
	// 2026-10-14): down still walks the mercenary's statistics and left goes back to the
	// first, while up stays the item's tooltip and right its size.
	bool itemFocus = false;
	if (focus.widget && !onTabs) {
		if (focusedPanelName == HirelingPanel) itemFocus = true;
		else if (!panel && focusedPanelName == PlayerInventory) {
			panel = VisiblePanel(HirelingPanel);
			itemFocus = panel != 0;
		}
	}
	if (!panel || !NativeUi::Visible(panel)) {
		if (!g_spentButton.empty() && g_spentLeftAt)
			D2RAccess::AsyncLog::InfoF(context, "[stats] %s left unwatched: focus went to %s", g_spentButton.c_str(), focusedPanelName.c_str());
		Reset();
		return false;
	}
	const uint64_t now = GetTickCount64();
	if (panel != g_panel) {
		g_panel = panel; g_cursor = -1; g_named = false;
		g_pointsDone = NativeWidgets::ReadName(panel) != CharacterPanel; // a mercenary has no points to spend
		g_openedAt = now; g_pointsDueAt = 0; g_pendingPoints.clear(); g_pointsWidget = 0; g_pointsAttachedAt = 0;
		ForgetSpent();
	}
	// The window announcer has already said the panel's title; the name
	// queues behind it. It is read until the game has rendered it -- MEASURED
	// 2026-09-21, the first opening of a session shows the labels a moment
	// before the values.
	if (!g_named && !itemFocus) {
		if (const auto name = CharacterStats::Name(panel); !name.empty()) {
			g_named = true;
			D2RAccess::AsyncLog::InfoF(context, "[stats] name -> \"%s\"", name.c_str());
			ScreenReader::Speak(name, false);
		}
	}
	if (g_named && !g_pointsDone) OfferPoints(context, panel, now);
	if (g_pointsDueAt && now >= g_pointsDueAt) {
		g_pointsDueAt = 0;
		D2RAccess::AsyncLog::InfoF(context, "[stats] points alone, no button announcement -> \"%s\"", g_pendingPoints.c_str());
		ScreenReader::Speak(g_pendingPoints, false);
		g_pendingPoints.clear();
	}
	if (NativeWidgets::ReadName(panel) == CharacterPanel) {
		WatchSpent(context, panel, focus, now);
		SayHover(context, panel);
	}
	// A "+" with focus keeps the stick on the statistics too (a tester,
	// 2026-10-13: the level-up sheet read controls instead); right still ends on
	// the points left, CombinedPointsInfo being the layouts' last row.
	// Read the panel only once a push is pending: the walk costs a tree
	// traversal, and the stick is idle for all but a few of these ticks.
	using InputDirection::StickPush;
	constexpr std::array AllPushes{StickPush::Up, StickPush::Down, StickPush::Left, StickPush::Right};
	constexpr std::array ItemPushes{StickPush::Down, StickPush::Left};
	StickPush push = StickPush::None;
	for (const auto wanted : itemFocus ? std::span<const StickPush>(ItemPushes) : std::span<const StickPush>(AllPushes)) {
		if (InputDirection::TakeRightStickPushIf(wanted, "stats")) { push = wanted; break; }
	}
	if (push == StickPush::None) return false;
	const auto lines = CharacterStats::Rows(panel);
	g_cursor = Advance(g_cursor, push, static_cast<int>(lines.size()));
	const std::string text = g_cursor < 0 ? Strings::Current().detailNone
		: WithChance(lines[static_cast<size_t>(g_cursor)].text, lines[static_cast<size_t>(g_cursor)].chance);
	D2RAccess::AsyncLog::InfoF(context, "[stats] %d of %zu -> \"%s\"", g_cursor + 1, lines.size(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

bool Owns(std::string_view panel) { return panel == CharacterPanel || panel == HirelingPanel; }

void Reset() {
	g_panel = 0; g_cursor = -1; g_named = false; g_pointsDone = false; g_pointsDueAt = 0; g_pendingPoints.clear();
	g_pointsWidget = 0; g_pointsAttachedAt = 0;
	ForgetSpent();
}

std::string PointsFor(uintptr_t widget, uint64_t now) {
	if (!widget || widget != g_pointsWidget || g_pendingPoints.empty()) return {};
	if (!g_pointsAttachedAt) g_pointsAttachedAt = now;
	else if (now - g_pointsAttachedAt >= PointsWaitMs) { g_pendingPoints.clear(); return {}; }
	g_pointsDueAt = 0; // said with the button, never alone as well
	return g_pendingPoints;
}

bool FocusPoints() { return g_focusPoints.load(); }
bool ZeroPoints() { return g_zeroPoints.load(); }
void SetZeroPoints(bool enabled) { g_zeroPoints.store(enabled); }
void SetFocusPoints(bool enabled) { g_focusPoints.store(enabled); }

}
