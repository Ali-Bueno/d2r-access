#pragma once

#include <D2RLPlugin/api.h>
#include "callback_boundary.h"

// The SDK has no per-frame callback; UI callbacks run during a later
// root-widget update. Re-queueing a callback from inside itself yields one
// tick per UI update, which is how the focus tracker polls the game's UI.
namespace D2RAccess::UiThreadPump {

using TickFn = void (*)(const D2RL::PluginContext* context);
using RecoveryFn = void (*)(const D2RL::PluginContext*, CallbackFault) noexcept;

// Starts ticking `tick` on the UI thread once per update. Returns false only
// when the Thread service itself is unavailable; a failed first queue (UI
// root not created yet at plugin load) is retried by Kick.
bool Start(const D2RL::PluginContext* context, TickFn tick, RecoveryFn recover = nullptr);

// Re-arms the pump if a queue attempt failed (call from any UI-thread
// callback, e.g. the UI message listener). Cheap when already queued.
void Kick(const D2RL::PluginContext* context);

// Stops re-queueing; the in-flight callback (if any) becomes a no-op.
void Stop();

}
