#pragma once

#include "native_ui.h"
#include <D2RLPlugin/api.h>
#include <string>

namespace D2RAccess::InteractionReader {
bool HandlesFocus(NativeUi::Focus focus);
// Hold `detail` for the right-stick-up gesture, against `owner`. Moving to a
// different owner drops what the previous one offered, so the gesture can
// never answer about the thing the player has just left. An empty detail
// under a new owner simply clears it.
void OfferDetail(std::string_view owner, std::string detail);
bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, bool conversation, std::string_view tabChange = {});
void Reset();
std::string Repeat();
}
