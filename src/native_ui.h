#pragma once

#include "ui_text.h"
#include <string>
#include <vector>

namespace D2RAccess::NativeUi {
struct Focus { uintptr_t panel = 0, widget = 0; };
struct Cell { int x = -1, y = -1; };
enum class ItemPresence { Unknown, Empty, Occupied };
struct Type {
	std::string name;
	UiText::Kind kind = UiText::Kind::Other;
	UiText::Surface surface = UiText::Surface::Other;
};

// UI thread only. All object addresses are re-resolved every poll.
Type Classify(uintptr_t widget);
bool Visible(uintptr_t widget);
uintptr_t Parent(uintptr_t widget);
Focus CurrentFocus();
// Read-only FocusableWidget selection; safe to copy during tooltip rendering.
uint64_t FocusSelection(uintptr_t widget);
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
// The belt slot the controller is on and the item in it, read straight from
// the belt rather than waiting for the game's tooltip, which the controller
// layout shows 400 ms late (Controller/TooltipsPanelHD.json
// showDelayMilliseconds). `known` is false when the slot could not be read;
// `item` is 0 for an empty slot. The unit is only named, never kept.
struct BeltSlot { bool known = false; int slot = -1; uintptr_t item = 0; };
BeltSlot BeltItem(uintptr_t widget);
int EquipmentLocation(uintptr_t widget);
std::string SelectedTabText(uintptr_t widget);
// The tab a controller highlight is on in a tab bar, which moving along the bar
// changes without opening it: the label at the FocusableWidget hover index
// (+0x544), not the open tab's (+0x16C4). Empty when there is none.
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
void Reset();
}
