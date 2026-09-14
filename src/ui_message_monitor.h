#pragma once

#include <D2RLPlugin/api.h>

// Listens to every UI message the game dispatches (button clicks, panel
// open/close, ...). Two jobs: (1) discovery - log each (target, command, text)
// triple so we learn the game's UI vocabulary; (2) announce panel changes.
namespace D2RAccess::UiMessageMonitor {

bool Register(const D2RL::PluginContext* context);
void Unregister();

// Discovery logging of every message to the plugin log. Toggled from the
// console; off once the vocabulary is mapped.
void SetTraceEnabled(bool enabled);
bool IsTraceEnabled();

}
