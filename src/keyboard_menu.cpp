#include "keyboard_menu.h"

#include "async_log.h"
#include "click_words.h"
#include "game_text_reader.h"
#include "hire_stats.h"
#include "input_direction.h"
#include "inventory_cursor.h"
#include "keyboard_service.h"
#include "keyboard_menu_policy.h"
#include "menu_cursor.h"
#include "menu_mouse.h"
#include "native_belt_fill.h"
#include "native_belt_rows.h"
#include "native_class_choice.h"
#include "localization.h"
#include "native_ground_items.h"
#include "native_list_rows.h"
#include "native_memory.h"
#include "native_skill_hand.h"
#include "native_skill_icons.h"
#include "native_skill_navigation.h"
#include "native_sort.h"
#include "native_tooltips.h"
#include "native_ui.h"
#include "native_value_controls.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tooltip_text.h"
#include "ui_text.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace D2RAccess::KeyboardMenu {
namespace {
using GridJump::Cell;
using GridJump::Direction;
using MenuCursor::Place;
using MenuCursor::Target;
using UiText::Kind;

// Policy, not game constants.
constexpr uint64_t ControlTooltipSettleMs = 90; // existing guard for controls without an item's identity check
constexpr uint64_t RefreshMs = 250;     // how often the open menu is looked at again
constexpr uint64_t AfterClickMs = 250;  // from a click to a look at what it changed
constexpr uint64_t RepeatDelayMs = 500; // a held arrow steps again after this long
constexpr uint64_t RepeatMs = 180;      // and then this often
constexpr uint64_t SilenceMs = 450;     // a place no reader has named by then is named here
constexpr uint64_t GraceMs = 60;        // how long a key seen directly waits for the loader to deliver it
constexpr uint64_t DeliveredMs = 150;   // a delivery this near a key seen directly is the same press
constexpr uint64_t FillGapMs = 500;     // a plan sent again before the server answered is one it refuses
constexpr uint64_t ClickGapMs = 200;    // a held key repeats its press; a click is not repeated
constexpr uint64_t TitleMs = 1200;      // what a menu is called is heard before the cursor names its first place
constexpr uint64_t OwnStepMs = 120;     // by then the game has stepped a value on the arrow key itself, if it does
constexpr uint64_t ChoiceMs = 3000;     // a class clicked that is not chosen by then was not chosen
constexpr uint64_t RowPairMs = 400;     // a click on a row this soon after another is the second of the two that act on it
constexpr size_t MaxEntries = 256;
constexpr int MaxDepth = 32;

// Panels that are open without being a menu, by the names the layouts give them.
constexpr std::string_view NotMenus[] = {
	"TooltipsPanel", "PanelBordersPanel", "BlockingPanel", "MiniMenuPanel", "HelpButtonPanel", "AutoMap", "ChatPanel",
	"FocusManager", "GlobalData", "SaveIndicatorPanel", "NotificationPanel", "LoadScreenPanel", "MainMenuLoadingPanel",
	"LobbyBackgroundPanel", "DismissibleTooltipPanel", "TutorialTooltip", "EmoteWheelPanel", "ClickCatcher",
	"InGameMoviePlayerPanel", "MoviePlayerPanel", "ControllerOverlay", "UISwitcher", "EndGameTimerPanel",
	// MEASURED 2026-09-27, open through a whole game: the mercenary's portrait,
	// the list of an input method's candidates, the names of items on the ground.
	"HireablesPanel", "CandidateListPanel", "ShowItemsPanel",
};
// The message log, by the start of its name: it has one layout for each size of the old screen.
constexpr std::string_view MessageLog = "MessageLogPanel";
constexpr std::string_view Hud = "HUDPanel";
// The HUD's two skill buttons. Between them the HUD has nothing of its own.
constexpr std::string_view LeftSkill = "LeftSkillButton", RightSkill = "RightSkillButton";
// The list of skills, one panel for both hands, and the game's own words for
// each hand: the lines of its help screen for the two mouse buttons.
constexpr std::string_view SkillList = "SkillSelect";
// The rows of an open dropdown, a panel of its own drawn over the menu it belongs to.
constexpr std::string_view DropdownList = "DropdownListContents";
// The list of mercenaries to hire, and the NPC's dialogue it is opened from.
constexpr std::string_view HirePanel = "HireMenuPanel", NpcDialog = "NpcDialogPanel";
constexpr const char* LeftHandKey = "StrHDHelpLeftMouse";
constexpr const char* RightHandKey = "StrHDHelpRightMouse";

// The menus Home and End go through, each with its button of the mini menu
// and the start of the name of the panel it opens.
struct Menu { std::string_view button, panel; };
constexpr Menu Menus[] = {
	{"MiniMenuCharacter", "CharacterStatsPanel"}, {"MiniMenuInventory", "PlayerInventory"},
	{"MiniMenuSkillTree", "SkillsTreePanel"}, {"MiniMenuQuestLog", "QuestLogPanel"}, {"MiniMenuPauseMenu", "PauseLayout"},
};
constexpr std::string_view ReturnToGame = "ReturnToGame";

struct Entry {
	uintptr_t widget = 0, panel = 0;
	uintptr_t tab = 0;  // the tab it is drawn inside, when it is inside one
	Kind kind = Kind::Other;
	bool cells = false; // several places: a grid of items, the belt, a bar of tabs
	bool belt = false;  // the belt, which counts its places from the bottom row up
	std::string name, type;
	NativeUi::Rect point{}; // where the pointer rests when that is not the middle of the control; no size when it is
};
// A step of a value that is under way. The game itself steps a slider or an
// option the pointer is over when the left or right arrow key goes down (READ,
// not measured), so the value is looked at first, and the arrow of the control
// is clicked only when the key has not changed it.
struct Adjusting {
	uintptr_t widget = 0;
	int before = 0;
	bool up = false, clicked = false;
	uint64_t since = 0, look = 0;
};
Adjusting g_adjusting;
// A class that was clicked, until it is seen to be chosen or the wait is over.
struct Choosing { uintptr_t widget = 0; uint64_t since = 0; };
Choosing g_choosing;

// The open menu: what each thing is, and beside it where it is for the cursor.
std::vector<Entry> g_entries;
std::vector<Target> g_targets;
// The steps made for what the arrows alone would miss, and the menu they were made for.
std::vector<MenuCursor::Link> g_links;
std::vector<Target> g_mended;
std::string g_menu;         // the open panels by name, to tell one menu from the next
Place g_place;
std::atomic<bool> g_active{false};
std::atomic<bool> g_typing{false}; // the cursor is on a box of text, which has the keys that type
bool g_engaged = false;     // the cursor is the keyboard's; false while a pad has the menu
bool g_holding = false;     // an item was in hand when the pointer was last placed
bool g_landed = false;      // the pointer went to where the cursor is
NativeUi::Cell g_pointerAt; // the point the pointer was last put on, and went to
uint64_t g_refreshAt = 0;
uint64_t g_refreshedAt = UINT64_MAX; // one menu snapshot serves every action in this UI tick
size_t g_things = 0;        // how many things the menu had when they were last listed in the log
uint64_t g_thingsLogged = 0;
uint64_t g_landedAt = 0;    // when the cursor came to where it is, 0 once that was named
std::string g_toSay;        // what to say if no reader does
std::string g_said;         // what the cursor said of where it is, when no reader did
bool g_beltShown = false;   // the game's "show belt" was turned on from here, and is to be turned off
int g_beltRows = 0;         // the rows of the belt when the menu was last read, 0 with no belt in it
uint64_t g_lastClick = 0;
uint64_t g_lastFill = 0;
// The last click that went to a row of a list that tells choosing from acting.
struct RowClick { uintptr_t widget = 0; uint64_t at = 0; };
RowClick g_rowClicked;
uint64_t g_titleUntil = 0;  // until then the cursor waits to land: what the menu is called is being said
NativeSkillHand::Hand g_listHand = NativeSkillHand::Hand::None; // the hand the list of skills was last said to be for

// Diagnostics: what reading the menu again costs, by part (the user,
// 2026-09-30: "slight lag when navigating any inventory screen"; HANDOFF).
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point from, Clock::time_point to) { return std::chrono::duration<double, std::milli>(to - from).count(); }
struct BuildCost { double walk = 0, grids = 0, thin = 0, mend = 0; int cells = 0; };
BuildCost g_cost;
Clock::time_point g_tickStart;     // when this tick of the menu began
bool g_logBuild = false;           // the next build is a key's, and is logged whatever it costs
uint64_t g_buildLogged = 0;        // when a build was last logged for being slow
constexpr double SlowBuildMs = 10; // a build this slow is logged, at most once a second

