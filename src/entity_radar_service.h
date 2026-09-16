#pragma once

#include <D2RLPlugin/api.h>

// Right-stick entity scan plus the friendly NPC announcer: lifecycle, input
// and speech plumbing around EntityRadar and NpcProximity. The scan reads the
// world only on a push or console command; the announcer polls on its own
// interval and stays silent while the UI owns speech.
namespace D2RAccess::EntityRadarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
}
