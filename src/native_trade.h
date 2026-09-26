#pragma once
#include "trade.h"
#include <D2RLPlugin/api.h>
#include <optional>
#include <string>

// Player-to-player trade state of D2R 3.3.93787, read-only. See docs/trade.md.
namespace D2RAccess::NativeTrade {
bool Initialize(const D2RL::PluginContext* context);
// Hooks the "trade completed" reply so a finished trade can be told from a
// cancelled one. Needs the [native] code-hook switch.
bool InstallCompletionHook(const D2RL::PluginContext* context);
bool CompletionHooked();

// The client's trade state, nothing when unreadable. UI thread only.
std::optional<Trade::State> Read() noexcept;
// The trade partner's name from the party roster, "" when unknown.
std::string PartnerName();
// True once per "trade completed" reply received since the last call.
bool TakeCompleted();
}
