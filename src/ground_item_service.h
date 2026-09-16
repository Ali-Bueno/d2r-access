#pragma once
#include <D2RLPlugin/api.h>
#include <string_view>

namespace D2RAccess::GroundItemService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// The shared tooltip reader defers only matching nearby ground labels, so the
// proximity service announces them once. Inventory/NPC speech is unchanged.
bool OwnsWorldLabel(std::string_view label);
}
