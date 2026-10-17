#pragma once
#include <D2RLPlugin/api.h>
#include "native_ui.h"

namespace D2RAccess::NativeSkillNavigation {
bool Initialize(const D2RL::PluginContext* context);
bool Available();
// UI thread: move the real cursor to the native focus rectangle, then select.
bool Select(NativeUi::Focus focus);
// The same for one place inside a widget that holds several, such as a cell of
// an inventory grid: `selection` is the place, as FocusSelection reads it.
bool SelectAt(NativeUi::Focus focus, uint64_t selection);
// Screen position for general/granted skill buttons whose names have no cell.
NativeUi::Cell Position(uintptr_t widget);
// The same guarded focus geometry is shared by runtime-created menus.
std::optional<NativeUi::Rect> FocusRect(uintptr_t widget);
// The rectangle of one place inside a widget that holds several, such as a
// cell of an inventory grid. With an item in hand the game gives the rectangle
// the pointer has to be in the middle of for the item to begin on that cell.
std::optional<NativeUi::Rect> RectAt(uintptr_t widget, uint64_t selection);
// Puts the game's mouse on a point of the UI and the real pointer with it; no
// widget is selected. The keyboard's menu cursor: docs/keyboard-play.md
bool PlacePointer(NativeUi::Cell point) noexcept;
// The panel a focusable widget belongs to, as the game's own focus names it; 0 when unreadable.
uintptr_t Owner(uintptr_t widget);
// Null when the native text-editing state cannot be verified.
std::optional<bool> TextEditing();
// The text box the game is typing into when it belongs to `panel`, else 0.
uintptr_t ActiveTextInput(uintptr_t panel);
// The editor that takes typed text whatever has focus, and the panel it belongs to:
// the chat box leaves the focus on its candidate list (a tester, 2026-10-14). 0 when none is shown.
uintptr_t AnyActiveTextInput(uintptr_t& owner);
// Starts the game's own editing of the focused text box, as its A does when it works:
// the box becomes the active editor and typed keys reach it. Null when it did, else why not.
const char* BeginEdit(uintptr_t box);
// Temporarily stop the game's analog cursor update during a D-pad gesture.
void HoldCursor(bool hold);
}
