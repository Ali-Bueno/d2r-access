#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::GameTextReader {
bool Register(const D2RL::PluginContext* context);
void Unregister();
// True while this reader owns the text/menu context; prevents the old indicator
// reader from speaking a control behind a conversation or tutorial.
bool Tick(const D2RL::PluginContext* context);
void SetEnabled(bool enabled);
void RequestRepeat();
void RequestDump();
}
