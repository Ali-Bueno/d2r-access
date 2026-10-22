#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::EntityRadar { struct Entity; }

// The new character's tutorial: lifecycle, signals, speech and its message history (docs/tutorial.md).
namespace D2RAccess::TutorialService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
bool Enabled();
void SetEnabled(bool on);
// tutorial-sound-wait: a lesson's sounds wait for the player after each one.
bool WaitForSounds();
void SetWaitForSounds(bool on);

// What the scanner named, the route described and the beacon reached, from the UI thread.
void Scanned(const EntityRadar::Entity& entity);
void Routed(const EntityRadar::Entity& entity);
void Arrived(const EntityRadar::Entity& entity);
}
