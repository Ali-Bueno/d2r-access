#pragma once

#include "ground_items.h"
#include "percent_steps.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

// How fast the loops that pulse repeat -- a knock, a click, a chime with silence
// after it -- one setting per kind (the user, 2026-10-16). Faster cuts each pass
// short with a brief fade, slower pads it with silence, as the drop sounds' own
// speed does (ItemAudio::DropSpeedPercents). The steady loops -- gold, equipment,
// the portal's hum, the slime door -- have no beat to pace.
namespace D2RAccess::LoopPace {
enum class Group { Doors, SmallItems, Containers, Shrines, Quest, Rooms, Epoch, Count };
inline constexpr size_t GroupCount = static_cast<size_t>(Group::Count);
inline constexpr auto Percents = PercentSteps::Quarters<50, 300>();
inline constexpr int NeutralLevel = PercentSteps::LevelOf(Percents, 100);
static_assert(Percents[NeutralLevel] == 100);

constexpr std::optional<Group> Of(GroundItems::Sound sound) {
	using S = GroundItems::Sound;
	switch (sound) {
	case S::WoodenDoor: case S::MetalDoor: case S::StoneDoor: case S::Doorway: return Group::Doors;
	case S::Scroll: case S::Rune: case S::Gem: return Group::SmallItems;
	case S::Chest: case S::Corpse: case S::WeaponRack: case S::Stash: return Group::Containers;
	case S::Shrine: case S::Well: return Group::Shrines;
	case S::Quest: return Group::Quest;
	case S::UnexploredRoom: return Group::Rooms;
	case S::EpochGold: case S::EpochHealthPotion: case S::EpochManaPotion: case S::EpochRejuvenationPotion:
	case S::EpochOtherPotion: return Group::Epoch;
	default: return std::nullopt;
	}
}

// One pass of a recording `natural` samples long at a level; 0 at 100 percent,
// where the recording plays as it is.
constexpr size_t PassSamples(size_t natural, int level) {
	if (level == NeutralLevel || level < 0 || level >= static_cast<int>(Percents.size())) return 0;
	return natural * 100 / static_cast<size_t>(Percents[static_cast<size_t>(level)]);
}

// Each group's level, as the settings list trades it.
inline std::atomic<int>& Setting(Group group) {
	struct Level { std::atomic<int> value{NeutralLevel}; };
	static std::array<Level, GroupCount> levels;
	return levels[static_cast<size_t>(group)].value;
}
}
