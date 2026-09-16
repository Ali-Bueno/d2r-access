#pragma once

#include "generated/panel_widgets.h"
#include "native_ui.h"
#include <span>

namespace D2RAccess::NativeFocus {
struct Match {
	const Generated::PanelWidgets* panel = nullptr;
	const Generated::WidgetEntry* widget = nullptr;
};
// Resolve the actual focused object's ancestry against the layout vocabulary.
// Screen geometry and the timing of panel-open messages cannot change identity.
Match Resolve(NativeUi::Focus focus, std::span<const Generated::PanelWidgets> catalog);
}
