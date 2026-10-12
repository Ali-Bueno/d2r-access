#pragma once

// A knock when the inventory cursor is pushed against its grid's edge (grid_edge.h),
// panned to the side hit. Switch `inventory-edge` in the Ctrl+S list.
namespace D2RAccess::GridEdgeService {
// Every UI update, after input is polled.
void Tick();
bool Enabled();
void SetEnabled(bool enabled);
}
