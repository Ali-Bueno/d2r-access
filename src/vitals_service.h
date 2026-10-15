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
// Experience within the level, said by the life report (BACK+X, Mod+X).
bool ExperienceReport();
void SetExperienceReport(bool enabled);
// Poison, freezing, chill, stun, slowing and curses as they begin and end.
bool StatusAlerts();
void SetStatusAlerts(bool enabled);
// The summons (and a Necromancer's usable corpses) in the life report.
bool MinionsReported();
void SetMinionsReported(bool enabled);
// How much mana is spoken unasked, a Vitals::ManaAlerts level.
int ManaAlerts();
void SetManaAlerts(int level);
}
