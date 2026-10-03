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
std::string ItemName(uint32_t id, uint32_t classId, uint32_t quality);
std::string Repeat();
// Whether each item tooltip is written to the log ([item]). Off by default --
// logged on every poll it filled a session's log to 12 MB (the user,
// 2026-09-22) -- and once per change when on. Console: access-itemlog on|off.
void SetItemLogging(bool enabled);
bool ItemLogging();
}
