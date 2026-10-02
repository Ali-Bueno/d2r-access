#pragma once

#include <D2RLPlugin/api.h>

// Speaks the named place the player walks into: a named map piece, an
// interior, the landmark in reach. Lifecycle and speech plumbing around
// Places. docs/places.md
namespace D2RAccess::PlaceService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
}
