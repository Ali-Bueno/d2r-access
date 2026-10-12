#pragma once

#include "radar.h"

#include <D2RLPlugin/api.h>
#include <array>

// The four always-playing wall tone voices (XAudio2, synthesized loops). The
// worker glides each voice toward its target and treats stale targets as silence.
namespace D2RAccess::WallToneAudio {
using Gains = std::array<float, Radar::DirectionCount>;
bool Start(const D2RL::PluginContext* context);
// Target proximity gain per direction, 0..1 (WallTones::Gain), by Radar::Direction.
void Update(const Gains& gains);
// User level 0..1 applied on top of the gains.
void SetLevel(float level);
void Silence();
void Stop();
}
