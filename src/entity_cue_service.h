#pragma once

#include <D2RLPlugin/api.h>
#include <optional>
#include <vector>
#include "entity_cues.h"

namespace D2RAccess::EntityCueService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
// How far the player's attacks reach, in subtiles; nothing outside a game or before the first read.
std::optional<double> CurrentReach();
// How far a monster the HUD plate names may stand and still be named: the melee
// reach. Nothing for a ranged attack, whose shot reaches what the auto-target picks.
std::optional<double> NamingReach();
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
enum class Cue { All, Npcs, Enemies, Ranged, Ordinary, Reach };
bool Enabled(Cue cue);
void SetEnabled(Cue cue, bool enabled);
// A bell when a town NPC's "!" appears, and after its name wherever it is said.
bool NpcAlerts();
void SetNpcAlerts(bool on);
void PlayNpcAlert();
// The town NPCs whose "!" has just appeared, nearest first, each once whoever asks; RingAlert rings one, panned to it.
std::vector<EntityCues::Cue> RaisedAlerts(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities);
void RingAlert(const EntityCues::Cue& cue);
}
