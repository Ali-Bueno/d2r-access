#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>

// Every action the plugin gives the loader's Controls menu, registered here so a key the player
// changes there and does not Apply can be pointed out (docs/keyboard-keys.md#changing-a-key).
namespace D2RAccess::InputActions {

D2RL::Input::Result Register(const D2RL::InputService* input, const D2RL::PluginContext* context,
	const D2RL::Input::ActionRegistration* action, D2RL::Input::ActionHandle* handle);
D2RL::Input::Result Unregister(const D2RL::InputService* input, const D2RL::PluginContext* context, D2RL::Input::ActionHandle handle);

// Watches the bindings while Options is open: the first change says to press Apply, and closing
// Options with a change the loader did not save says it will be lost.
void Tick(const D2RL::PluginContext* context, uint64_t now);

}
