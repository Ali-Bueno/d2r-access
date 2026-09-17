#pragma once
#include "radar.h"
#include <cstdint>
#include <vector>

namespace D2RAccess::NativeLevelExits {
struct Exit { uint32_t warp = 0, destination = 0; Radar::Position position; };
// Walk existing logical rooms in the current level, including inactive rooms.
// No native generator, room loader, map reveal or memory write is involved.
// Empty/unavailable presets are skipped. A malformed traversal returns no exits.
bool Read(uintptr_t activeRoom, std::vector<Exit>& output);
// Coordinate identity survives native warp-unit unloading/recreation. Zero is
// reserved for entities that are not fixed level exits.
uint64_t Identity(Radar::Position position);
}
