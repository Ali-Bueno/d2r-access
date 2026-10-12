#pragma once

#include <cstdint>

namespace D2RL {
struct PluginContext;
}

// A face button pressed with BACK held is one of the mod's BACK readouts, and
// the SDK's input actions bind keyboard keys only, so the one way to stop the
// game also acting on it is to take the press out of what the game reads.
//
// This replaces pointers, not code: the game image's own slot for
// XInputGetState, which the game's pad poll jumps through. The mod reads its
// pad through a pointer of its own (input_direction.cpp), so it still sees
// every press. The replacement calls the original and hands the game exactly
// what came back, with those presses cleared (and B while the skill line
// reader is open, and the left stick while Y is held on a compare screen).
// The pad is never reported absent, idle or on another slot, and no slot is
// ever occupied: every other button, stick and trigger passes through byte for
// byte (CRITICAL RULE 3). R3 and L3 are the game's (weapon swap, run/walk):
// the beacon is BACK+LB since 2026-10-11, so LB with BACK is kept too, and
// RB with BACK since 2026-10-12 (the settings list).
namespace D2RAccess::PadFilter {

// Swaps the slot. When the game has not filled it yet, Claim looks
// again for a while; either way the log says what happened. Safe to call
// twice.
bool Install(const D2RL::PluginContext* context);
void Remove();

// Whether the mod owns its BACK combinations right now: during play, and not
// in a menu, where the game uses those buttons itself. Set from the UI tick.
// Also logs, once per press, what was kept from the game.
void Claim(const D2RL::PluginContext* context, bool claimed);

// One button's press as the filter has seen it.
struct Press {
	bool down = false;
	bool kept = false; // this press is being kept from the game
};
// The presses the filter decides on, per pad.
struct Presses { Press a, b, x, y, lb, rb, back; };

// Pure: the buttons the game is handed. What the mod owns during play: the
// face buttons and the bumpers pressed while BACK is held, the mod's BACK+A,
// B, X, Y, LB and RB actions (input_direction.h). A press is decided on its first frame
// and kept to its release, so one that began in a menu reaches the game whole
// and one that began in play never reaches it at all, whatever the claim does
// while it is held.
//
// `keepBack` keeps BACK itself from the game during play, where the game only
// opens and zooms its automap with it (a tester: every mod readout toggled the
// map); in a menu BACK is the game's. `holdB` keeps a B press from the game in
// a menu too, while a mod reader that closes on B is open (HoldB below),
// decided on the press's first frame like the rest.
uint16_t Filter(uint16_t buttons, bool claimed, Presses& presses, bool holdB = false, bool keepBack = false);

// The switch behind `keepBack`, on by default; the keyboard's Tab still opens the automap.
bool KeepBack();
void SetKeepBack(bool keep);

// Keeps B from the game while the skill line reader is open, so closing it
// with B does not also close the skill tree (the user, 2026-09-21: "right
// stick left should close the buffer, or the b button"). Call on every tick
// the reader is open; the hold lapses by itself HoldBMs after the last call,
// so a reader that stops ticking can never leave B held.
inline constexpr uint64_t HoldBMs = 500;
void HoldB(uint64_t now);
void ReleaseB();

// Keeps the left stick from the game while Y is held on a screen whose held-Y
// reader it drives (an inventory item, the hire list), so moving it reads
// rather than moving the cursor off the thing being read. Y itself still
// reaches the game, which needs it to draw what is read. Call on every tick
// such a screen has focus; it lapses HoldBMs after the last call, and while
// armed it takes the stick only in a frame where Y is down.
void ArmLeftStickWithY(uint64_t now);
// Pure: whether a frame's left stick is kept from the game.
bool KeepsLeftStick(uint16_t buttons, bool armed);

}
