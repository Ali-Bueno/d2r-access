#pragma once

#include <D2RLPlugin/api.h>

// Speaks the ways out of the native room the player walks into, in maze
// levels only, and on request with BACK+Y; and the exit finder, BACK+A or
// Ctrl+V, which names them by side on the right stick (exit_review.h).
// Lifecycle and speech plumbing around Rooms::Describe. Ported from upstream's
// room announcer (origin/main room_service.cpp); `docs/rooms.md` there.
namespace D2RAccess::RoomService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// Levels.txt DrlgType 1: one streamed room is one chamber.
bool Maze(int level);
}
