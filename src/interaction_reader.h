#pragma once

#include "native_ui.h"
#include "stash_count.h"
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
// Out of a game, where Tick does not run: right stick up reads what the front end's
// highlight offered (the focus tracker's OfferDetail), down its controls.
void TickFrontEnd(const D2RL::PluginContext* context);
void Reset();
std::string ItemName(uint32_t id, uint32_t classId, uint32_t quality);
// The stash tab the open expansion stash shows, Unknown when it is closed or unreadable.
StashCount::GoldTab OpenStashTab(const D2RL::PluginContext* context);
std::string Repeat();
// Whether picking an item up and putting it down are spoken ("Item in hand",
// "Placed in Row 1, column 2"). A settings-list switch, on by default.
bool HandSpeech();
void SetHandSpeech(bool enabled);
// Whether the equipped item the game draws beside the highlighted one (compare,
// Y held) is read as it appears. A settings-list switch, off by default: the
// user chose compare on demand, hold Y and the left stick (2026-10-03).
bool CompareAloud();
void SetCompareAloud(bool enabled);
// Whether each item tooltip is written to the log ([item]). Off by default --
// logged on every poll it filled a session's log to 12 MB (the user,
// 2026-09-22) -- and once per change when on. Console: access-itemlog on|off.
void SetItemLogging(bool enabled);
bool ItemLogging();
}
