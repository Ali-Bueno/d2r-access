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
// What the scanner names is followed as it is named (the user, 2026-11-04: optional).
bool FollowsScan();
void SetFollowsScan(bool enabled);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
void SetFullAssistance(bool enabled);
bool Sounds();
void SetSounds(bool enabled);
// The next leg said at each turn of a route asked for (docs/route-description.md).
bool RoutePrompts();
void SetRoutePrompts(bool enabled);
// A route asked for says its whole length before its legs (a tester, 2026-10-20: optional).
bool RouteTotal();
void SetRouteTotal(bool enabled);
// ... and its legs (the user, 2026-10-20: optional; the turn prompts have their own switch).
bool RouteLegsSaid();
void SetRouteLegsSaid(bool enabled);
// No turn sooner than RouteLegs::MinLegUnits after the last (a tester, 2026-10-22: to try).
bool RouteLongLegs();
void SetRouteLongLegs(bool enabled);
}
