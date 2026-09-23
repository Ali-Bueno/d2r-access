#pragma once
#include <cstdint>
#include <optional>
namespace D2RL { struct PluginContext; }
namespace D2RAccess::NativeBelt {
// The inventory potion drawn translucently in an empty bottom belt cell.
std::optional<uintptr_t> AutomaticItem(const D2RL::PluginContext* context, uintptr_t widget, int column);
}
