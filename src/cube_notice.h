#pragma once

#include "inventory_arrivals.h"

#include <cstdint>
#include <vector>

namespace D2RL { struct PluginContext; }

// Says what happened in the Horadric Cube: its window opening, items sent in or out,
// a transmute's result. docs/horadric-cube.md
namespace D2RAccess::CubeNotice {
// "Cube opened" when the cube's window comes up.
void Tick(const D2RL::PluginContext* context);
// What the cube's items did between two samples of the player's items.
void Announce(const D2RL::PluginContext* context, const std::vector<InventoryArrivals::Item>& before,
	const std::vector<InventoryArrivals::Item>& now, uint32_t heldBefore, uint32_t heldNow);
}
