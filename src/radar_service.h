#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::RadarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
}
