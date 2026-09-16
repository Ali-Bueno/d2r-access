#pragma once
#include "ground_items.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::ItemAudio {
bool Start(const D2RL::PluginContext* context);
void Update(std::vector<GroundItems::Voice> voices);
void Silence();
void Stop();
bool Available();
}
