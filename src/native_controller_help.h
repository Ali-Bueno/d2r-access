#pragma once
#include "native_ui.h"

namespace D2RAccess::NativeControllerHelp {
std::string Legend(uintptr_t widget);
std::string ForFocus(NativeUi::Focus focus);
void Reset();
}
