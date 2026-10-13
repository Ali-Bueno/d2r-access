#pragma once

#include <D2RLPlugin/api.h>

#include <string_view>

// Single speech sink for the whole plugin. Wraps the PRISM C API loaded
// dynamically from prism.dll next to the plugin (see prism-prebuilt.md).
// Speech runs on its own thread: Speak/Stop only queue, so the caller (the
// game's UI thread) never waits for the screen reader.
namespace D2RAccess::ScreenReader {

// Loads prism.dll from the given directory and acquires the best backend.
// Returns false (and leaves the sink silent) when PRISM is unavailable.
bool Initialize(const wchar_t* pluginDirectory);
void Shutdown();

bool IsAvailable();

// Why the last Initialize failed (empty when it succeeded).
const char* LastError();

// Name of the backend PRISM picked (NVDA, JAWS, SAPI, ...), or "" if none.
const char* BackendName();

// interrupt=true only on context changes (new screen / panel); false for
// incremental information so previous speech is not cut off.
void Speak(std::string_view text, bool interrupt);
// When anything was last given to Speak, as GetTickCount64 counts; 0 before
// the first. For a reader of last resort that speaks only if no other did.
unsigned long long LastSpoken();
// One saying of exactly `text` before `untilMs` (GetTickCount64) is dropped:
// a switch put back at start as it was saved says its own change, which is
// not news to the player.
void Swallow(std::string_view text, unsigned long long untilMs);
// Speaks `text` queued and keeps it from being cut off: until `untilMs`
// (GetTickCount64) an interrupting phrase queues behind it and Stop waits. For
// news that comes with a change the game makes in the same moment (the Cairn
// scroll's order, as the game moves the inventory cursor beside it).
void Lead(std::string_view text, unsigned long long untilMs);
void Stop();
// Every phrase given to Speak is logged through this context from now on, and
// every one Swallow held back, so a test's log says what the player heard.
void LogTo(const D2RL::PluginContext* context);

}
