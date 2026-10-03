#pragma once
#include <D2RLPlugin/api.h>

// Wiring around SkillBar: holding LT alone in play reads the LT bar, and
// releasing LT cuts the reading. `access-skill-bar on|off` toggles it.
namespace D2RAccess::SkillBarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
}
