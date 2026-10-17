#pragma once

#include "ui_text.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::NativeUi {
struct Focus { uintptr_t panel = 0, widget = 0; };
struct Cell { int x = -1, y = -1; };
struct Rect {
	int x = 0, y = 0, width = 0, height = 0;
	bool operator==(const Rect&) const = default;
};
enum class ItemPresence { Unknown, Empty, Occupied };
struct Type {
	std::string name;
	UiText::Kind kind = UiText::Kind::Other;
	UiText::Surface surface = UiText::Surface::Other;
};

// UI thread only. All object addresses are re-resolved every poll.
Type Classify(uintptr_t widget);
// Current text in a non-masked native input; empty when unavailable.
std::string InputText(uintptr_t widget);
bool Visible(uintptr_t widget);
uintptr_t Parent(uintptr_t widget);
std::optional<Rect> LocalRect(uintptr_t widget);
std::vector<uintptr_t> Children(uintptr_t widget);
Focus CurrentFocus();
// The game's own focus, whatever the keyboard's cursor is on: for a reader
// that asks whether the game itself gave a panel anything to stand on.
Focus GameFocus();
// The keyboard's cursor, standing in for the focus the game does not keep in
// keyboard mode (MEASURED 2026-09-27, docs/keyboard-play.md). While it is set
// and shown CurrentFocus gives it, and FocusSelection gives `selection` for
// its widget, so every reader reads where the keyboard is as it reads where
// the controller is. Nothing of the game's is written.
void SetCursor(Focus focus, uint64_t selection);
void ClearCursor();
// Read-only FocusableWidget selection; safe to copy during tooltip rendering.
uint64_t FocusSelection(uintptr_t widget);
// The open tab of a tab bar and how many tabs it shows; -1 and 0 when unreadable.
int SelectedTab(uintptr_t widget);
int TabCount(uintptr_t widget);
Cell GridCell(uintptr_t widget);
ItemPresence GridItemPresence(uintptr_t widget);
// The item on the grid cell the controller is on, or in the equipment slot,
// from the same getters as the presence reads; 0 when there is none or it
// could not be read. The unit is only named, never kept.
uintptr_t GridItem(uintptr_t widget);
uintptr_t EquipmentItem(uintptr_t widget);
// Any cell of an inventory grid, and the grid's size in cells (x = width,
// y = height; -1 when unreadable), for the inventory census log. `known` is
// false when the cell could not be read.
struct GridSlot { bool known = false; uintptr_t item = 0; };
GridSlot GridItemAt(uintptr_t widget, Cell cell);
Cell GridSize(uintptr_t widget);
ItemPresence EquipmentItemPresence(uintptr_t widget);
// The stack count drawn on a gem, rune or material stash slot, and whether the slot shows it
// (the game hides an empty slot's); empty text when the widget is not such a slot or it cannot be read.
struct StashSlotCount { std::string text; bool shown = false; bool slot = false; }; // slot: the widget is a stash slot
StashSlotCount StashSlotStackCount(uintptr_t widget);
// The belt slot the controller is on and the item in it, read straight from
// the belt rather than waiting for the game's tooltip, which the controller
// layout shows 400 ms late (Controller/TooltipsPanelHD.json
// showDelayMilliseconds). `known` is false when the slot could not be read;
// `item` is 0 for an empty slot. The unit is only named, never kept.
struct BeltSlot { bool known = false; int slot = -1; uintptr_t item = 0; };
BeltSlot BeltItem(uintptr_t widget);
// BeltWidget's inventory getter accepts slots 0..15 (0x3883F2): four columns of four rows.
inline constexpr int BeltSlots = 16;
// Same read-only getter, for a known belt cell rather than the selected one.
BeltSlot BeltItemAt(uintptr_t widget, int slot);
int EquipmentLocation(uintptr_t widget);
std::string SelectedTabText(uintptr_t widget);
// The tab a controller highlight is on in a tab bar, which moving along the bar
// changes without opening it: the label at the FocusableWidget hover index
// (+0x544), not the open tab's (+0x16C4). Empty when there is none.
// A tab's name. The skill tree's "Tab<n>" carries "Character (A) (C)" as its
// own text in the mouse layout (MEASURED 2026-09-28); its tree's name is the
// "TextTab<n>" drawn beside it, "Defensive Auras". Any other tab: its own text.
std::string TabLabel(uintptr_t widget);
std::string FocusedTabText(uintptr_t widget);
std::vector<uintptr_t> Panels();
// Bounded discovery, including hidden controls. Visibility is rechecked when read.
std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind kind);
// The rows of a ListWidget, in order: every child of the list's own row
// container, where the game writes them, shown or not. MEASURED 2026-09-22:
// the Advanced Stats rows never reach Snapshot. Empty when `list` has no row
// container.
std::vector<uintptr_t> ListRows(uintptr_t list);
// One line for the log: the list's type, its row container and how many
// children it holds, and the first child's name and visible flag.
std::string DescribeList(uintptr_t list);
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t focus);
// Words a native reader knows of the focused control beyond the tree (Node::state),
// from the panel's name and widget and the control's name; null for none.
using FocusState = std::string (*)(std::string_view panelName, uintptr_t panel, std::string_view control);
void SetFocusState(FocusState reader);
void Reset();
}
