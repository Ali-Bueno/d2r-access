#pragma once

#include <D2RLPlugin/api.h>

// Live wiring for the level scanner (map_survey.h): keeps the registry fed
// from the world and answers the right stick during play -- up/down through
// categories, left/right through the objects in one. Keys: \ repeats, , and .
// step categories, Ctrl+Numpad 8/2/4/6 mirror the stick.
namespace D2RAccess::MapSurveyService {

bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Shutdown();

}
