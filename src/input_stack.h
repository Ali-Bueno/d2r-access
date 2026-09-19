#pragma once

namespace D2RL {
struct PluginContext;
}

// Which input API the game itself reads a controller through decides where a
// synthetic pad has to be injected, and nothing in the shipped image answers
// it: D2RLoader replaces D2R.exe's import table with a single entry for its
// own DLL, and the decrypted dump carries no API name strings, so the usual
// static answers (imports, WinRT class names) are both unavailable offline.
//
// This reports what the process has actually loaded. The snapshot is taken at
// plugin load, before anything in the mod touches XInput itself -- otherwise
// the mod's own LoadLibrary would answer the question with its own footprint.
namespace D2RAccess::InputStack {

// Records which input modules were already present, before the mod loads any.
// Call once, early in plugin load, ahead of the first InputDirection::Poll.
void Capture();

// Logs the captured snapshot next to the process's current state, so a module
// the game loads lazily (only once a pad is first polled) still shows up, and
// is distinguishable from one the mod pulled in.
void Report(const D2RL::PluginContext* context);

}
