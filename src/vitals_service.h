#pragma once

#include <D2RLPlugin/api.h>

// Announces player and mercenary condition changes, mercenary level gains,
// and broken equipment. Console command: access-vitals on|off|status|report.
namespace D2RAccess::VitalsService {

bool Register(const D2RL::PluginContext* context);
void Unregister();
void Tick(const D2RL::PluginContext* context);
void Reset();

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
// Walk/run changes and falling stamina; stacks in hand running low.
bool MovementAlerts();
void SetMovementAlerts(bool enabled);
bool AmmoAlerts();
void SetAmmoAlerts(bool enabled);
// How much mana is spoken unasked, a Vitals::ManaAlerts level.
int ManaAlerts();
void SetManaAlerts(int level);
}
