#pragma once

#include <D2RLPlugin/api.h>

// UI-thread adapter: stock quest alerts and SDK primary-goal events feed a
// testable reader. Missing/stale panel text falls back to a generic alert.
namespace D2RAccess::QuestService {
bool Register(const D2RL::PluginContext* context);
void Unregister();
void Note(const char* command);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Reset();
}