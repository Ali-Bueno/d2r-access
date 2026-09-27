#pragma once

#include "cairn_stones.h"

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <optional>

namespace D2RAccess::NativeCairn {
bool Register(const D2RL::PluginContext* context);
void Unregister();
bool Available();
std::optional<CairnStones::Order> Read(uint64_t session);
}
