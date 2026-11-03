#pragma once

#include "entity_radar.h"
#include "pad_links.h"
#include "radar.h"

#include <D2RLPlugin/api.h>

#include <vector>

namespace D2RAccess::LevelRoute { class Memory; }

// Keyboard play: the keys that walk, fight, retreat and explore, and the keys
// that stand in for the pad's reading gestures. The character acts through the
// game's own player command (native_actions.h), never through an imitated
// controller, so a pad that is connected keeps working untouched.
//
// In play:
//   arrows            walk while held
//   A / W             fight with the left / right skill while held
//                     (in a menu: left click / right click, keyboard_menu.h)
//   Tab, Shift+Tab    next / previous monster
//   D                 walk up to the nearest thing and use it
//   Shift+Home        walk to what the scanner named
//   H                 walk to the nearest unexplored ground
//   Ctrl+R            walk back to the way the player came into the level
//   End               stop
//   Ctrl+Y            the ways out of this room
// Reading, everywhere (the pad's right stick):
//   PageUp, PageDown            previous / next: objects in play, lines in a menu
//   Ctrl+PageUp, Ctrl+PageDown  categories in play; first / last, or aside, in a menu
//   Shift+PageUp, Shift+PageDown  compare: the item in full / the one equipped
//   Home                        say it again
// In a menu PageUp, PageDown, Home and End are the menu's (keyboard_menu.h):
// the tab before and after, the menu before and after. Ctrl with an arrow key
// reads line by line there.
// Keys that press a button of the open menu for the player:
//   Ctrl+R            repair all, at a merchant who repairs
//   Shift+R           repair, the same
//   Alt+T             transmute, in the Horadric Cube
//   Alt+S             sort the grid the cursor is in (native_sort.h)
// Settings, everywhere:
//   Ctrl+S            open or close the list of the mod's switches
//   PageUp, PageDown  previous / next switch while it is open
//   Space             change the switch
namespace D2RAccess::KeyboardService {
bool Register(const D2RL::PluginContext* context, bool nativeHooks);
// Right after InputDirection::Poll: what it posts is for this update's readers.
void Tick(const D2RL::PluginContext* context);
// UI callback recovery: drop pending actions/selection and stop mod movement.
// Does not allocate, speak, or discard learned map/exploration state.
void CancelAfterFault() noexcept;
void Unregister();
// In a game and in play, as of the last update: the keys act on the world.
bool Playing();
// The list of the mod's switches is open and steps with PageUp and PageDown.
bool SettingsOpen();
// For each of `things`, whether the game has shown that it cannot be got to on
// foot from where `world` has the player (level_route.h, Reach): the walk's own
// test, here for the scanner's filter. All false when the level's map is not kept.
std::vector<bool> CutOff(const Radar::World& world, const std::vector<const EntityRadar::Entity*>& things);
// The teleport pads among `entities`, read in the level `world` is of, are kept
// with that level's map, to work out where each leads (pad_links.h).
void NotePads(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities);
// Whether the teleport pad `pad` leads where the player has stood (Pads::Leads).
LevelRoute::Destination PadLeads(const Radar::World& world, const EntityRadar::Entity& pad);
// For each of `rooms` (scanner room entries), the steps of the way to it on foot
// and through the teleport pads; -1 when unknown, and all of them in a level
// with no pad whose way is known.
std::vector<int> PadSteps(const Radar::World& world, const std::vector<const EntityRadar::Entity*>& rooms);
// The shut doors among `entities` into the level's kept map, so a way passes one whose cells the
// game marks as an object's, not a door's (LevelRoute::Memory::NoteDoor).
void NoteDoors(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities);
// The map of the level `world` is of that the walks plan on (level_route.h),
// learnt from `world` first, for the beacon to plan on too. Null when it is not
// kept. Valid until the next call into this service.
const LevelRoute::Memory* RememberedMap(const Radar::World& world);
}
