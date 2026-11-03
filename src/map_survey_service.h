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
// Says the object the scanner stands on again, as the backslash key does.
void Repeat();
// The filter for what cannot be reached, Ctrl+U: on, the scanner lists only
// what the player can get to on foot from where they stand, as far as the game
// has shown the level (KeyboardService::CutOff). Off until switched on.
bool ReachFilter();
void SetReachFilter(bool on);
// Every town NPC listed and led to from anywhere in town, seen or not; on by
// default (the user, 2026-10-12). Off keeps the 2026-09-28 rule: only those seen.
bool TownNpcsEverywhere();
void SetTownNpcsEverywhere(bool on);
bool UnexploredSpread();
void SetUnexploredSpread(bool on);
// Every unexplored area of the level listed in exploration mode, not only those at the
// edge of known ground; on by default (a tester, 2026-10-25; the user, 2026-10-26). docs/entity-scan.md
bool UnexploredEverywhere();
void SetUnexploredEverywhere(bool on);
// A teleport pad says whether it leads where the player has stood.
bool PadLeads();
void SetPadLeads(bool on);
// An unexplored room past teleport pads is as far as the way through them.
bool PadSteps();
void SetPadSteps(bool on);
// Doorways listed with the doors, on by default (docs/ground-items.md#doorways-2026-10-20).
bool DoorwaysListed();
void SetDoorwaysListed(bool on);

}
