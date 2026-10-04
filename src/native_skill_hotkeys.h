#pragma once

#include <D2RLPlugin/api.h>
#include <vector>

// Observe successful native keyboard skill hotkeys without consuming input.
// Hook callbacks copy ids only; names and speech belong on the UI tick.
namespace D2RAccess::NativeSkillHotkeys {
struct Event { int slot = -1, skill = -1; bool left = false, quickCast = false; };
bool Initialize(const D2RL::PluginContext* context);
std::vector<Event> Take();
}
