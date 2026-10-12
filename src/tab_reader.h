#pragma once

#include "native_ui.h"
#include <span>

namespace D2RAccess::TabReader {
// Observe visible tab bars, including bars outside the panel holding focus.
// Returns only changed tab labels, never their unselected siblings.
std::string Update(std::span<const uintptr_t> panels, NativeUi::Focus focus, bool suspended);
std::string Selected(uintptr_t panel);
// Bar name, index and text of the last change, for the log.
std::string Detail();
void Reset();
}
