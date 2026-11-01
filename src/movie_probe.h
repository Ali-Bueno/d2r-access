#pragma once

#include <D2RLPlugin/api.h>

// Diagnostic: logs where the MoviePlayerWidget keeps the playing file's name and its clock, which
// audio description needs in play (docs/cinematics-audio-description.md). UI thread only.
namespace D2RAccess::MovieProbe {
void Started();
void Ended();
void Tick(const D2RL::PluginContext* context);
}
