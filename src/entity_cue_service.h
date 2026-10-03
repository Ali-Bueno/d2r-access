#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::EntityCueService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
enum class Cue { All, Npcs, Enemies, Ranged, Ordinary, Reach };
bool Enabled(Cue cue);
void SetEnabled(Cue cue, bool enabled);
}
