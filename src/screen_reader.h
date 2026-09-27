#pragma once

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
void Stop();

}
