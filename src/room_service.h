#pragma once

#include <D2RLPlugin/api.h>

// Speaks the ways out of the native room the player walks into, in maze
// levels only. Lifecycle and speech plumbing around Rooms::Describe.
namespace D2RAccess::RoomService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// Levels.txt DrlgType 1: one streamed room is one chamber.
bool Maze(int level);
// Levels.txt DrlgType 3: open ground, where the frontier of the streamed
// terrain is what "unexplored" means.
bool Outdoor(int level);
}