struct Arrow { int key; Direction direction; InputDirection::StickPush push; bool down = false; uint64_t next = 0; };
Arrow g_arrows[] = {
	{VK_LEFT, Direction::Left, InputDirection::StickPush::Left}, {VK_RIGHT, Direction::Right, InputDirection::StickPush::Right},
	{VK_UP, Direction::Up, InputDirection::StickPush::Up}, {VK_DOWN, Direction::Down, InputDirection::StickPush::Down},
};
// The keys of the menu, watched directly beside the loader's delivery of them,
// and what may be held with each for it still to be the menu's key.
enum class With { Nothing, Shift, ClickModifiers };
struct Watched { int key; Act act; With with; bool down = false, waiting = false, logged = false; uint64_t since = 0; };
Watched g_watched[] = {
	{'A', Act::ClickLeft, With::ClickModifiers}, {'D', Act::ClickRight, With::ClickModifiers}, {VK_PRIOR, Act::TabPrevious, With::Nothing},
	{VK_NEXT, Act::TabNext, With::Nothing}, {VK_HOME, Act::MenuPrevious, With::Nothing}, {VK_END, Act::MenuNext, With::Nothing},
	{VK_OEM_5, Act::SayAgain, With::Nothing}, {'J', Act::Detail, With::Nothing},
};
constexpr size_t Acts = 8;
constexpr const char* ActNames[Acts] = {"left click", "right click", "previous tab", "next tab", "previous menu", "next menu",
	"say again", "description"};
std::atomic<uint64_t> g_delivered[Acts]{};
std::atomic<uint32_t> g_pending{0};

bool Down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }

// Character creation's name box takes what is typed wherever the cursor is
// (READ), so A typed an "a" as it clicked a class. What the game would type is
// kept from it unless the cursor is on the box (the user, 2026-09-28).
std::atomic<bool> g_keepLetters{false};
// Character select moves its own selection with up and down too, so the cursor
// walking the buttons beside the list changed the character (a tester, 2026-10-17).
std::atomic<bool> g_keepArrows{false};
HHOOK g_letterHook = nullptr;
bool g_letterTried = false;
const D2RL::PluginContext* g_letterContext = nullptr;

