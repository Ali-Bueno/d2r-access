#pragma once

#include "ui_text.h"
#include <optional>
#include <string>
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
// Read-only FocusableWidget selection; safe to copy during tooltip rendering.
uint64_t FocusSelection(uintptr_t widget);
Cell GridCell(uintptr_t widget);
// The item under the controller's selection, read straight from the widget
// rather than waiting for the game's tooltip, which the controller layout shows
// 400 ms late (Controller/TooltipsPanelHD.json showDelayMilliseconds).
// BeltWidget, InventoryGridWidget and InventorySlotWidget share one getter and
// differ only in the cell it takes. `known` is false when the cell could not be
// read; `item` is 0 for an empty one; `slot` is the column a belt slot is
// numbered by. The unit is only named, never kept.
struct ItemSlot { bool known = false; int slot = -1; uintptr_t item = 0; };
ItemSlot FocusedItem(uintptr_t widget);
// Same read-only getter, for a known belt cell rather than the selected one.
ItemSlot BeltItem(uintptr_t widget, int slot);
constexpr ItemPresence Presence(const ItemSlot& slot) {
	return !slot.known ? ItemPresence::Unknown : slot.item ? ItemPresence::Occupied : ItemPresence::Empty;
}
int EquipmentLocation(uintptr_t widget);
std::string SelectedTabText(uintptr_t widget);
std::vector<uintptr_t> Panels();
// Bounded discovery, including hidden controls. Visibility is rechecked when read.
std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind kind);
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t focus);
void Reset();
}
