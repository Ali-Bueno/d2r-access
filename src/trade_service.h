#pragma once
#include "ui_text.h"
#include <D2RLPlugin/api.h>
#include <string>

// Wiring around Trade: what the other player offers, accepts or withdraws, and
// how the trade ends. `access-trade on|off`. See docs/trade.md.
namespace D2RAccess::TradeService {
bool Register(const D2RL::PluginContext* context, bool nativeHooks);
void Tick(const D2RL::PluginContext* context);
// Names the partner in the game's own nameless trade prompts.
void NamePrompt(UiText::Readout& readout);
// Restores the space the game drops between a name and a trade notice.
std::string FixNotice(std::string notice);
// The partner's name while a trade is pending or active, "" otherwise.
std::string PartnerName();
}
