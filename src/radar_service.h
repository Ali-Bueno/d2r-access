#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::RadarService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
bool Diagonal();
void SetDiagonal(bool diagonal);
// The thud of pushing into a wall, and the small obstacle's own knock.
bool BumpEnabled();
void SetBumpEnabled(bool enabled);
// How often the radar's wall ahead repeats while the player heads into it: a
// Radar::WallRepeatMs level, 0 once per change.
int WallRepeatLevel();
void SetWallRepeatLevel(int level);
// How often the thud repeats while the push goes on: a WallBump::LoopMs level, 0 once.
int BumpLoopLevel();
void SetBumpLoopLevel(int level);
bool SmallObstaclesEnabled();
void SetSmallObstaclesEnabled(bool enabled);
}
