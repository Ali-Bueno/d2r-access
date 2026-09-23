#pragma once
#include <D2RLPlugin/api.h>
#include "native_ui.h"

namespace D2RAccess::NativeSkillNavigation {
bool Initialize(const D2RL::PluginContext* context);
bool Available();
// UI thread: move the real cursor to the native focus rectangle, then select.
bool Select(NativeUi::Focus focus);
// Screen position for general/granted skill buttons whose names have no cell.
NativeUi::Cell Position(uintptr_t widget);
// The same guarded focus geometry is shared by runtime-created menus.
std::optional<NativeUi::Rect> FocusRect(uintptr_t widget);
// Null when the native text-editing state cannot be verified.
std::optional<bool> TextEditing();
// Temporarily stop the game's analog cursor update during a D-pad gesture.
void HoldCursor(bool hold);
}
