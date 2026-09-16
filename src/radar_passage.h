#pragma once
#include "navigation_grid.h"

namespace D2RAccess::Radar {
// Native footprints are one cell (pattern 0) or three cells across.
double BodySpan(int bodyPattern);
// Grid radius that contains every route the passage scan can prove.
int PassageRadius(double reachDepth, int bodyPattern);
// The straight route in the named direction: the raster of a diagonal line
// wanders at most one cell off its axis, and that is the only lateral
// tolerance. Side routes are not "ahead" (live analysis 2026-09-16: three
// cells of slack hid walls until contact and counted deflections as exits).
// Open at or beyond openDepth, blocked at or below closeDepth; depth keeps
// growing to the window edge so a route past a known wall is measurable.
// Unknown cells beyond openDepth only cap the reach.
Passage ScanPassage(const Navigation::Grid& grid, Position axis, double openDepth, double closeDepth);
}
