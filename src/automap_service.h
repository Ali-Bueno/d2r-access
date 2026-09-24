#pragma once
#include <D2RLPlugin/api.h>

// Diagnostics for the read-only automap reader (native_automap.h): the console
// command "access-automap status|dump|watch" logs what the stock automap holds
// for the current level, and Ctrl+J toggles a watch that logs each change.
// Nothing here changes navigation or the scanner.
namespace D2RAccess::AutomapService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
}
