#pragma once

#include <D2RLPlugin/api.h>

// Keyboard play: the keys that walk, fight, retreat and explore, and the keys
// that stand in for the pad's reading gestures. The character acts through the
// game's own player command (native_actions.h), never through an imitated
// controller, so a pad that is connected keeps working untouched.
//
// In play:
//   arrows            walk while held
//   A / W             fight with the left / right skill while held
//   Tab, Shift+Tab    next / previous monster
//   D                 walk up to the nearest thing and use it
//   Shift+Home        walk to what the scanner named
//   H                 walk to the nearest unexplored ground
//   Ctrl+R            retreat to the nearest way out
//   Shift+R           retreat the way the player came in
//   Alt+R             retreat to the nearest corridor or doorway
//   End               stop
//   Ctrl+Y            the ways out of this room
// Reading, everywhere (the pad's right stick):
//   PageUp, PageDown            previous / next: objects in play, lines in a menu
//   Ctrl+PageUp, Ctrl+PageDown  categories in play; first / last, or aside, in a menu
//   Shift+PageUp, Shift+PageDown  compare: the item in full / the one equipped
//   Home                        say it again
// Settings, everywhere:
//   Ctrl+S            open or close the list of the mod's switches
//   PageUp, PageDown  previous / next switch while it is open
//   Space             change the switch
namespace D2RAccess::KeyboardService {
bool Register(const D2RL::PluginContext* context, bool nativeHooks);
// Right after InputDirection::Poll: what it posts is for this update's readers.
void Tick(const D2RL::PluginContext* context);
void Unregister();
}
