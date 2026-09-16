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
ItemPresence EquipmentItemPresence(uintptr_t widget);
int EquipmentLocation(uintptr_t widget);
std::string SelectedTabText(uintptr_t widget);
std::vector<uintptr_t> Panels();
// Bounded discovery, including hidden controls. Visibility is rechecked when read.
std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind kind);
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t focus);
void Reset();
}