LRESULT CALLBACK KeepLetters(int code, WPARAM wParam, LPARAM lParam) {
	if (code == HC_ACTION && wParam == PM_REMOVE && g_keepLetters.load()) {
		auto* message = reinterpret_cast<MSG*>(lParam);
		// Printable characters only: Enter, Esc and Backspace stay the game's.
		if (message->message == WM_CHAR && message->wParam >= 0x20 && message->wParam != 0x7F) {
			AsyncLog::InfoF(g_letterContext, "[menu-keys] kept '%c' from the name box: the cursor is not on it",
				message->wParam < 0x80 ? static_cast<char>(message->wParam) : '?');
			message->message = WM_NULL;
		}
	}
	if (code == HC_ACTION && wParam == PM_REMOVE && g_keepArrows.load()) {
		auto* message = reinterpret_cast<MSG*>(lParam);
		if ((message->message == WM_KEYDOWN || message->message == WM_KEYUP)
			&& (message->wParam == VK_UP || message->wParam == VK_DOWN))
			message->message = WM_NULL;
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}

// On the thread of the game's window, which is in front when this is called.
void HookLetters(const D2RL::PluginContext* context) {
	if (g_letterHook || g_letterTried) return;
	DWORD process = 0;
	const DWORD thread = GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (process != GetCurrentProcessId() || !thread) return;
	g_letterTried = true;
	g_letterContext = context;
	g_letterHook = SetWindowsHookExW(WH_GETMESSAGE, KeepLetters, nullptr, thread);
	if (g_letterHook) AsyncLog::InfoF(context, "[menu-keys] typed letters can be kept from a box of text (thread %lu)", thread);
	else AsyncLog::WarnF(context, "[menu-keys] typed letters cannot be kept from a box of text: error %lu", GetLastError());
}

bool Foreground() {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return process == GetCurrentProcessId();
}
uint64_t Pack(Cell cell) {
	return static_cast<uint32_t>(cell.x) | (static_cast<uint64_t>(static_cast<uint32_t>(cell.y)) << 32);
}
MenuCursor::Rect ToRect(const NativeUi::Rect& rect) { return {rect.x, rect.y, rect.width, rect.height}; }
NativeUi::Rect Union(const NativeUi::Rect& a, const NativeUi::Rect& b) {
	const int left = std::min(a.x, b.x), top = std::min(a.y, b.y);
	const int right = std::max(a.x + a.width, b.x + b.width), bottom = std::max(a.y + a.height, b.y + b.height);
	return {left, top, right - left, bottom - top};
}
bool Contains(const MenuCursor::Rect& outer, const MenuCursor::Rect& inner) {
	const int x = inner.x + inner.width / 2, y = inner.y + inner.height / 2;
	return x >= outer.x && y >= outer.y && x < outer.x + outer.width && y < outer.y + outer.height;
}

// A place drawn off the screen cannot be pointed at. MEASURED 2026-09-27: the
// main menu's bar of tabs is drawn at y = -130 when there is one tab to show,
// the pointer stayed where it was, and the click went to what was under it.
bool OnScreen(const NativeUi::Rect& rect) {
	return rect.x + rect.width / 2 >= 0 && rect.y + rect.height / 2 >= 0;
}

bool NotMenu(std::string_view name) {
	return name.starts_with("HUD") || name.starts_with(MessageLog) || std::find(std::begin(NotMenus), std::end(NotMenus), name) != std::end(NotMenus);
}

GridJump::Grid ReadGrid(uintptr_t widget) {
	GridJump::Grid grid;
	const auto start = Clock::now();
	struct Timed { Clock::time_point start; ~Timed() { g_cost.grids += Ms(start, Clock::now()); } } timed{start};
	const auto size = NativeUi::GridSize(widget);
	if (size.x < 1 || size.y < 1) return grid;
	grid.width = size.x;
	grid.height = size.y;
	std::vector<uintptr_t> units;
	for (int y = 0; y < size.y; ++y) {
		for (int x = 0; x < size.x; ++x) {
			++g_cost.cells;
			const auto slot = NativeUi::GridItemAt(widget, {x, y});
			if (!slot.known || !slot.item) continue;
			const auto known = std::find(units.begin(), units.end(), slot.item);
			if (known == units.end()) {
				// Rows are read top first and cells left first, so an item is met at its top-left cell.
				units.push_back(slot.item);
				grid.items.push_back({x, y, 1, 1});
				continue;
			}
			auto& item = grid.items[static_cast<size_t>(known - units.begin())];
			item.width = std::max(item.width, x - item.x + 1);
			item.height = std::max(item.height, y - item.y + 1);
		}
	}
	return grid;
}

// A place of several as the game counts it. The belt counts its slots from the
// bottom row up, four to a row (native_belt_rows.h); a grid of items and a bar
// of tabs count from the top left, as the cursor does.
uint64_t Selection(const Entry& entry, Cell cell, int rows) {
	return entry.belt ? Pack({cell.x + NativeBeltRows::Columns * (rows - 1 - cell.y), 0}) : Pack(cell);
}

// A widget of several places, by the rectangles of its first and its last.
bool AddCells(Entry entry, GridJump::Grid grid) {
	if (grid.width < 1 || grid.height < 1) return false;
	const auto first = NativeSkillNavigation::RectAt(entry.widget, Selection(entry, {0, 0}, grid.height));
	const auto last = NativeSkillNavigation::RectAt(entry.widget, Selection(entry, {grid.width - 1, grid.height - 1}, grid.height));
	if (!first || !last || (grid.width * grid.height > 1 && *first == *last) || !OnScreen(*first) || !OnScreen(*last)) return false;
	entry.cells = true;
	g_targets.push_back({entry.widget, ToRect(Union(*first, *last)), std::move(grid)});
	g_entries.push_back(std::move(entry));
	return true;
}

void Collect(uintptr_t widget, uintptr_t panel, uintptr_t tab, int depth) {
	if (!widget || depth > MaxDepth || g_entries.size() >= MaxEntries || !NativeUi::Visible(widget)) return;
	Entry entry;
	entry.widget = widget;
	entry.panel = panel;
	entry.tab = tab;
	entry.name = NativeWidgets::ReadName(widget);
	// A template container of the layouts, not a part of the menu.
	if (entry.name == "Templates") return;
	const auto type = NativeUi::Classify(widget);
	entry.kind = type.kind;
	entry.type = type.name;
	bool whole = false; // nothing inside it is a place of its own
	if (type.kind == Kind::InventoryGrid) {
		whole = AddCells(entry, ReadGrid(widget));
	} else if (type.name == "BeltWidget") {
		entry.belt = true;
		// The rows above the bottom one are places while the belt is drawn in full.
		const int slots = NativeBeltRows::Expanded(widget) ? NativeBeltRows::Slots(widget) : 0;
		whole = AddCells(entry, {NativeBeltRows::Columns, slots ? slots / NativeBeltRows::Columns : 1, {}});
	} else if (type.kind == Kind::Tabs) {
		if (const int tabs = NativeUi::TabCount(widget); tabs > 1) whole = AddCells(entry, {tabs, 1, {}});
	}
	// A slider and an option that steps are one place each: their arrows are
	// what the left and right arrow keys click, not places of their own.
	const bool steps = NativeValueControls::Of(widget) != NativeValueControls::Kind::None;
	const bool control = type.kind == Kind::Button || type.kind == Kind::Toggle || type.kind == Kind::ListItem
		|| type.kind == Kind::Focusable || type.kind == Kind::InventorySlot || type.kind == Kind::InventoryItem
		|| type.kind == Kind::Tabs || type.kind == Kind::StatButton || type.kind == Kind::TabButton
		|| type.kind == Kind::ValueControl || type.kind == Kind::QuestButton || type.kind == Kind::TextInput;
	if (!whole && control && type.name != "BeltWidget") {
		if (auto rect = NativeSkillNavigation::RectAt(widget, NativeUi::FocusSelection(widget)); rect && OnScreen(*rect)) {
			// A row of the hire list is a place, and so is each skill icon and the
			// price drawn inside it (the user, 2026-10-01: "the player should be able
			// to highlight the individual skill icons"). The row's place is its name
			// and level, the part left of its first icon: whole, it was dropped as what
			// its icons stand in (Thin; MEASURED 2026-09-30, no place for the name),
			// and the pointer in its middle was on the first icon's edge.
			if (type.kind == Kind::ListItem && NativeWidgets::ReadName(panel) == HirePanel) {
				int left = rect->x + rect->width;
				for (const uintptr_t child : NativeUi::Children(widget)) {
					if (!NativeUi::Visible(child) || NativeUi::Classify(child).name != "HirelingSkillIconWidget") continue;
					if (const auto icon = NativeSkillNavigation::RectAt(child, NativeUi::FocusSelection(child))) left = std::min(left, icon->x);
				}
				if (left - rect->x >= rect->height) rect->width = left - rect->x;
				entry.point = *rect;
			}
			// The skills of a skill tree, and of the list of skills, stand together:
			// a step from one lands on another before it lands on a tab beside them.
			const bool skill = NativeSkillIcons::InTree(widget) || NativeSkillIcons::InList(widget);
			g_targets.push_back({widget, ToRect(*rect), {}, skill ? NativeUi::Parent(widget) : 0});
			g_entries.push_back(entry);
		}
	}
	if (whole || steps) return;
	const uintptr_t inside = type.kind == Kind::TabButton ? widget : tab;
	for (const uintptr_t child : NativeUi::Children(widget)) Collect(child, panel, inside, depth + 1);
}

// A control drawn around others is where they stand, not one of them, and a
// control drawn exactly over another is the same place twice.
void Thin() {
	std::vector<bool> drop(g_targets.size(), false);
	for (size_t i = 0; i < g_targets.size(); ++i) {
		if (g_entries[i].cells) continue;
		int holds = 0;
		for (size_t j = 0; j < g_targets.size(); ++j) {
			if (i == j || drop[j]) continue;
			if (j < i && !g_entries[j].cells && g_targets[j].rect == g_targets[i].rect) { drop[i] = true; break; }
			if (g_targets[j].rect != g_targets[i].rect && Contains(g_targets[i].rect, g_targets[j].rect)
				&& (g_entries[j].cells || g_targets[j].rect.width * g_targets[j].rect.height < g_targets[i].rect.width * g_targets[i].rect.height)) ++holds;
		}
		if (holds >= 2) drop[i] = true;
	}
	size_t kept = 0;
	for (size_t i = 0; i < g_targets.size(); ++i) {
		if (drop[i]) continue;
		if (kept != i) { g_targets[kept] = std::move(g_targets[i]); g_entries[kept] = std::move(g_entries[i]); }
		++kept;
	}
	g_targets.resize(kept);
	g_entries.resize(kept);
}

bool Same(const std::vector<Target>& first, const std::vector<Target>& second) {
	return std::equal(first.begin(), first.end(), second.begin(), second.end(), [](const Target& a, const Target& b) {
		return a.widget == b.widget && a.rect == b.rect && a.group == b.group && a.grid.width == b.grid.width && a.grid.height == b.grid.height
			&& std::equal(a.grid.items.begin(), a.grid.items.end(), b.grid.items.begin(), b.grid.items.end(),
				[](const GridJump::Item& one, const GridJump::Item& other) {
					return one.x == other.x && one.y == other.y && one.width == other.width && one.height == other.height;
				});
	});
}

uintptr_t FindNamed(uintptr_t widget, std::string_view name, bool byType, int depth) {
	if (!widget || depth > MaxDepth || !NativeUi::Visible(widget)) return 0;
	if ((byType ? NativeUi::Classify(widget).name : NativeWidgets::ReadName(widget)) == name) return widget;
	for (const uintptr_t child : NativeUi::Children(widget))
		if (const uintptr_t found = FindNamed(child, name, byType, depth + 1)) return found;
	return 0;
}

// The game's "show belt" is turned off again if it was turned on from here.
void HideBelt(const D2RL::PluginContext* context) {
	if (!g_beltShown) return;
	g_beltShown = false;
	if (NativeBeltRows::Shown().value_or(false)) (void)NativeBeltRows::ToggleShown(context);
}

// A belt of more than one row is drawn in full while it is in the menu, so the
// rows above the bottom one are there to be reached and clicked. The game's
// own "show belt" does it; one the player turned on is left as it is.
void ShowBelt(const D2RL::PluginContext* context, uintptr_t belt) {
	if (g_beltShown || NativeBeltRows::Slots(belt) <= NativeBeltRows::Columns) return;
	const auto shown = NativeBeltRows::Shown();
	if (!shown || *shown) return;
	g_beltShown = NativeBeltRows::ToggleShown(context) && NativeBeltRows::Shown().value_or(false);
}

// The open menu, read again. The panels that are open and shown, oldest first;
// under a modal only the modal, which takes every click until it is answered.
// Character creation: down from every class is the name box, drawn under all
// of them, or the hardcore box while there is no name box. Added to the
// nearest-thing rule, which sent four of the eight classes elsewhere (MEASURED
// 2026-09-27: the name box and the hardcore box were never landed on).
void LinkClasses() {
	const Entry* below = nullptr;
	for (const std::string_view name : {"InputText", "HardcoreCheckbox"}) {
		for (const auto& entry : g_entries)
			if (entry.name == name && NativeWidgets::ReadName(entry.panel) == "CharacterCreatePanel") { below = &entry; break; }
		if (below) break;
	}
	if (!below) return;
	for (const auto& entry : g_entries)
		if (NativeClassChoice::Is(entry.widget)) g_links.push_back({{entry.widget, {}, {}}, {below->widget, {}, {}}, Direction::Down});
}

std::string Build(const D2RL::PluginContext* context) {
	g_cost = {};
	const auto start = Clock::now();
	g_entries.clear();
	g_targets.clear();
	struct Open { uintptr_t panel; std::string name; };
	std::vector<Open> shown;
	const auto panels = NativeUi::Panels();
	for (const auto& name : PanelState::OpenPanels()) {
		if (NotMenu(name)) continue;
		for (const uintptr_t panel : panels) {
			if (NativeWidgets::ReadName(panel) != name || !NativeUi::Visible(panel)) continue;
			shown.push_back({panel, name});
			break;
		}
	}
	for (size_t i = shown.size(); i-- > 0;) {
		if (!shown[i].name.ends_with("Modal") && shown[i].name != DropdownList) continue;
		shown = {shown[i]};
		break;
	}
	std::string menu;
	bool items = false;
	size_t asked = 0; // the choices of the NPC's dialogue that are drawn
	for (const auto& open : shown) {
		// Hiring with a mercenary already in service, the NPC asks before it is
		// replaced, and the two answers are drawn over the hire list as choices of
		// its own dialogue (MEASURED 2026-09-30 12:19:28: two rows at 169,282 and
		// 169,306, 480 wide, over the row that was chosen). The list stays drawn
		// under them, but a click on it takes the question away: each time the
		// cursor went from Close to a row, its click of the row ended the question
		// (12:14:17, OpenPanel HireMenuPanel 190 ms after). While the dialogue
		// offers a choice the list is not the cursor's.
		if (open.name == HirePanel && asked) continue;
		menu += open.name;
		menu += ' ';
		items = items || open.name.find("Inventory") != std::string::npos;
		const size_t before = g_entries.size();
		Collect(open.panel, open.panel, 0, 0);
		if (open.name == NpcDialog) asked = g_entries.size() - before;
	}
	// The belt is the HUD's, and is where potions are put from the inventory.
	const uintptr_t hud = items ? NativeWidgets::FindPanel(Hud) : 0;
	if (const uintptr_t belt = hud && NativeUi::Visible(hud) ? FindNamed(hud, "BeltWidget", true, 0) : 0) {
		ShowBelt(context, belt);
		Collect(belt, hud, 0, 0);
	} else HideBelt(context);
	const auto walked = Clock::now();
	Thin();
	const auto thinned = Clock::now();
	if (!Same(g_targets, g_mended)) {
		g_links = MenuCursor::Mend(g_targets);
		LinkClasses();
		g_mended = g_targets;
	}
	g_cost.walk = Ms(start, walked);
	g_cost.thin = Ms(walked, thinned);
	g_cost.mend = Ms(thinned, Clock::now());
	return menu;
}

const Entry* EntryOf(uintptr_t widget) {
	for (const auto& entry : g_entries) if (entry.widget == widget) return &entry;
	return nullptr;
}

// A slot of the belt takes a click on the cell drawn in the top left of its
// rectangle, which holds the space between the cells as well and, drawn in
// full, the belt's cap. The belt's own hit test says which point is the slot's.
NativeUi::Rect BeltPoint(uintptr_t belt, int slot, const NativeUi::Rect& rect) {
	constexpr int Parts[][2] = {{2, 2}, {2, 3}, {3, 3}, {4, 4}};
	for (const auto& part : Parts) {
		const NativeUi::Cell point{rect.x + rect.width / part[0], rect.y + rect.height / part[1]};
		if (NativeBeltRows::SlotAt(belt, point) == slot) return {point.x - 1, point.y - 1, 2, 2};
	}
	return rect;
}

// Where the pointer goes for a place: the middle of the control, of the whole
// item, or of the cell. With an item in hand the game's own rectangle for the
// cell is where the pointer has to be for the item to begin on that cell.
std::optional<NativeUi::Rect> PointerRect(const Entry& entry, const Place& place, bool holding) {
	if (!entry.cells && entry.point.width > 0) return entry.point;
	if (!entry.cells) return NativeSkillNavigation::RectAt(entry.widget, NativeUi::FocusSelection(entry.widget));
	const auto* target = MenuCursor::Find(g_targets, entry.widget);
	const uint64_t selection = Selection(entry, place.anchor, target ? target->grid.height : 1);
	const auto first = NativeSkillNavigation::RectAt(entry.widget, selection);
	if (first && entry.belt) return BeltPoint(entry.widget, static_cast<int>(selection), *first);
	if (!first || holding) return first;
	const int item = target ? GridJump::ItemAt(target->grid, place.anchor) : GridJump::Empty;
	if (item == GridJump::Empty) return first;
	const auto& found = target->grid.items[static_cast<size_t>(item)];
	const auto last = NativeSkillNavigation::RectAt(entry.widget, Pack({found.x + found.width - 1, found.y + found.height - 1}));
	return last ? Union(*first, *last) : *first;
}

std::string TabText(uintptr_t bar, int tab) {
	return UiText::Clean(NativeWidgets::ReadText(NativeWidgets::FindChild(bar, "Text" + std::to_string(tab))));
}

// What a place is called when no reader has called it anything.
std::string Name(const Entry& entry, const Place& place) {
	if (entry.kind == Kind::InventoryGrid) {
		const auto slot = NativeUi::GridItemAt(entry.widget, {place.anchor.x, place.anchor.y});
		if (slot.item) return NativeGroundItems::ItemBaseName(slot.item);
		char phrase[64]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, place.anchor.y + 1, place.anchor.x + 1);
		return phrase;
	}
	if (entry.kind == Kind::Tabs && entry.cells) return TabText(entry.widget, place.anchor.x);
	// Stock advanced-stash layouts name each fixed slot by its item-code
	// string key, including an empty slot (BankExpansionLayoutHD.json).
	if (entry.type == "AdvancedStashSlotWidget") {
		if (auto name = UiText::Clean(Localization::Text(entry.name)); !name.empty()) return name;
	}
	if (entry.belt) {
		// The slot the cursor is on, which the belt is read at while the cursor stands in for the focus.
		const auto slot = NativeUi::BeltItem(entry.widget);
		return slot.item ? NativeGroundItems::ItemBaseName(slot.item) : std::string(Strings::Current().slotEmpty);
	}
	// A skill's icon has no text of its own; the text under it is its level.
	if (auto skill = NativeSkillIcons::Name(entry.widget); !skill.empty()) return skill;
	// The hire list's skill icons and gold lines: by the skill, and by the game's words with the amount.
	if (auto hire = HireStats::Said(entry.widget, entry.panel); !hire.empty()) return hire;
	// A class's figure is called by the class's key in the string table.
	if (NativeClassChoice::Is(entry.widget)) {
		std::string name = UiText::Clean(Localization::Text(entry.name));
		if (name.empty()) name = entry.name;
		if (NativeClassChoice::Of(entry.widget) == NativeClassChoice::State::Chosen) name += ", " + std::string(Strings::Current().checked);
		return name;
	}
	const auto readout = UiText::Describe(NativeUi::Snapshot(entry.widget, entry.widget));
	std::string text = !readout.option.empty() ? readout.option : readout.body;
	if (text.empty()) text = UiText::Clean(NativeWidgets::ReadText(entry.widget));
	return text.empty() ? entry.name : text;
}

