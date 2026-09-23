#pragma once

#include "exploration_cues.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::ExplorationCueService {
// Called before the terrain service records the current room as visited.
void Update(const D2RL::PluginContext* context, const NativeLevelExits::Layout& layout,
	Radar::Position player, const ExploredRooms::Memory& memory, bool maze);
void Silence();
}
