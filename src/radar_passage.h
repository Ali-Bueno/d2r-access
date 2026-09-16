#pragma once
#include "navigation_grid.h"

namespace D2RAccess::Radar {
// A short route in the named direction, with at most one body span of lateral
// adjustment. This is not global reachability or a promise about distant rooms.
Passage ScanPassage(const Navigation::Grid& grid, Position axis, const Settings& settings);
int PassageRadius(const Settings& settings, int bodyPattern);
}