void Expect(uint64_t now, std::string text) {
	g_landedAt = now;
	g_toSay = std::move(text);
}

// The row of the hire list a place is drawn inside: a skill icon is the row's
// child, a price the child of a container in it. Null for anything else.
const Entry* RowAround(const Entry& entry) {
	if (entry.point.width > 0) return nullptr; // a row itself
	uintptr_t widget = entry.widget;
	for (int depth = 0; depth < 3 && widget; ++depth) {
		widget = NativeUi::Parent(widget);
		for (const auto& other : g_entries)
			if (other.widget == widget && other.point.width > 0) return &other;
	}
	return nullptr;
}

bool Land(const D2RL::PluginContext* context, uint64_t now, const Place& place) {
	const auto* entry = EntryOf(place.widget);
	if (!entry) return false;
	const bool holding = InventoryCursor::HoldingItem(context).value_or(false);
	const auto rect = PointerRect(*entry, place, holding);
	if (!rect) {
		AsyncLog::WarnF(context, "[cursor] no rectangle for %s (%s); the cursor stays", entry->name.c_str(), entry->type.c_str());
		return false;
	}
	const auto* target = MenuCursor::Find(g_targets, entry->widget);
	const uint64_t selection = entry->cells ? Selection(*entry, place.anchor, target ? target->grid.height : 1)
		: NativeUi::FocusSelection(entry->widget);
	const uintptr_t owner = NativeSkillNavigation::Owner(entry->widget);
	g_place = place;
	g_holding = holding;
	g_said.clear();
	POINT before{};
	(void)GetCursorPos(&before);
	const bool placed = MenuMouse::Place(*rect);
	// Item browsing accepts the next draw immediately: its reader checks the
	// selected unit or fixed stash identity. Other controls retain their guard
	// against the previous control's tooltip; their own labels still read at once.
	const bool item = entry->kind == Kind::InventoryGrid || entry->kind == Kind::InventorySlot || entry->kind == Kind::InventoryItem;
	NativeTooltips::Discard(now + (item ? 0 : ControlTooltipSettleMs));
	NativeUi::SetCursor({owner ? owner : entry->panel, entry->widget}, selection);
	POINT pointer{};
	(void)GetCursorPos(&pointer);
	// The pointer is where it is put at once (MEASURED 2026-09-27). One that was
	// put somewhere new and has not moved did not go there.
	const NativeUi::Cell point{rect->x + rect->width / 2, rect->y + rect->height / 2};
	const bool same = point.x == g_pointerAt.x && point.y == g_pointerAt.y;
	g_landed = placed && (same || pointer.x != before.x || pointer.y != before.y);
	if (g_landed) g_pointerAt = point;
	else AsyncLog::WarnF(context, "[cursor] the pointer did not go to %s: it is at %ld,%ld and was put on %d,%d; a click is refused",
		entry->name.c_str(), pointer.x, pointer.y, point.x, point.y);
	AsyncLog::InfoF(context, "[cursor] on %s (%s) cell %d,%d rect %d,%d %dx%d holding=%u placed=%u pointer %ld,%ld; %.1f ms into the tick",
		entry->name.c_str(), entry->type.c_str(), entry->cells ? place.anchor.x : -1, entry->cells ? place.anchor.y : -1,
		rect->x, rect->y, rect->width, rect->height, holding ? 1u : 0u, placed ? 1u : 0u, pointer.x, pointer.y,
		Ms(g_tickStart, Clock::now()));
	if (entry->belt) AsyncLog::InfoF(context, "[cursor] belt slot %d; the belt takes the point for slot %d", static_cast<int>(selection),
		NativeBeltRows::SlotAt(entry->widget, {rect->x + rect->width / 2, rect->y + rect->height / 2}));
	// The game sees what is under the pointer only once the mouse moves over it
	// (menu_mouse.h): without the nudge a skill showed no tooltip and took no
	// F-key, and a waypoint was read with the Close button's tooltip.
	if (g_landed) MenuMouse::Nudge();
	if (const auto skill = NativeSkillIcons::Skill(entry->widget))
		AsyncLog::InfoF(context, "[cursor] skill id=%d \"%s\"", *skill, NativeSkillIcons::Name(entry->widget).c_str());
	Expect(now, Name(*entry, place));
	// No reader names a class or a row of an open dropdown (MEASURED 2026-09-27):
	// waiting for one only left them silent, so they are named at once. Nor a
	// skill icon or a gold line of the hire list: left to the wait they were
	// called "Skill2" and "YourGoldTooltipWidget" (MEASURED 2026-09-30).
	if (NativeClassChoice::Is(entry->widget) || g_menu.starts_with(DropdownList)
		|| !HireStats::Said(entry->widget, entry->panel).empty()) {
		AsyncLog::InfoF(context, "[cursor] said at once \"%s\"", g_toSay.c_str());
		ScreenReader::Speak(g_toSay, true);
		g_said = g_toSay;
		g_landedAt = 0;
		g_toSay.clear();
	}
	// A quest shows what it is about when it is clicked, not when the pointer is over it.
	if (entry->kind == Kind::QuestButton) MenuMouse::ClickAt(*rect, MenuMouse::Button::Left);
	// A row that one click chooses is chosen as the cursor comes to it: Enter
	// acts on the row the list has chosen, not on the one the pointer is over
	// (READ), and the two have to be the same row for a player who cannot see
	// which is lit. A row that is chosen already is not clicked again: a second
	// click within half a second would act on it.
	if (const auto places = NativeListRows::Of(entry->widget); places && places->chosen != places->row && g_landed) {
		AsyncLog::InfoF(context, "[cursor] row %d is chosen as the cursor comes to it; the list had %d", places->row, places->chosen);
		MenuMouse::ClickAt(*rect, MenuMouse::Button::Left);
		g_rowClicked = {entry->widget, now};
	} else if (const Entry* row = g_landed ? RowAround(*entry) : nullptr) {
		// An icon or a price inside a row: the row it is drawn in is chosen too,
		// by a click on the row's own place, and the pointer goes back to the icon.
		if (const auto around = NativeListRows::Of(row->widget); around && around->chosen != around->row) {
			AsyncLog::InfoF(context, "[cursor] row %d is chosen as the cursor comes to %s inside it; the list had %d",
				around->row, entry->name.c_str(), around->chosen);
			MenuMouse::ClickAt(row->point, MenuMouse::Button::Left, *rect);
			g_rowClicked = {row->widget, now};
		}
	}
	return true;
}

