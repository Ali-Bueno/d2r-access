#pragma once

#include <D2RLPlugin/api.h>

// Right-stick entity scan: lifecycle, input and speech plumbing around
// EntityRadar. Only reads the world when a push or console command arrives.
namespace D2RAccess::EntityRadarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
}
