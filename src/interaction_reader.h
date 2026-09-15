#pragma once

#include "native_ui.h"
#include <D2RLPlugin/api.h>
#include <string>

namespace D2RAccess::InteractionReader {
bool IsInventoryFocus(NativeUi::Focus focus);
bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, bool conversation, std::string_view tabChange = {});
void Reset();
std::string Repeat();
}
