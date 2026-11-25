#pragma once

#include "entity_radar.h"
#include "radar.h"

#include <cstdint>
#include <optional>
#include <vector>

// Unexplored rooms cut off on foot in their own level but got to by way of a
// neighbouring one, in two legs: to the crossing out, then, once across, to the
// crossing back in. docs/detours.md
namespace D2RAccess::DetourService {
// The rooms of `cut` (scanner room entries) with a way through a neighbour, each as the crossing to
// leave by, nearest first by the whole way. `mostlyUnseen`: most of the level was never shown, so ground of
// the neighbour not shown yet may be a way when none is known.
std::vector<EntityRadar::Entity> Find(const Radar::World& world, const std::vector<EntityRadar::Entity>& cut, bool mostlyUnseen);
bool IsDetour(const EntityRadar::Entity& entity);
// The player chose one of the entries Find gave last: its second leg is kept for the other side.
void Chosen(const EntityRadar::Entity& entity);
// In the level passed through: the crossing back in, on the way to the room chosen.
std::optional<EntityRadar::Entity> Onward(const Radar::World& world);
// True once for each level come into with a room chosen, some time after arriving. Forgets the choice
// when the game or the levels are others. Cheap: called every update.
bool Due(uint64_t session, int level, uint64_t now);
// Forgets the choice once its room is got to on foot from where `world` has the player.
void ForgetReached(const Radar::World& world);
}