void Census(const D2RL::PluginContext* context) {
	AsyncLog::InfoF(context, "[cursor] menu: %s-- %zu things", g_menu.c_str(), g_targets.size());
	for (size_t i = 0; i < g_targets.size(); ++i) {
		const auto& target = g_targets[i];
		AsyncLog::InfoF(context, "[cursor] thing %s (%s) rect %d,%d %dx%d cells %dx%d items %zu", g_entries[i].name.c_str(),
			g_entries[i].type.c_str(), target.rect.x, target.rect.y, target.rect.width, target.rect.height,
			target.grid.width, target.grid.height, target.grid.items.size());
	}
	constexpr const char* Ways[] = {"left", "right", "up", "down"};
	for (const auto& link : g_links) {
		const auto* from = EntryOf(link.from.widget);
		const auto* to = EntryOf(link.to.widget);
		AsyncLog::InfoF(context, "[cursor] a step made for what the arrows miss: from %s cell %d,%d %s to %s cell %d,%d",
			from ? from->name.c_str() : "?", link.from.anchor.x, link.from.anchor.y, Ways[static_cast<size_t>(link.direction)],
			to ? to->name.c_str() : "?", link.to.anchor.x, link.to.anchor.y);
	}
	const auto all = MenuCursor::Everything(g_targets);
	const auto reached = MenuCursor::Reachable(g_targets, MenuCursor::First(g_targets), g_links);
	size_t missed = 0;
	for (const auto& place : all) {
		if (std::find(reached.begin(), reached.end(), place) != reached.end()) continue;
		++missed;
		const auto* entry = EntryOf(place.widget);
		AsyncLog::WarnF(context, "[cursor] NOT REACHED by the arrows: %s cell %d,%d", entry ? entry->name.c_str() : "?",
			place.anchor.x, place.anchor.y);
	}
	AsyncLog::InfoF(context, "[cursor] %zu places, %zu not reached", all.size(), missed);
}

void Release(const D2RL::PluginContext* context, const char* reason) {
	if (g_engaged && context) AsyncLog::InfoF(context, "[cursor] released: %s", reason);
	g_engaged = false;
	ClickWords::SetKeyboard(false);
	g_landed = false;
	g_pointerAt = {};
	g_place = {};
	g_menu.clear();
	g_landedAt = 0;
	g_toSay.clear();
	g_said.clear();
	g_titleUntil = 0;
	g_listHand = NativeSkillHand::Hand::None;
	g_adjusting = {};
	g_choosing = {};
	g_rowClicked = {};
	NativeUi::ClearCursor();
}

// When play goes on the pointer is put on the HUD, between its two skill
// buttons. Left where the menu had it, it would be over the world, where the
// game takes what is under it for what the player means and names it.
void Park(const D2RL::PluginContext* context) {
	const uintptr_t hud = NativeWidgets::FindPanel(Hud);
	const uintptr_t left = FindNamed(hud, LeftSkill, false, 0), right = FindNamed(hud, RightSkill, false, 0);
	const auto from = left ? NativeSkillNavigation::RectAt(left, NativeUi::FocusSelection(left)) : std::nullopt;
	const auto to = right ? NativeSkillNavigation::RectAt(right, NativeUi::FocusSelection(right)) : std::nullopt;
	const int gap = from && to ? to->x - (from->x + from->width) : 0;
	if (gap < 2) {
		AsyncLog::InfoF(context, "[cursor] the pointer stays where the menu left it: left skill button %s, right %s, %d between them",
			!left ? "not shown" : from ? "found" : "has no rectangle", !right ? "not shown" : to ? "found" : "has no rectangle", gap);
		return;
	}
	const NativeUi::Rect rest{from->x + from->width, from->y, gap, from->height};
	AsyncLog::InfoF(context, "[cursor] play goes on: the pointer to %d,%d, between the skill buttons",
		rest.x + rest.width / 2, rest.y + rest.height / 2);
	MenuMouse::PlaceAfter(rest);
}

// The list of skills is one panel for both hands and has no title. The hand it
// is open for is said when it opens, and again when it changes to the other.
void SayHand(const D2RL::PluginContext* context, uint64_t now, const std::string& menu) {
	using NativeSkillHand::Hand;
	const bool open = (' ' + menu).find(' ' + std::string(SkillList) + ' ') != std::string::npos;
	const Hand hand = open ? NativeSkillHand::Chosen() : Hand::None;
	if (hand == g_listHand) return;
	g_listHand = hand;
	if (hand == Hand::None) return;
	const std::string words = UiText::Clean(Localization::Text(hand == Hand::Left ? LeftHandKey : RightHandKey));
	AsyncLog::InfoF(context, "[cursor] the list of skills is open for the %s hand; said \"%s\"", hand == Hand::Left ? "left" : "right", words.c_str());
	if (words.empty()) return;
	ScreenReader::Speak(words, true);
	g_titleUntil = now + TitleMs;
}

// The menu as it is now, and the cursor on it: where it was if that still
// stands, at the menu's first thing otherwise.
void Refresh(const D2RL::PluginContext* context, uint64_t now) {
	const bool build = g_refreshedAt != now;
	g_refreshedAt = now;
	g_refreshAt = now + RefreshMs;
	const auto checks = NativeMemory::Checks();
	const auto start = Clock::now();
	const std::string menu = build ? Build(context) : g_menu;
	const double took = Ms(start, Clock::now());
	if (build && (g_logBuild || (took >= SlowBuildMs && now - g_buildLogged >= 1000))) {
		if (!g_logBuild) g_buildLogged = now;
		const auto after = NativeMemory::Checks();
		AsyncLog::InfoF(context, "[cursor] read the menu again%s in %.1f ms: walk %.1f ms (grids %.1f ms, %d cells), thin %.1f ms, "
			"mend %.1f ms; %llu page checks taking %.1f ms; %zu places", g_logBuild ? " for a key" : "", took, g_cost.walk,
			g_cost.grids, g_cost.cells, g_cost.thin, g_cost.mend, static_cast<unsigned long long>(after.count - checks.count),
			after.ms - checks.ms, g_targets.size());
	}
	g_logBuild = false;
	const bool changed = menu != g_menu;
	g_menu = menu;
	SayHand(context, now, menu);
	if (g_targets.empty()) {
		if (g_place.widget) { g_place = {}; NativeUi::ClearCursor(); }
		return;
	}
	// The belt counts from the bottom row. When rows come or go above it the
	// cursor stays on its slot, and the pointer follows the slot to where it is drawn now.
	bool redrawn = false;
	int rows = 0;
	for (size_t i = 0; i < g_entries.size(); ++i) {
		if (!g_entries[i].belt) continue;
		rows = g_targets[i].grid.height;
		if (g_beltRows && rows != g_beltRows && g_place.widget == g_entries[i].widget) {
			g_place.anchor.y += rows - g_beltRows;
			g_place.travel.y += rows - g_beltRows;
			redrawn = true;
		}
	}
	// The things are listed again when their number changes under the same
	// panels: 2026-09-30 the NPC's two answers appeared over the hire list, the
	// menu's name stayed the same, and the log could not show they were there.
	const bool recount = g_targets.size() != g_things && now - g_thingsLogged >= 1000;
	if (changed || rows != g_beltRows || recount) {
		Census(context);
		g_things = g_targets.size();
		g_thingsLogged = now;
	}
	g_beltRows = rows;
	auto place = MenuCursor::Settle(g_targets, g_place);
	if (!place.widget) {
		uintptr_t content = 0;
		// The keyboard inventory opens on its items, not the Close button
		// above the equipment or a tab chooser left under the pointer.
		if (g_menu.starts_with("PlayerInventory"))
			for (const auto& entry : g_entries)
				if (entry.kind == Kind::InventoryGrid) { content = entry.widget; break; }
		place = KeyboardMenuPolicy::Entry(g_targets, content);
	}
	const bool holding = InventoryCursor::HoldingItem(context).value_or(false);
	if (place == g_place && holding == g_holding && !redrawn) return;
	// What the menu is called is heard out first, unless an arrow asks sooner.
	if (now < g_titleUntil) { g_refreshAt = g_titleUntil; return; }
	(void)Land(context, now, place);
}

