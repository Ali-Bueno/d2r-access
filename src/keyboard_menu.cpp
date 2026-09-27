#include "keyboard_menu.h"

#include "async_log.h"
#include "click_words.h"
#include "game_text_reader.h"
#include "input_direction.h"
#include "inventory_cursor.h"
#include "keyboard_service.h"
#include "menu_cursor.h"
#include "menu_mouse.h"
#include "native_belt_rows.h"
#include "native_ground_items.h"
#include "native_skill_navigation.h"
#include "native_sort.h"
#include "native_tooltips.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "ui_text.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
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
constexpr uint64_t SettleMs = 90;       // from a move of the pointer to a tooltip taken as the new place's
constexpr uint64_t RefreshMs = 250;     // how often the open menu is looked at again
constexpr uint64_t AfterClickMs = 250;  // from a click to a look at what it changed
constexpr uint64_t RepeatDelayMs = 500; // a held arrow steps again after this long
constexpr uint64_t RepeatMs = 180;      // and then this often
constexpr uint64_t SilenceMs = 450;     // a place no reader has named by then is named here
constexpr uint64_t GraceMs = 60;        // how long a key seen directly waits for the loader to deliver it
constexpr uint64_t DeliveredMs = 150;   // a delivery this near a key seen directly is the same press
constexpr uint64_t ClickGapMs = 200;    // a held key repeats its press; a click is not repeated
constexpr size_t MaxEntries = 256;
constexpr int MaxDepth = 32;

// Panels that are open without being a menu, by the names the layouts give them.
constexpr std::string_view NotMenus[] = {
	"TooltipsPanel", "PanelBordersPanel", "BlockingPanel", "MiniMenuPanel", "HelpButtonPanel", "AutoMap", "ChatPanel",
	"FocusManager", "GlobalData", "SaveIndicatorPanel", "NotificationPanel", "LoadScreenPanel", "MainMenuLoadingPanel",
	"LobbyBackgroundPanel", "DismissibleTooltipPanel", "TutorialTooltip", "EmoteWheelPanel", "ClickCatcher",
	"InGameMoviePlayerPanel", "MoviePlayerPanel", "ControllerOverlay", "UISwitcher", "EndGameTimerPanel",
};
constexpr std::string_view Hud = "HUDPanel";
// The HUD's two skill buttons. Between them the HUD has nothing of its own.
constexpr std::string_view LeftSkill = "LeftSkillButton", RightSkill = "RightSkillButton";

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
};
// The open menu: what each thing is, and beside it where it is for the cursor.
std::vector<Entry> g_entries;
std::vector<Target> g_targets;
std::string g_menu;         // the open panels by name, to tell one menu from the next
Place g_place;
std::atomic<bool> g_active{false};
bool g_engaged = false;     // the cursor is the keyboard's; false while a pad has the menu
bool g_holding = false;     // an item was in hand when the pointer was last placed
uint64_t g_refreshAt = 0;
uint64_t g_landedAt = 0;    // when the cursor came to where it is, 0 once that was named
std::string g_toSay;        // what to say if no reader does
std::string g_said;         // what the cursor said of where it is, when no reader did
bool g_beltShown = false;   // the game's "show belt" was turned on from here, and is to be turned off
int g_beltRows = 0;         // the rows of the belt when the menu was last read, 0 with no belt in it
uint64_t g_lastClick = 0;

struct Arrow { int key; Direction direction; InputDirection::StickPush push; bool down = false; uint64_t next = 0; };
Arrow g_arrows[] = {
	{VK_LEFT, Direction::Left, InputDirection::StickPush::Left}, {VK_RIGHT, Direction::Right, InputDirection::StickPush::Right},
	{VK_UP, Direction::Up, InputDirection::StickPush::Up}, {VK_DOWN, Direction::Down, InputDirection::StickPush::Down},
};
// The keys of the menu, watched directly beside the loader's delivery of them,
// and what may be held with each for it still to be the menu's key. Ctrl+D is
// the whole description's switch, so D is a click alone and with Shift.
enum class With { Nothing, Shift, ShiftOrControl };
struct Watched { int key; Act act; With with; bool down = false, waiting = false, logged = false; uint64_t since = 0; };
Watched g_watched[] = {
	{'A', Act::ClickLeft, With::ShiftOrControl}, {'D', Act::ClickRight, With::Shift}, {VK_PRIOR, Act::TabPrevious, With::Nothing},
	{VK_NEXT, Act::TabNext, With::Nothing}, {VK_HOME, Act::MenuPrevious, With::Nothing}, {VK_END, Act::MenuNext, With::Nothing},
	{VK_OEM_5, Act::SayAgain, With::Nothing},
};
constexpr size_t Acts = 7;
constexpr const char* ActNames[Acts] = {"left click", "right click", "previous tab", "next tab", "previous menu", "next menu",
	"say again"};
