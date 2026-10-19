#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// The harmful states a monster can put on the player, by their states.txt rows,
// spoken as they begin and end (a tester, 2026-10-14). Pure logic, unit-tested.
// docs/status-effects.md
namespace D2RAccess::StatusEffects {

// The conditions the mod words itself: the game's tables have no name for them.
enum class Condition { Poisoned, Frozen, Chilled, Stunned, Slowed, Count };
// The monster curses the game leaves unnamed (their skills have no description).
enum class CurseName { DefenseCurse, BloodMana, Count };

struct Effect {
	int state = -1;     // states.txt row, the id the game's state checker takes
	int condition = -1; // a Condition, or
	int skill = -1;     // the skills.txt row of the curse, whose name the game gives, or
	int curseName = -1; // a CurseName
	bool Curse() const { return condition < 0; }
};

// states.txt rows (game-data/global/excel/states.txt): poison 2, freeze 1, cold 11,
// stunned 21, slowed 24; the curse=1 rows a monster casts, with the skill whose
// auratargetstate they are. Not the curse=1 buffs (battlecry, shrines, bloodlust).
inline constexpr std::array Effects{
	Effect{2, static_cast<int>(Condition::Poisoned)},
	Effect{1, static_cast<int>(Condition::Frozen)},
	Effect{11, static_cast<int>(Condition::Chilled)},
	Effect{21, static_cast<int>(Condition::Stunned)},
	Effect{24, static_cast<int>(Condition::Slowed)},
	Effect{9, -1, 66},   // amplifydamage, Amplify Damage
	Effect{19, -1, 72},  // weaken
	Effect{23, -1, 71},  // dimvision
	Effect{55, -1, 76},  // ironmaiden
	Effect{56, -1, 77},  // terror
	Effect{57, -1, 86},  // attract
	Effect{58, -1, 82},  // lifetap
	Effect{59, -1, 81},  // confuse
	Effect{60, -1, 87},  // decrepify
	Effect{61, -1, 91},  // lowerresist
	Effect{113, -1, -1, static_cast<int>(CurseName::DefenseCurse)}, // defense_curse, skill 309
	Effect{114, -1, -1, static_cast<int>(CurseName::BloodMana)},    // blood_mana, skill 310
};

// An effect gone for less than this that comes back is the same one going on: a
// cold hit chills again before the last chill is over. Its end waits that long, and
// no longer (a tester, 2026-10-19: said as close to the real end as can be).
inline constexpr uint64_t EndHoldMs = 500;

// The shrines' own states (states.txt shrine_armor 128 to shrine_experience 137). The
// game holds one at a time; when the last one ends the player hears it (a tester, 2026-10-15).
inline constexpr std::array<int, 10> ShrineStates{128, 129, 130, 131, 132, 133, 134, 135, 136, 137};

// The stamina potion's state (states.txt staminapot 180): the bar stays full while it holds.
inline constexpr int StaminaPotionState = 180;
// The antidote and thawing potions' states (states.txt antidote 178, thawing 179): +50 poison or cold resistance.
inline constexpr std::array<int, 2> FortifyStates{178, 179};

struct Change {
	std::vector<size_t> started, ended; // indexes into Effects
	bool Empty() const { return started.empty() && ended.empty(); }
};

class Tracker {
public:
	// `active` holds the Effects indexes on the player now. The first update
	// after a reset takes a silent baseline: entering a game is not news.
	Change Update(const std::vector<size_t>& active, uint64_t now);
	// The effects on the player at the last update.
	std::vector<size_t> Active() const;
	// An ended effect waits to be said.
	bool EndPending() const;
	void Reset();
private:
	std::array<bool, Effects.size()> on_{};
	std::array<uint64_t, Effects.size()> endAt_{}; // when an end is said, 0 for none pending
	bool baseline_ = false;
};
}
