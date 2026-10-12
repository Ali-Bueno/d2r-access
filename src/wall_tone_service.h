#pragma once

#include "radar.h"

#include <D2RLPlugin/api.h>

// Wall tones: the optional continuous wall sounds beside the radar. Off by default.
namespace D2RAccess::WallTones {
bool Register(const D2RL::PluginContext* context);
// Feeds one poll of the world; null (menu, pause, unreadable world) silences the tones.
void Update(const Radar::World* world);
void Unregister();
// For the keyboard's settings list: the switch as it stands, and the console command's change.
bool Enabled();
void SetEnabled(bool enabled);
}
