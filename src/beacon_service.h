#pragma once

#include "entity_radar.h"
#include <D2RLPlugin/api.h>

#include <string>

// Wiring around Beacon: the left-stick click locks whatever the level scanner
// last named, and a ping then leads the player there along a walkable route.
namespace D2RAccess::BeaconService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
// The scanner hands over what it just announced, so locking needs no second
// read and always follows exactly what the player heard.
void Offer(const EntityRadar::Entity& entity, int level, uint64_t now);
// What the scanner named last on this level, for a walk there; false when nothing was.
bool Offered(EntityRadar::Entity& entity, int level);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
void SetFullAssistance(bool enabled);
bool Sounds();
void SetSounds(bool enabled);
}
