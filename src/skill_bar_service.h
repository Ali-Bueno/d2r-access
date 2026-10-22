#pragma once
#include <D2RLPlugin/api.h>
#include <initializer_list>
#include <string>

// Wiring around SkillBar: holding LT alone in play reads the LT bar, and
// releasing LT cuts the reading. `access-skill-bar on|off` toggles it.
namespace D2RAccess::SkillBarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
// The pad button one of `skills` is bound to, as spoken ("LT X"), the main bar first; empty when
// none is or the bars cannot be read. UI thread only.
std::string ButtonOf(std::initializer_list<int> skills);
}
