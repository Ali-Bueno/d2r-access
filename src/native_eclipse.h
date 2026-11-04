#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>

// Whether the land is dark as the client draws it: Act II under the Tainted Sun. Read-only.
// docs/quests.md
namespace D2RAccess::NativeEclipse {
struct Sky {
	uintptr_t act = 0; // the act the client holds, to tell when another one loads
	bool dark = false;
};
bool Initialize(const D2RL::PluginContext* context);
// Nothing outside a game or on another game build.
std::optional<Sky> Read();
}
