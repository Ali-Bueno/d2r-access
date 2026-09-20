#pragma once

namespace D2RL {
struct PluginContext;
}

// Keys the simulated pad owns, claimed so the GAME never sees them.
//
// The pad is built by polling the keyboard (GetAsyncKeyState in xinput_hook),
// which reads a key without consuming it, so every mapped key reaches D2R as
// well. Usually that is harmless -- controller mode suppresses the game's own
// bindings once a pad is really driving it (MEASURED 2026-09-20: W moves the
// character and no longer swaps weapons). Menu navigation is the exception:
// the arrow keys still reach the inventory and the skill tree, so one tap
// moves the highlight twice, once for our D-pad frame and once for the game's
// own arrow handling.
//
// The fix the user asked for is consumption rather than avoidance: "we don't
// check against conflicts or rebind keys. we just make sure only the action we
// want is passed to the game by the mod/loader."
//
// OPEN, and this module is the experiment that settles it. TWO things must
// hold and only one is established:
//   1. Returning ActionResult::Handled consumes the press. ESTABLISHED --
//      PluginSDK README "Input Actions" says so outright, and the loader's own
//      dispatch aborts on the first Handled and returns before reaching the
//      downstream handler.
//   2. D2R's menu navigation reads the key EVENT rather than polling key
//      state. NOT established. If it polls, consuming the event changes
//      nothing and this whole approach is a dead end.
// And a third thing is already known to be wrong: the SDK's Key enum has NO
// arrow keys -- it runs Home = 0x24 straight to Print = 0x2A, skipping
// 0x25-0x28. The registration below casts the virtual-key codes in, which the
// enum's uint32_t base makes expressible but which the SDK never documents.
// Register() logs whether the loader accepted each one, so a failure is
// visible in the log without anyone having to play the game to find out.
namespace D2RAccess::KeyClaim {

// Registers the claimed keys. Safe to call when the input service is missing;
// it simply claims nothing. Never throws.
bool Register(const D2RL::PluginContext* context);

void Unregister();

}
