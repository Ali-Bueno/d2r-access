#pragma once

#include <D2RLPlugin/api.h>

// Speaks the ways out of the native room the player walks into, in maze
// levels only. Lifecycle and speech plumbing around Rooms::Describe.
namespace D2RAccess::RoomService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
}
