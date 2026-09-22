#pragma once

#include "generated/panel_widgets.h"
#include "native_ui.h"
#include <span>

namespace D2RAccess::NativeFocus {
struct Match {
	const Generated::PanelWidgets* panel = nullptr;
	const Generated::WidgetEntry* widget = nullptr;
	uintptr_t nativeWidget = 0;
};
// Resolve the actual focused object's ancestry against the layout vocabulary.
// Screen geometry and the timing of panel-open messages cannot change identity.
Match Resolve(NativeUi::Focus focus, std::span<const Generated::PanelWidgets> catalog);
// The CharacterSelect list keeps keyboard selection in its Selection sibling
// while native focus remains on ListItem0. Returns the uniquely highlighted clone.
uintptr_t SelectedCharacterRow(uintptr_t focusedRow);
}
