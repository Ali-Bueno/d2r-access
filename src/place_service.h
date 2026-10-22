#pragma once

#include "doorways.h"
#include "entity_radar.h"

#include <D2RLPlugin/api.h>
#include <vector>

// Speaks the named place the player walks into: a named map piece, an
// interior, the landmark in reach. Lifecycle and speech plumbing around
// Places. docs/places.md
namespace D2RAccess::PlaceService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
bool Enabled();
void SetEnabled(bool on);
// The tracked landmarks (Places::Tracked) of the maps of the loaded rooms, as
// scanner entities with no unit. Game thread; empty when the maps are unreadable.
std::vector<EntityRadar::Entity> MapLandmarks();
// The loaded map pieces that are one building (Places::Separate: a cottage, the Tower's
// ruin), where doorways are listed and creak. Game thread.
std::vector<Doorways::Box> Buildings();
}
