#pragma once

#include "entity_radar.h"
#include <D2RLPlugin/api.h>

#include <string>

// Wiring around Beacon: the right-stick click locks whatever the level scanner
// last named, and a ping then leads the player there along a walkable route.
namespace D2RAccess::BeaconService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
// The scanner hands over what it just announced, so locking needs no second
// read and always follows exactly what the player heard.
void Offer(const EntityRadar::Entity& entity, int level, uint64_t now);
// A way out with no unit behind it (a passage between two streamed rooms): the
// beacon leads to the point itself and never tries to re-locate it.
void OfferPoint(Radar::Position point, const std::string& label, int level, uint64_t now);
}
