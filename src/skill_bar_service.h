#pragma once
#include <D2RLPlugin/api.h>

// Wiring around SkillBar: holding LT alone in play reads the LT bar, and
// releasing LT cuts the reading. `access-skill-bar on|off` toggles it.
namespace D2RAccess::SkillBarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
}
