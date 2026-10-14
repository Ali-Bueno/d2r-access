#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>

// The unit the game targets now, the one its world target label names: the
// hover entry the game keeps per local player. Several units can share a label
// (five Cairn Stones, two barrels); this says which one it is.
// docs/target-sight.md#the-games-own-target
namespace D2RAccess::NativeHover {
struct Target {
	uint32_t type = 0; // the game's unit type: 1 monster, 2 object, 5 warp (NativeActions::Unit)
	uint32_t id = 0;
};
bool Initialize(const D2RL::PluginContext* context);
// Nothing when the game targets nothing, or the image is not the one read. UI thread.
std::optional<Target> Read() noexcept;
}