std::atomic<uint64_t> g_delivered[Acts]{};
std::atomic<uint32_t> g_pending{0};

bool Down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
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

bool NotMenu(std::string_view name) {
	return name.starts_with("HUD") || std::find(std::begin(NotMenus), std::end(NotMenus), name) != std::end(NotMenus);
}

GridJump::Grid ReadGrid(uintptr_t widget) {
	GridJump::Grid grid;
	const auto size = NativeUi::GridSize(widget);
	if (size.x < 1 || size.y < 1) return grid;
	grid.width = size.x;
	grid.height = size.y;
	std::vector<uintptr_t> units;
	for (int y = 0; y < size.y; ++y) {
		for (int x = 0; x < size.x; ++x) {
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
	if (!first || !last || (grid.width * grid.height > 1 && *first == *last)) return false;
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
	const bool control = type.kind == Kind::Button || type.kind == Kind::Toggle || type.kind == Kind::ListItem
		|| type.kind == Kind::Focusable || type.kind == Kind::InventorySlot || type.kind == Kind::InventoryItem
		|| type.kind == Kind::Tabs || type.kind == Kind::StatButton || type.kind == Kind::TabButton
		|| type.kind == Kind::ValueControl || type.kind == Kind::QuestButton || type.kind == Kind::TextInput;
	if (!whole && control && type.name != "BeltWidget") {
		if (const auto rect = NativeSkillNavigation::RectAt(widget, NativeUi::FocusSelection(widget))) {
			g_targets.push_back({widget, ToRect(*rect), {}});
			g_entries.push_back(entry);
		}
	}
	if (whole) return;
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
std::string Build(const D2RL::PluginContext* context) {
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
		if (!shown[i].name.ends_with("Modal")) continue;
		shown = {shown[i]};
		break;
	}
	std::string menu;
	bool items = false;
	for (const auto& open : shown) {
		menu += open.name;
		menu += ' ';
		items = items || open.name.find("Inventory") != std::string::npos;
		Collect(open.panel, open.panel, 0, 0);
	}
	// The belt is the HUD's, and is where potions are put from the inventory.
	const uintptr_t hud = items ? NativeWidgets::FindPanel(Hud) : 0;
	if (const uintptr_t belt = hud && NativeUi::Visible(hud) ? FindNamed(hud, "BeltWidget", true, 0) : 0) {
		ShowBelt(context, belt);
		Collect(belt, hud, 0, 0);
	} else HideBelt(context);
	Thin();
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
	if (entry.belt) {
		// The slot the cursor is on, which the belt is read at while the cursor stands in for the focus.
		const auto slot = NativeUi::BeltItem(entry.widget);
		return slot.item ? NativeGroundItems::ItemBaseName(slot.item) : std::string(Strings::Current().slotEmpty);
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
	const bool placed = MenuMouse::Place(*rect);
	NativeTooltips::Discard(now + SettleMs);
	NativeUi::SetCursor({owner ? owner : entry->panel, entry->widget}, selection);
	POINT pointer{};
	(void)GetCursorPos(&pointer);
	AsyncLog::InfoF(context, "[cursor] on %s (%s) cell %d,%d rect %d,%d %dx%d holding=%u placed=%u pointer %ld,%ld",
		entry->name.c_str(), entry->type.c_str(), entry->cells ? place.anchor.x : -1, entry->cells ? place.anchor.y : -1,
		rect->x, rect->y, rect->width, rect->height, holding ? 1u : 0u, placed ? 1u : 0u, pointer.x, pointer.y);
	if (entry->belt) AsyncLog::InfoF(context, "[cursor] belt slot %d; the belt takes the point for slot %d", static_cast<int>(selection),
		NativeBeltRows::SlotAt(entry->widget, {rect->x + rect->width / 2, rect->y + rect->height / 2}));
	Expect(now, Name(*entry, place));
	// A quest shows what it is about when it is clicked, not when the pointer is over it.
	if (entry->kind == Kind::QuestButton) MenuMouse::ClickAt(*rect, MenuMouse::Button::Left);
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
	const auto all = MenuCursor::Everything(g_targets);
	const auto reached = MenuCursor::Reachable(g_targets, MenuCursor::First(g_targets));
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
	g_place = {};
	g_menu.clear();
	g_landedAt = 0;
	g_toSay.clear();
	g_said.clear();
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

// The menu as it is now, and the cursor on it: where it was if that still
// stands, at the menu's first thing otherwise.
void Refresh(const D2RL::PluginContext* context, uint64_t now) {
	g_refreshAt = now + RefreshMs;
	const std::string menu = Build(context);
	const bool changed = menu != g_menu;
	g_menu = menu;
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
	if (changed || rows != g_beltRows) Census(context);
	g_beltRows = rows;
	auto place = MenuCursor::Settle(g_targets, g_place);
	if (!place.widget) place = MenuCursor::First(g_targets);
	const bool holding = InventoryCursor::HoldingItem(context).value_or(false);
	if (place != g_place || holding != g_holding || redrawn) (void)Land(context, now, place);
}

void Step(const D2RL::PluginContext* context, uint64_t now, Direction direction) {
	if (!g_engaged) {
		g_engaged = true;
		AsyncLog::Info(context, "[cursor] taken by an arrow key");
		Refresh(context, now);
		return; // the first press says where the cursor is
	}
	Refresh(context, now);
	if (!g_place.widget) return;
	const bool holding = InventoryCursor::HoldingItem(context).value_or(false);
	const auto next = MenuCursor::Move(g_targets, g_place, direction, holding);
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
	const bool shift = Down(VK_SHIFT), control = Down(VK_CONTROL);
	const auto* entry = g_engaged ? EntryOf(g_place.widget) : nullptr;
	const auto rect = entry ? PointerRect(*entry, g_place, InventoryCursor::HoldingItem(context).value_or(false)) : std::nullopt;
	AsyncLog::InfoF(context, "[cursor] %s click%s%s on %s", button == MenuMouse::Button::Left ? "left" : "right",
		shift ? " with Shift" : "", control ? " with Ctrl" : "", entry ? entry->name.c_str() : "where the pointer is");
	if (rect) MenuMouse::ClickAt(*rect, button);
	else MenuMouse::Click(button);
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

void Do(const D2RL::PluginContext* context, uint64_t now, Act act) {
	switch (act) {
	case Act::ClickLeft:    Click(context, now, MenuMouse::Button::Left); break;
	case Act::ClickRight:   Click(context, now, MenuMouse::Button::Right); break;
	case Act::TabPrevious:  StepTab(context, now, -1); break;
	case Act::TabNext:      StepTab(context, now, 1); break;
	case Act::MenuPrevious: StepMenu(context, now, -1); break;
	case Act::MenuNext:     StepMenu(context, now, 1); break;
	case Act::SayAgain:     SayAgain(context, now); break;
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
		const bool mine = key.with == With::ShiftOrControl || (key.with == With::Shift && !control)
			|| (!shift && !control && !KeyboardService::SettingsOpen());
		if (pressed && !typing && !alt && mine) {
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

void Reset() {
	Release(nullptr, "reset");
	MenuMouse::Reset();
	g_entries.clear();
	g_targets.clear();
	g_active = false;
	g_beltShown = false;
	g_beltRows = 0;
	g_pending = 0;
	for (auto& arrow : g_arrows) { arrow.down = false; arrow.next = 0; }
	for (auto& key : g_watched) { key.down = key.waiting = false; }
}

void Tick(const D2RL::PluginContext* context) {
	const uint64_t now = GetTickCount64();
	MenuMouse::Tick(now);
	const bool menu = Foreground() && NativeWidgets::SupportsTree() && NativeSkillNavigation::Available()
		&& !KeyboardService::Playing();
	g_active = menu;
	if (!menu) {
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
	const bool typing = NativeSkillNavigation::TextEditing().value_or(false);
	// With no pad the menu is the keyboard's from the moment it opens; with one
	// connected it is the pad's until an arrow key is pressed.
	if (!g_engaged && !InputDirection::LeftStick().connected) {
		g_engaged = true;
		AsyncLog::Info(context, "[cursor] taken: a menu is up and no pad is connected");
		g_refreshAt = 0;
	}
	ClickWords::SetKeyboard(g_engaged);
	if (g_engaged && now >= g_refreshAt && !MenuMouse::Busy()) Refresh(context, now);

	const bool control = Down(VK_CONTROL), alt = Down(VK_MENU);
	for (auto& arrow : g_arrows) {
		const bool down = Down(arrow.key);
		bool step = false;
		if (down && !arrow.down) { step = true; arrow.next = now + RepeatDelayMs; }
		else if (down && now >= arrow.next) { step = true; arrow.next = now + RepeatMs; }
		arrow.down = down;
		if (!step || typing || alt || MenuMouse::Busy()) continue;
		if (control) InputDirection::PostLineStep(arrow.push);
		else Step(context, now, arrow.direction);
	}

	Watch(context, now, typing);
	const uint32_t pending = g_pending.exchange(0);
	for (size_t act = 0; act < Acts; ++act)
		if (pending & (1u << act)) Do(context, now, static_cast<Act>(act));

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
