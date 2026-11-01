#pragma once

#include "radar.h"

#include <D2RLPlugin/api.h>

#include <cstdint>

// Making the character act without a controller or a mouse: the game's own
// function for issuing a player command, the one its built-in tests call to
// walk somewhere or use a skill on a unit, called with the same arguments.
// Nothing is sent to the game as input and no device is imitated, so a
// connected controller is untouched. docs/keyboard-actions.md
//
// Every call is refused until the functions' fingerprints have matched this
// build. "Handed over" below means the game accepted the call, not that the
// character did it: the game still refuses what its own rules refuse.
namespace D2RAccess::NativeActions {
// The game's unit types, as its commands name a target.
enum class Unit : uint32_t { Player = 0, Monster = 1, Object = 2, Missile = 3, Item = 4, Tile = 5 };
enum class Hand { Left, Right };

// `hooks` false leaves the game's code untouched: commands still work, the
// log of the game's own commands does not.
bool Initialize(const D2RL::PluginContext* context, bool hooks);
bool Available();

// UI thread only. Positions are collision subtiles, the frame of World::player.
// A move goes the way a click on the ground does: the game works out its own
// path to the cell first. `plain` hands the cell over as given instead, as the
// game's tests do; kept to tell the two apart in a live test.
bool MoveTo(Radar::Position cell, bool run = true, bool plain = false);
bool SkillAt(Hand hand, Radar::Position cell);
// A cast where the player stands: reads only the player's position, no terrain
// or enemies. The game decides town, mana and form restrictions.
bool SkillHere(Hand hand);
// `stationary` is Shift held: the game's own stand-still unit command, which
// does not walk up to the unit.
bool SkillOn(Hand hand, Unit type, uint32_t id, bool stationary = false);
// Talk to, open, take or use: the command the game sends for that kind of
// unit. The game refuses one that is too far, so the caller walks up first.
bool Interact(Unit type, uint32_t id);
// Stop where the character stands.
bool Halt();
// The character's own movement state, for the log when it will not move: its
// animation mode (unit +0xC), its path's type (path +0x50) and its position as
// the game keeps it, 16 bits of fraction. `read` is false when unreadable.
struct MoveState { bool read = false; uint32_t mode = 0, pathType = 0, x = 0, y = 0; };
MoveState ReadMoveState();
// The skill the player has selected for that hand, by skill id; -1 when it
// cannot be read. The skill commands above use it: they carry no skill of
// their own.
int SelectedSkill(Hand hand);

// One log line per command the game itself issues, rate limited: what the
// player's own play sends is the evidence these calls are built from.
void SetLogging(bool enabled);
bool Logging();
}
