#pragma once

#include <D2RLPlugin/api.h>

// Speaks the ways out of the native room the player walks into, in maze
// levels only, and on request with BACK+Y (Ctrl+Y). Lifecycle and speech
// plumbing around Rooms::Describe; `docs/rooms.md`.
namespace D2RAccess::RoomService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// Levels.txt DrlgType 1: one streamed room is one chamber.
bool Maze(int level);
// Levels.txt DrlgType 3: open ground, where the frontier of the streamed
// terrain is what "unexplored" means.
bool Outdoor(int level);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
// Ways out told by the map's sides (the room's walls, NE/SE/SW/NW) instead of
// the screen's (room_sides.h). On by default. docs/rooms.md
bool MapSides();
void SetMapSides(bool enabled);
}
