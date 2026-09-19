#pragma once

#include "keyboard_pad.h"

#include <cstdint>

namespace D2RL {
struct PluginContext;
}

// A pass-through detour on the XInputGetState the GAME reads, so we can measure
// how it polls before anything is injected. It changes no behaviour at all: it
// counts the call, forwards to the original, and returns exactly what came
// back.
//
// What it is here to answer: **does D2R read a pad on a slot above 0?** Critical
// rule 3 forbids both merging with a physically connected controller and
// speaking over it, so a synthetic pad has to take a free slot -- which only
// works if the game looks at one. If it polls slot 0 only, the documented
// fallback is last-input-wins on slot 0 instead. That is a measurement, and
// this is the instrument.
namespace D2RAccess::XInputHook {

struct SlotStats {
	uint64_t calls     = 0; // times the game asked about this slot
	uint64_t connected = 0; // times the original answered ERROR_SUCCESS
};

// Detours the named XInputGetState in whichever XInput module the game already
// loaded -- never LoadLibrary, because a module the mod pulled in is not the
// one the game calls. Verifies the function's first 15 bytes against the
// prologue this was written for and refuses to patch anything else. Safe to
// call twice; the second call does nothing.
bool Install(const D2RL::PluginContext* context);

// Restores the original bytes. Called on shutdown, and by `access-pad off`.
void Remove(const D2RL::PluginContext* context);

// Logs the per-slot counts. Called on a timer for the first half minute and by
// `access-pad`, so a single launch answers the question without a command.
void Report(const D2RL::PluginContext* context);

// Drives the automatic report. Cheap; call from the UI tick.
void Tick(const D2RL::PluginContext* context);

// Starts or stops answering on the chosen free slot. Injection is what puts
// D2R into controller mode, which disables its own keyboard scheme -- so this
// belongs at startup rather than mid-session, or the player is left between
// modes with neither input live.
void SetInjection(const D2RL::PluginContext* context, bool enabled);
bool Injecting();

// The synthetic pad as it stands right now, sampled from the keyboard afresh
// rather than remembered from the game's last poll.
//
// The mod's own features read input on its UI tick (input_direction.h), and
// they read it through a DIFFERENT XInput module than the game does -- the
// detour above patches the game's, so nothing the keyboard does is visible
// there. Without this, keyboard play would drive the game while the entity
// scan, the pathfinder and guidance stayed deaf to it.
struct Synthetic {
	bool     active = false; // injecting, on a slot that was free
	unsigned slot   = 0;     // the slot it answers on, so a reader can skip the real one
	KeyboardPad::Pad pad;
};
Synthetic SyntheticPad();

SlotStats Stats(unsigned slot);

}