// The left and right arrows on a slider or an option that steps change its
// value and leave the cursor where it is (the user, 2026-09-27). False for
// any other control.
bool Adjust(const D2RL::PluginContext* context, uint64_t now, const Entry& entry, bool up) {
	if (NativeValueControls::Of(entry.widget) == NativeValueControls::Kind::None) return false;
	if (g_adjusting.widget) return true; // one step at a time
	const auto value = NativeValueControls::Value(entry.widget);
	if (!value) {
		AsyncLog::WarnF(context, "[cursor] value of %s (%s) could not be read", entry.name.c_str(), entry.type.c_str());
		ScreenReader::Speak(Strings::Current().unavailable, true);
		return true;
	}
	g_adjusting = {entry.widget, *value, up, false, now, now + OwnStepMs};
	return true;
}

void Adjusted(const D2RL::PluginContext* context, uint64_t now) {
	auto& step = g_adjusting;
	if (!step.widget || now < step.look || MenuMouse::Busy()) return;
	const auto* entry = EntryOf(step.widget);
	const auto value = entry ? NativeValueControls::Value(step.widget) : std::nullopt;
	if (!value) { step = {}; return; }
	if (*value == step.before && !step.clicked) {
		const uintptr_t arrow = NativeValueControls::Arrow(step.widget, step.up);
		const auto rect = arrow ? NativeSkillNavigation::RectAt(arrow, NativeUi::FocusSelection(arrow)) : std::nullopt;
		if (rect && OnScreen(*rect)) {
			AsyncLog::InfoF(context, "[cursor] value of %s: the key left it at %d; click on its arrow %s", entry->name.c_str(), *value,
				step.up ? "up" : "down");
			const auto back = PointerRect(*entry, g_place, false);
			MenuMouse::ClickAt(*rect, MenuMouse::Button::Left, back.value_or(NativeUi::Rect{}));
			step.clicked = true;
			step.look = now + AfterClickMs;
			return;
		}
		AsyncLog::WarnF(context, "[cursor] value of %s: its arrow %s", entry->name.c_str(), arrow ? "has no rectangle" : "is not there");
	}
	const auto shown = NativeValueControls::Shown(step.widget);
	AsyncLog::InfoF(context, "[cursor] value of %s: %d -> %d %s, shown \"%s\"", entry->name.c_str(), step.before, *value,
		*value == step.before ? "unchanged" : step.clicked ? "by a click on its arrow" : "by the game on the key", shown.c_str());
	// A reader that heard the game say the value changed has said it already.
	if (ScreenReader::LastSpoken() < step.since && !shown.empty()) ScreenReader::Speak(shown, true);
	step = {};
}

// A class that was clicked is said once it is chosen, with what the screen
// then shows of it: its description.
void Chosen(const D2RL::PluginContext* context, uint64_t now) {
	auto& choice = g_choosing;
	if (!choice.widget) return;
	const auto* entry = EntryOf(choice.widget);
	const auto state = entry ? NativeClassChoice::Of(choice.widget) : NativeClassChoice::State::Unknown;
	const bool over = !entry || now - choice.since >= ChoiceMs;
	if (state != NativeClassChoice::State::Chosen && !over) return;
	if (state == NativeClassChoice::State::Chosen) {
		std::string words = Name(*entry, {});
		const auto about = NativeClassChoice::Description(entry->panel);
		if (!about.empty()) words += ". " + about;
		AsyncLog::InfoF(context, "[cursor] class %s chosen after %llu ms; said \"%s\"", entry->name.c_str(),
			static_cast<unsigned long long>(now - choice.since), words.c_str());
		ScreenReader::Speak(words, true);
	} else if (entry) {
		AsyncLog::WarnF(context, "[cursor] class %s was clicked and is not chosen: its state is %d", entry->name.c_str(), static_cast<int>(state));
		ScreenReader::Speak(Strings::Current().unavailable, true);
	}
	choice = {};
}

void Step(const D2RL::PluginContext* context, uint64_t now, Direction direction) {
	if (!g_engaged) {
		g_engaged = true;
		AsyncLog::Info(context, "[cursor] taken by an arrow key");
		Refresh(context, now);
		return; // the first press says where the cursor is
	}
	const bool waited = now < g_titleUntil && !g_place.widget;
	g_titleUntil = 0;
	g_logBuild = true;
	Refresh(context, now);
	if (waited) return; // the press ends the wait, and says where the cursor is
	if (!g_place.widget) return;
	if (direction == Direction::Left || direction == Direction::Right)
		if (const auto* entry = EntryOf(g_place.widget); entry && Adjust(context, now, *entry, direction == Direction::Right)) return;
	const bool holding = InventoryCursor::HoldingItem(context).value_or(false);
	const auto next = MenuCursor::Move(g_targets, g_place, direction, holding, g_links);
	if (next == g_place) {
		// Nothing that way: where the cursor is, again.
		if (const auto* entry = EntryOf(g_place.widget)) ScreenReader::Speak(Name(*entry, g_place), true);
		return;
	}
	(void)Land(context, now, next);
}

void Click(const D2RL::PluginContext* context, uint64_t now, MenuMouse::Button button) {
	// Counted from the last press that came, so a key held down clicks once.
	const bool repeated = now - g_lastClick < ClickGapMs;
	g_lastClick = now;
	if (repeated) return;
	const bool shift = Down(VK_SHIFT), control = Down(VK_CONTROL), alt = Down(VK_MENU);
	const auto* entry = g_engaged ? EntryOf(g_place.widget) : nullptr;
	if (entry && !g_landed) {
		// Never a click on whatever the pointer happens to be over.
		AsyncLog::WarnF(context, "[cursor] no click: the pointer is not on %s", entry->name.c_str());
		ScreenReader::Speak(Strings::Current().unavailable, true);
		return;
	}
	// A click on a slider sets its value to where the click was, the middle of
	// it here. The value is said instead; the arrows change it. A click on an
	// option that steps goes to its next state, as Enter does for the game.
	if (entry && !alt && NativeValueControls::Of(entry->widget) == NativeValueControls::Kind::Slider) {
		AsyncLog::InfoF(context, "[cursor] no click on the slider %s: its value is said", entry->name.c_str());
		ScreenReader::Speak(NativeValueControls::Shown(entry->widget), true);
		return;
	}
	// Alt-clicks use the game's mouse path with Alt held, including stat reset.
	if (entry && !alt && button == MenuMouse::Button::Left && Adjust(context, now, *entry, true)) return;
	// A skill icon or a gold line of the hire list has nothing a click does, and
	// a click inside a row the cursor has just chosen would be the second click
	// of two on that row, which hires. It is said again; the row is what hires.
	if (entry) {
		if (const auto said = HireStats::Said(entry->widget, entry->panel); !said.empty()) {
			AsyncLog::InfoF(context, "[cursor] no click on %s of the hire list: it is said", entry->name.c_str());
			ScreenReader::Speak(said, true);
			return;
		}
	}
	// A click on the class that is chosen would put it back, and one on a figure
	// that is stepping forward or back is lost. Another class is chosen by a
	// click on it, which puts this one back.
	if (entry && NativeClassChoice::Is(entry->widget)) {
		const auto state = NativeClassChoice::Of(entry->widget);
		if (state != NativeClassChoice::State::NotChosen && state != NativeClassChoice::State::Unknown) {
			AsyncLog::InfoF(context, "[cursor] no click on %s: its state is %d", entry->name.c_str(), static_cast<int>(state));
			ScreenReader::Speak(Name(*entry, g_place), true);
			return;
		}
		g_choosing = {entry->widget, now};
	}
	const auto rect = entry ? PointerRect(*entry, g_place, InventoryCursor::HoldingItem(context).value_or(false)) : std::nullopt;
	// A row that one click only chooses is acted on by two, as the mouse does it
	// (the user, 2026-09-27, of five clicks on a character that began no game:
	// the click "should have clicked"). A click that went to the row a moment
	// ago is the first of the two.
	const bool row = entry && rect && button == MenuMouse::Button::Left && !shift && !control && !alt && NativeListRows::TwoClicks(entry->widget);
	const bool twice = row && !(g_rowClicked.widget == entry->widget && now - g_rowClicked.at < RowPairMs);
	AsyncLog::InfoF(context, "[cursor] %s click%s%s%s%s on %s", button == MenuMouse::Button::Left ? "left" : "right",
		shift ? " with Shift" : "", control ? " with Ctrl" : "", alt ? " with Alt" : "", twice ? ", twice" : "", entry ? entry->name.c_str() : "where the pointer is");
	if (rect) MenuMouse::ClickAt(*rect, button);
	else MenuMouse::Click(button);
	if (twice) MenuMouse::ClickAt(*rect, button);
	g_rowClicked = row ? RowClick{entry->widget, now} : RowClick{};
	g_refreshAt = now + AfterClickMs;
}

