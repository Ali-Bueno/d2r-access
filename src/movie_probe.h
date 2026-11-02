#pragma once

#include <D2RLPlugin/api.h>

// Diagnostic: looks for the MoviePlayerWidget's playback clock when the known one does not read, which
// audio description needs in play (docs/cinematics-audio-description.md). UI thread only.
namespace D2RAccess::MovieProbe {
void Started();
void Ended();
void Tick(const D2RL::PluginContext* context);
}
