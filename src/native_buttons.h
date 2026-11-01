#pragma once

#include <D2RLPlugin/api.h>

// Verifies the game's button code before NativeUi::ButtonDisabled reads a button's disabled flag.
// docs/native-ui.md
namespace D2RAccess::NativeButtons {
bool Initialize(const D2RL::PluginContext* context);
}
