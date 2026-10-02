#pragma once

#include "target_sight.h"

#include <D2RLPlugin/api.h>
#include <string_view>

// The friendly NPC announcer and the HUD-label sight check: lifecycle, input
// and speech plumbing around NpcProximity and TargetSight. The announcer polls
// on its own interval and stays silent while the UI owns speech.
namespace D2RAccess::EntityRadarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// Whether the entity a HUD label names is in sight, for the shared tooltip
// reader: this service already owns the world and unit reads the answer needs.
// The snapshot behind it is refreshed at most every few frames, so asking on
// every UI poll is cheap.
TargetSight::Sight LabelSight(std::string_view label);
// The nearest entity in sight that the label names, from the same snapshot.
TargetSight::Bearer LabelTarget(std::string_view label);
}
