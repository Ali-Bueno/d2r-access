#pragma once

#include "entity_radar.h"
#include <D2RLPlugin/api.h>

// Wiring around Beacon: the right-stick click locks whatever the entity scan
// last named, and a ping then leads the player there along a walkable route.
namespace D2RAccess::BeaconService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
// The entity scan hands over what it just announced, so locking needs no
// second scan and always follows exactly what the player heard.
void Offer(const EntityRadar::Entity& entity, int level, uint64_t now);
}
