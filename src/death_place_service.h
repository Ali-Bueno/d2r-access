#pragma once

#include "entity_radar.h"
#include "radar.h"

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <vector>

// Remembers where the player last died on each map, on disk, and gives the
// level scanner the way back: the place itself on its level, else the known
// exit (or waypoint) toward it. `docs/death-place.md`
namespace D2RAccess::DeathPlaceService {

void Register(const D2RL::PluginContext* context);
// The death screen opened. Any thread.
void NoteDeath();
void Unregister();
// Called with every fresh world read: records a pending death at the last
// place the player stood, and forgets the death place once walked back to.
void Track(uint32_t seed, int level, Radar::Position player);
// The scanner's entries for the map `seed`, in the corpse category.
// `here` is the current level's catalogue, whose exits and waypoint are used.
std::vector<EntityRadar::Entity> Landmarks(uint32_t seed, int level, Radar::Position player,
	const std::vector<EntityRadar::Entity>& here);

}
