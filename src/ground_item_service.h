#pragma once
#include <D2RLPlugin/api.h>
#include <string_view>

namespace D2RAccess::GroundItemService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// The shared tooltip reader defers only matching nearby ground labels, so the
// proximity service announces them once. Inventory/NPC speech is unchanged.
bool OwnsWorldLabel(std::string_view label);
// The game's world target, as the reader saw it this update: when it newly names a
// ground item (its pickup prompt is up), one click (switch `pickup-click`).
void NoteWorldTarget(std::string_view label);
bool PickupClick();
void SetPickupClick(bool enabled);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
enum class Switch { Speech, Sounds, Exits, Chests, Doors, Quests, Stash };
bool Enabled(Switch which);
void SetEnabled(Switch which, bool enabled);
}
