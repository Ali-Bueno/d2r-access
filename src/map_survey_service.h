#pragma once

#include <D2RLPlugin/api.h>

// Live wiring for the level-wide catalogue (map_survey.h): keeps the registry
// fed from the world, and answers the cycling keys.
//
//   \        repeat the current object
//   [ / ]    previous / next object inside the current category
//   , / .    previous / next category
//
// The brackets are shared with menu navigation, which owns their registration:
// while a panel is focused they change tab, and only otherwise do they reach
// here. One plugin action per key, because two actions on the same key would
// conflict in the game's own Controls menu.
namespace D2RAccess::MapSurveyService {

bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Shutdown();

// Called by menu navigation when the brackets are pressed with no panel open.
// True while the long-range pathfinder is on (L3). It owns the right stick
// in that state, which is why this service ticks before the entity scan.
bool Active();

void CycleObject(int delta);

}
