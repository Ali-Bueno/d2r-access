#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::MultiplayerReader {

// UI-thread poll of the loader's visible multiplayer status and browser text.
void Initialize(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Reset();

}
