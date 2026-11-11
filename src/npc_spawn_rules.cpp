#include "npc_spawn_rules.h"

#include "entity_radar.h"

#include <algorithm>
#include <array>

namespace D2RAccess::NpcSpawnRules {
namespace {
// The InitFns, from the game's own init functions (D2R 3.3.93787; docs/entity-scan.md).
constexpr int JerhynPlaceholder1 = 18, JerhynPlaceholder2 = 19, HratliStart = 49, HratliEnd = 50,
	NatalyaStart = 52, DrehyaInTown = 66, NihlathakInTown = 68;
// Quest record rows, in the save's order (docs/quests.md).
constexpr int ActTwoIntroRow = 8, RadamentRow = 9, ArcaneSanctuaryRow = 12, SevenTombsRow = 14,
	ActThreeIntroRow = 16, GuardianRow = 22, PrisonOfIceRow = 37;
constexpr uint16_t PrimaryGoalBit = 0x2000;
// The game takes Nihlathak from town, for the rest of a game, once a player has been on the
// way to Anya (Arreat Plateau, Crystalline Passage, Frozen River) or in his temple (Levels.txt).
constexpr std::array<int, 7> NihlathakGoneLevels{112, 113, 114, 121, 122, 123, 124};

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

// Nihlathak's InitFn (0x548EE0) tests the Prison of Ice's game data (+0xA8), reset each game and set
// when its creator joins with Anya's rescue rewarded (row 37 bit 0 or 15), or on entering those
// levels. docs/entity-scan.md#npcs-at-marker-objects-2026-10-25
std::optional<bool> NihlathakHome(const QuestFlags& flags, const ThisGame& game) {
	if (!game.entered) return std::nullopt;
	// The player's own walk takes him whoever else is in the game; the others' cannot be seen.
	if (std::any_of(NihlathakGoneLevels.begin(), NihlathakGoneLevels.end(), game.entered)) return false;
	if (game.shared) return std::nullopt;
	const auto prison = flags(PrisonOfIceRow);
	if (!prison) return std::nullopt;
	return !(*prison & EntityRadar::QuestDoneBits);
}
}

void GameLevels::Note(uint64_t session, int level) {
	if (session != session_) { session_ = session; levels_.clear(); }
	if (level > 0 && std::find(levels_.begin(), levels_.end(), level) == levels_.end()) levels_.push_back(level);
}

bool GameLevels::Entered(uint64_t session, int level) const {
	return session == session_ && std::find(levels_.begin(), levels_.end(), level) != levels_.end();
}

std::optional<bool> Spawns(int initFn, const QuestFlags& flags, const ThisGame& game) {
	switch (initFn) {
	case JerhynPlaceholder1: return JerhynFirst(flags);
	case JerhynPlaceholder2: if (const auto first = JerhynFirst(flags)) return !*first; return std::nullopt;
	case HratliStart: if (const auto intro = flags(ActThreeIntroRow)) return !Done(*intro); return std::nullopt;
	case HratliEnd: if (const auto intro = flags(ActThreeIntroRow)) return Done(*intro); return std::nullopt;
	case NatalyaStart: if (const auto guardian = flags(GuardianRow)) return !Done(*guardian); return std::nullopt;
	// Anya comes to town once her rescue is rewarded: bit 0 or 15, not the thaw's bit 13 alone.
	case DrehyaInTown: if (const auto prison = flags(PrisonOfIceRow)) return (*prison & EntityRadar::QuestDoneBits) != 0; return std::nullopt;
	case NihlathakInTown: return NihlathakHome(flags, game);
	default: return std::nullopt;
	}
}
}