void ClickWidget(const D2RL::PluginContext* context, uint64_t now, uintptr_t widget, uint64_t selection, const char* what) {
	const auto rect = NativeSkillNavigation::RectAt(widget, selection);
	if (!rect) {
		AsyncLog::WarnF(context, "[cursor] %s: no rectangle for %s", what, NativeWidgets::ReadName(widget).c_str());
		ScreenReader::Speak(Strings::Current().unavailable, true);
		return;
	}
	const auto* entry = EntryOf(g_place.widget);
	const auto back = entry ? PointerRect(*entry, g_place, g_holding) : std::nullopt;
	AsyncLog::InfoF(context, "[cursor] %s: click on %s", what, NativeWidgets::ReadName(widget).c_str());
	MenuMouse::ClickAt(*rect, MenuMouse::Button::Left, back.value_or(NativeUi::Rect{}));
	NativeTooltips::Discard(now + AfterClickMs);
	g_refreshAt = now + AfterClickMs;
}

void StepTab(const D2RL::PluginContext* context, uint64_t now, int delta) {
	Refresh(context, now);
	// A bar of tabs first: the stash, the shop, the quest log, the waypoints, the options.
	for (const auto& entry : g_entries) {
		if (entry.kind != Kind::Tabs || !entry.cells) continue;
		const int tabs = NativeUi::TabCount(entry.widget), open = NativeUi::SelectedTab(entry.widget);
		const int next = std::clamp((open < 0 ? 0 : open) + delta, 0, tabs - 1);
		if (next == open) { ScreenReader::Speak(TabText(entry.widget, next), true); return; }
		ClickWidget(context, now, entry.widget, Pack({next, 0}), "tab");
		Expect(now + AfterClickMs, TabText(entry.widget, next));
		return;
	}
	// Tabs that are controls of their own: the skill tree's. The open one is
	// the one the skills shown are drawn inside.
	std::vector<const Entry*> tabs;
	for (const auto& entry : g_entries) if (entry.kind == Kind::TabButton) tabs.push_back(&entry);
	if (tabs.size() < 2) { ScreenReader::Speak(Strings::Current().unavailable, true); return; }
	std::sort(tabs.begin(), tabs.end(), [](const Entry* a, const Entry* b) {
		const auto* first = MenuCursor::Find(g_targets, a->widget);
		const auto* second = MenuCursor::Find(g_targets, b->widget);
		return first->rect.y != second->rect.y ? first->rect.y < second->rect.y : first->rect.x < second->rect.x;
	});
	int open = -1;
	for (size_t i = 0; i < tabs.size() && open < 0; ++i)
		for (const auto& entry : g_entries) if (entry.tab == tabs[i]->widget) { open = static_cast<int>(i); break; }
	const int next = std::clamp((open < 0 ? 0 : open) + delta, 0, static_cast<int>(tabs.size()) - 1);
	const auto* tab = tabs[static_cast<size_t>(next)];
	AsyncLog::InfoF(context, "[cursor] tabs: %zu, open %d, to %d", tabs.size(), open, next);
	if (next == open) { ScreenReader::Speak(Name(*tab, {}), true); return; }
	ClickWidget(context, now, tab->widget, NativeUi::FocusSelection(tab->widget), "tab");
	Expect(now + AfterClickMs, Name(*tab, {}));
}

void StepMenu(const D2RL::PluginContext* context, uint64_t now, int delta) {
	// The menu that is open: the one opened last.
	int open = -1;
	const auto& panels = PanelState::OpenPanels();
	for (auto name = panels.rbegin(); name != panels.rend() && open < 0; ++name)
		for (size_t i = 0; i < std::size(Menus); ++i)
			if (name->starts_with(Menus[i].panel)) { open = static_cast<int>(i); break; }
	const int count = static_cast<int>(std::size(Menus));
	const int next = open < 0 ? (delta > 0 ? 0 : count - 1) : ((open + delta) % count + count) % count;
	const uintptr_t hud = NativeWidgets::FindPanel(Hud);
	const uintptr_t button = FindNamed(hud, Menus[next].button, false, 0);
	const auto to = button ? NativeSkillNavigation::RectAt(button, NativeUi::FocusSelection(button)) : std::nullopt;
	AsyncLog::InfoF(context, "[cursor] menus: open %d, to %d (%.*s), button %s", open, next,
		static_cast<int>(Menus[next].button.size()), Menus[next].button.data(), !button ? "not shown" : to ? "found" : "has no rectangle");
	if (!to) { ScreenReader::Speak(Strings::Current().unavailable, true); return; }
	// The open menu is shut the way it was opened, by its own button; the game
	// menu covers the mini menu, and is left by its own way back.
	if (open >= 0) {
		const bool paused = Menus[open].button == Menus[std::size(Menus) - 1].button;
		uintptr_t shut = paused ? 0 : FindNamed(hud, Menus[open].button, false, 0);
		if (paused) for (const auto& entry : g_entries) if (entry.name == ReturnToGame) shut = entry.widget;
		if (const auto from = shut ? NativeSkillNavigation::RectAt(shut, NativeUi::FocusSelection(shut)) : std::nullopt)
			MenuMouse::ClickAt(*from, MenuMouse::Button::Left);
	}
	MenuMouse::ClickAt(*to, MenuMouse::Button::Left);
	NativeTooltips::Discard(now + AfterClickMs);
	g_refreshAt = now + 2 * AfterClickMs;
}

// What was said of where the cursor is, again: by the reader that said it, or
// by the cursor when it named the place itself.
void SayAgain(const D2RL::PluginContext* context, uint64_t now) {
	AsyncLog::InfoF(context, "[cursor] say again: %s", g_said.empty() ? "asked of the readers" : "the cursor's own words");
	if (!g_said.empty()) { ScreenReader::Speak(g_said, true); return; }
	GameTextReader::RequestRepeat();
	// A reader with nothing to say again leaves the place to be named here.
	if (const auto* entry = g_engaged ? EntryOf(g_place.widget) : nullptr) Expect(now, Name(*entry, g_place));
}

// On the character screen the pad's right stick up steps through the lines
// of stats, so J reads instead what the game's tooltip says of the line the
// cursor is on: a pad player cannot reach that at all (the user, 2026-09-28).
bool SayStatTooltip(const D2RL::PluginContext* context, uint64_t now) {
	if (g_menu.find("CharacterStatsPanel") == std::string::npos) return false;
	const auto tooltip = NativeTooltips::Latest(now);
	std::string text;
	if (g_place.widget && tooltip.widget == g_place.widget)
		for (const auto& line : TooltipText::Read(tooltip.body).lines) text += (text.empty() ? "" : ". ") + line;
	AsyncLog::InfoF(context, "[cursor] stat tooltip of %s: \"%s\"%s", EntryOf(g_place.widget) ? EntryOf(g_place.widget)->name.c_str() : "nothing",
		text.c_str(), tooltip.widget && tooltip.widget != g_place.widget ? " (the tooltip drawn is another's)" : "");
	ScreenReader::Speak(text.empty() ? std::string(Strings::Current().detailNone) : text, true);
	return true;
}

void Do(const D2RL::PluginContext* context, uint64_t now, Act act) {
	switch (act) {
	case Act::ClickLeft:    Click(context, now, MenuMouse::Button::Left); break;
	case Act::ClickRight:   Click(context, now, MenuMouse::Button::Right); break;
	case Act::TabPrevious:  StepTab(context, now, -1); break;
	case Act::TabNext:      StepTab(context, now, 1); break;
	case Act::MenuPrevious: StepMenu(context, now, -1); break;
	case Act::MenuNext:     StepMenu(context, now, 1); break;
	case Act::SayAgain:     SayAgain(context, now); break;
	// The pad's right stick up: the description of what the cursor is on, for
	// a player with autodetail off (the user, 2026-09-28).
	case Act::Detail:
		if (SayStatTooltip(context, now)) break;
		AsyncLog::Info(context, "[cursor] description asked for");
		InputDirection::PostRightStickPush(InputDirection::StickPush::Up);
		break;
	}
}

