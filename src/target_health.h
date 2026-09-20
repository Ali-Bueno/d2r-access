#pragma once

#include <D2RLPlugin/api.h>

// The health of the monster the player is currently targeting, on demand.
// The game draws it as a bar with no number anywhere in the widget tree, so
// nothing about it is readable by walking text boxes the way the player's own
// orbs are -- the percentage comes from the bar widget's own field.
//
// Key: F. Console: access-target (report|status).
namespace D2RAccess::TargetHealth {

bool Register(const D2RL::PluginContext* context);
void Unregister();
void Tick(const D2RL::PluginContext* context);

}
