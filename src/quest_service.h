#pragma once

#include <D2RLPlugin/api.h>

// UI-thread adapter: stock quest alerts and SDK primary-goal events feed a
// testable reader. Missing/stale panel text falls back to a generic alert.
namespace D2RAccess::QuestService {
// nativeHooks false ([native] enabled = false) leaves out the quest-text code hook.
bool Register(const D2RL::PluginContext* context, bool nativeHooks = true);
void Unregister();
void Note(const char* command);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Reset();
}