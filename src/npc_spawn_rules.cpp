#include "npc_spawn_rules.h"

#include "entity_radar.h"

namespace D2RAccess::NpcSpawnRules {
namespace {
// The InitFns, from the game's own init functions (D2R 3.3.93787; docs/entity-scan.md).
constexpr int JerhynPlaceholder1 = 18, JerhynPlaceholder2 = 19, HratliStart = 49, HratliEnd = 50,
	NatalyaStart = 52, DrehyaInTown = 66;
// Quest record rows, in the save's order (docs/quests.md).
constexpr int ActTwoIntroRow = 8, RadamentRow = 9, ArcaneSanctuaryRow = 12, SevenTombsRow = 14,
	ActThreeIntroRow = 16, GuardianRow = 22, PrisonOfIceRow = 37;
constexpr uint16_t PrimaryGoalBit = 0x2000;

// The game tests bit 13; a quest completed in an earlier game carries bit 0 or 15 as well.
bool Done(uint16_t flags) { return flags & (PrimaryGoalBit | EntityRadar::QuestDoneBits); }

// Jerhyn stands at his first place until Act II's intro, Radament and the Arcane Sanctuary
// are done and the Seven Tombs has begun nowhere; then the second place has him.
std::optional<bool> JerhynFirst(const QuestFlags& flags) {
	const auto intro = flags(ActTwoIntroRow), radament = flags(RadamentRow);
	const auto sanctuary = flags(ArcaneSanctuaryRow), tombs = flags(SevenTombsRow);
	if (!intro || !radament || !sanctuary || !tombs) return std::nullopt;
	return !Done(*intro) && !Done(*radament) && !Done(*sanctuary)
		&& !Done(*tombs) && !(*tombs & EntityRadar::QuestStartedBits);
}
}

std::optional<bool> Spawns(int initFn, const QuestFlags& flags) {
	switch (initFn) {
	case JerhynPlaceholder1: return JerhynFirst(flags);
	case JerhynPlaceholder2: if (const auto first = JerhynFirst(flags)) return !*first; return std::nullopt;
	case HratliStart: if (const auto intro = flags(ActThreeIntroRow)) return !Done(*intro); return std::nullopt;
	case HratliEnd: if (const auto intro = flags(ActThreeIntroRow)) return Done(*intro); return std::nullopt;
	case NatalyaStart: if (const auto guardian = flags(GuardianRow)) return !Done(*guardian); return std::nullopt;
	// Anya comes to town once her rescue is rewarded: bit 0 or 15, not the thaw's bit 13 alone.
	case DrehyaInTown: if (const auto prison = flags(PrisonOfIceRow)) return (*prison & EntityRadar::QuestDoneBits) != 0; return std::nullopt;
	default: return std::nullopt;
	}
}
}
