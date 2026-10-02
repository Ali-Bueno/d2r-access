#pragma once

#include <D2RLPlugin/api.h>

// Narrates a boss's life as it falls: lifecycle, native reads and speech around BossLife.
namespace D2RAccess::BossLifeService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
}