// The keys of the menu seen directly. One the loader delivers is the loader's;
// one it does not, on a screen where it delivers none, is acted on from here.
void Watch(const D2RL::PluginContext* context, uint64_t now, bool typing) {
	const bool shift = Down(VK_SHIFT), control = Down(VK_CONTROL), alt = Down(VK_MENU);
	for (auto& key : g_watched) {
		const bool down = Down(key.key);
		const bool pressed = down && !key.down;
		key.down = down;
		// PageUp, PageDown, Home and End with a modifier are reading keys, and
		// the settings list steps with them while it is open.
		const bool mine = key.with == With::ClickModifiers || (key.with == With::Shift && !control)
			|| (!shift && !control && !KeyboardService::SettingsOpen());
		if (pressed && !typing && (!alt || key.with == With::ClickModifiers) && mine) {
			key.waiting = true;
			key.since = now;
		}
		if (!key.waiting) continue;
		const size_t act = static_cast<size_t>(key.act);
		const uint64_t delivered = g_delivered[act].load();
		if (delivered && delivered + DeliveredMs >= key.since) { key.waiting = false; continue; }
		if (now - key.since < GraceMs) continue;
		key.waiting = false;
		if (!key.logged) {
			key.logged = true;
			AsyncLog::InfoF(context, "[menu-keys] %s came by watching the key: the loader did not deliver it here (menu: %s)",
				ActNames[act], g_menu.c_str());
		}
		g_pending |= 1u << act;
	}
}
}

bool Active() { return g_active.load(); }
bool Engaged() { return g_active.load() && g_engaged; }
bool Typing() { return g_typing.load(); }

void Deliver(Act act) {
	const size_t index = static_cast<size_t>(act);
	g_delivered[index] = GetTickCount64();
	g_pending |= 1u << index;
}

bool Press(const D2RL::PluginContext* context, std::string_view button) {
	if (!g_active.load()) return false;
	const uint64_t now = GetTickCount64();
	Refresh(context, now);
	for (const auto& entry : g_entries) {
		if (entry.name != button) continue;
		ClickWidget(context, now, entry.widget, NativeUi::FocusSelection(entry.widget), "key");
		return true;
	}
	AsyncLog::InfoF(context, "[cursor] key: %.*s is not in this menu", static_cast<int>(button.size()), button.data());
	return false;
}

bool LeftSkillList(const D2RL::PluginContext* context) {
	const uint64_t now = GetTickCount64();
	const uintptr_t hud = Foreground() ? NativeWidgets::FindPanel(Hud) : 0;
	const uintptr_t button = hud && NativeUi::Visible(hud) ? FindNamed(hud, LeftSkill, false, 0) : 0;
	const auto rect = button ? NativeSkillNavigation::RectAt(button, NativeUi::FocusSelection(button)) : std::nullopt;
	const bool there = rect && OnScreen(*rect);
	AsyncLog::InfoF(context, "[cursor] left skill: the HUD's button %s", !button ? "is not shown" : !rect ? "has no rectangle"
		: there ? "is clicked" : "is off the screen");
	if (!there) return false;
	MenuMouse::ClickAt(*rect, MenuMouse::Button::Left);
	NativeTooltips::Discard(now + AfterClickMs);
	g_refreshAt = now + AfterClickMs;
	return true;
}

bool Sort(const D2RL::PluginContext* context) {
	if (!g_active.load()) return false;
	const uint64_t now = GetTickCount64();
	Refresh(context, now);
	const Entry* grid = nullptr;
	if (const auto* on = EntryOf(g_place.widget); on && on->kind == Kind::InventoryGrid) grid = on;
	for (const auto& entry : g_entries) {
		if (grid) break;
		if (entry.kind == Kind::InventoryGrid && NativeWidgets::ReadName(entry.panel).starts_with("PlayerInventory")) grid = &entry;
	}
	if (!grid || NativeSort::Sort(context, grid->widget) != NativeSort::Result::Sorted) return false;
	// The items are where the game put them now, and the cursor is on another.
	NativeTooltips::Discard(now + AfterClickMs);
	g_refreshAt = now + AfterClickMs;
	return true;
}

bool FillBelt(const D2RL::PluginContext* context) {
	if (!g_active.load()) return false;
	const uint64_t now = GetTickCount64();
	if (now - g_lastFill < FillGapMs) return true;
	Refresh(context, now);
	for (const auto& entry : g_entries) {
		if (!entry.belt) continue;
		if (NativeBeltFill::Fill(context, entry.widget) != NativeBeltFill::Result::Sent) return false;
		g_lastFill = now;
		// The potions are where the game put them now.
		NativeTooltips::Discard(now + AfterClickMs);
		g_refreshAt = now + AfterClickMs;
		return true;
	}
	AsyncLog::Info(context, "[cursor] fill: the belt is not in this menu");
	return false;
}

void Reset() {
	Release(nullptr, "reset");
	MenuMouse::Reset();
	g_entries.clear();
	g_targets.clear();
	g_links.clear();
	g_mended.clear();
	g_active = false;
	g_beltShown = false;
	g_beltRows = 0;
	g_keepLetters = false;
	g_keepArrows = false;
	if (g_letterHook) UnhookWindowsHookEx(g_letterHook);
	g_letterHook = nullptr;
	g_letterTried = false;
	g_pending = 0;
	g_refreshedAt = UINT64_MAX;
	for (auto& arrow : g_arrows) { arrow.down = false; arrow.next = 0; }
	for (auto& key : g_watched) { key.down = key.waiting = false; }
}

void Tick(const D2RL::PluginContext* context) {
	const uint64_t now = GetTickCount64();
	g_refreshedAt = UINT64_MAX;
	g_tickStart = Clock::now();
	MenuMouse::Tick(now);
	const bool menu = Foreground() && NativeWidgets::SupportsTree() && NativeSkillNavigation::Available()
		&& !KeyboardService::Playing();
	g_active = menu;
	if (!menu) {
		g_typing = false;
		g_keepLetters = false;
		g_keepArrows = false;
		// The pointer was the menu's only if the cursor put it somewhere.
		const bool moved = g_place.widget != 0;
		if (g_engaged || g_place.widget) Release(context, "no menu, or the game is not in front");
		HideBelt(context);
		if (moved && Foreground() && KeyboardService::Playing()) Park(context);
		g_pending = 0;
		for (auto& arrow : g_arrows) arrow.down = Down(arrow.key);
		for (auto& key : g_watched) { key.down = Down(key.key); key.waiting = false; }
		return;
	}
	// A box of text the game says is being edited has every key. One the cursor
	// is on has the keys that type and the left and right arrows, whatever the
	// game says: out of controller mode it says nothing (READ), and the name box
	// of character creation takes what is typed at any time.
	const bool editing = NativeSkillNavigation::TextEditing().value_or(false);
	const auto* on = g_engaged ? EntryOf(g_place.widget) : nullptr;
	const bool field = on && on->kind == Kind::TextInput;
	g_typing = field;
	HookLetters(context);
	g_keepLetters = g_engaged && !field && g_menu.find("CharacterCreatePanel") != std::string::npos;
	const bool typing = editing || field;
	g_keepArrows = g_engaged && !typing && g_menu.find("CharacterSelectPanel") != std::string::npos;
		// Follow the game's mode: an idle connected device must not leave the
	// keyboard pointer on the world/HUD until the first arrow press.
	if (!g_engaged && KeyboardMenuPolicy::Automatic(NativeSkillNavigation::ControllerMode(), InputDirection::LeftStick().connected)) {
		g_engaged = true;
		AsyncLog::Info(context, "[cursor] taken: a menu is up in keyboard mode");
		g_refreshAt = 0;
	}
	ClickWords::SetKeyboard(g_engaged);
	if (g_engaged && now >= g_refreshAt && !MenuMouse::Busy()) Refresh(context, now);

	const bool control = Down(VK_CONTROL), shift = Down(VK_SHIFT), alt = Down(VK_MENU);
	for (auto& arrow : g_arrows) {
		const bool down = Down(arrow.key);
		bool step = false;
		if (down && !arrow.down) { step = true; arrow.next = now + RepeatDelayMs; }
		else if (down && now >= arrow.next) { step = true; arrow.next = now + RepeatMs; }
		arrow.down = down;
		if (!step || editing || alt || MenuMouse::Busy()) continue;
		// Up and down leave a box of text; left and right move in what is typed.
		if (field && (arrow.direction == Direction::Left || arrow.direction == Direction::Right)) continue;
		if (control) InputDirection::PostLineStep(arrow.push);
		else if (shift) InputDirection::PostLineStep(arrow.push, true);
		else Step(context, now, arrow.direction);
	}

	Watch(context, now, typing);
	const uint32_t pending = g_pending.exchange(0);
	for (size_t act = 0; act < Acts; ++act)
		if (pending & (1u << act)) Do(context, now, static_cast<Act>(act));
	Adjusted(context, now);
	Chosen(context, now);

	// A place no reader named is named here, so no move passes in silence.
	if (g_landedAt && now >= g_landedAt && now - g_landedAt >= SilenceMs) {
		if (ScreenReader::LastSpoken() < g_landedAt && !g_toSay.empty()) {
			AsyncLog::InfoF(context, "[cursor] no reader named it; said \"%s\"", g_toSay.c_str());
			ScreenReader::Speak(g_toSay, true);
			g_said = g_toSay;
		}
		g_landedAt = 0;
		g_toSay.clear();
	}
}
}
