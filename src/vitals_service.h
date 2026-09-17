#pragma once

#include <D2RLPlugin/api.h>

// Speaks the player's life and mana as they cross the announcement steps, and
// says when a worn item breaks. Console command: access-vitals on|off|status.
namespace D2RAccess::VitalsService {

bool Register(const D2RL::PluginContext* context);
void Unregister();
void Tick(const D2RL::PluginContext* context);
void Reset();

}
