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
// Null when the native text-editing state cannot be verified.
std::optional<bool> TextEditing();
// The text box the game is typing into when it belongs to `panel`, else 0.
uintptr_t ActiveTextInput(uintptr_t panel);
// Temporarily stop the game's analog cursor update during a D-pad gesture.
void HoldCursor(bool hold);
}
