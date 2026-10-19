#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Announcement logic for the player's life and mana and for equipment that
// breaks. Pure: no game, widget or speech dependency, so the steps, the orb
// parsing and the break detection are all covered by tests.
namespace D2RAccess::Vitals {

// Downward announcement steps, highest first: 25 points from full down to 50,
// 10 points from 40, single points from 5. A resource sitting above the first
// step is not announced, so ordinary chip damage stays silent; crossing a step
// speaks the real percentage, never the step itself.
inline constexpr int Steps[] = {75, 50, 40, 30, 20, 10, 5, 4, 3, 2, 1};

// At or below this percentage the announcement interrupts whatever is being
// spoken; above it the reading waits its turn.
inline constexpr int CriticalPercent = 10;

// The step a percentage sits in, 0 when the resource is empty.
int Step(int percent);

// Whole percent of current out of maximum, floored so a reading never rounds
// up into a safer number; -1 when the maximum is unusable.
int Percent(int current, int maximum);

// How far into its level an experience total is, floored to 0-99, as the
// game's experience bar fills between the two thresholds; -1 when they make no sense.
int ExperiencePercent(uint64_t experience, uint64_t levelStart, uint64_t nextLevel);
// Where a mercenary's level began, from its next-level threshold: the hireling
// panel (0x14F3400) uses ExpPerLvl * L * L * (L + 1), so the next one is
// ExpPerLvl * (L + 1)^2 * (L + 2). 0 when the threshold does not fit that rule.
uint64_t MercenaryLevelStart(int level, uint64_t nextLevel);

// The two numbers an orb's value display shows ("45/120"), digit grouping and
// surrounding text ignored. Nothing when the text carries no such pair.
struct Orb {
	int current = 0, maximum = 0;
};
std::optional<Orb> ParseOrb(std::string_view text);

// Remembers the last step so only real crossings speak.
class Meter {
public:
	// The percentage to announce, or -1 when there is nothing to say. The
	// first reading only takes a baseline: entering a game is not a change.
	int Update(int percent);
	int Previous() const { return previous_; }
	void Reset();

private:
	int step_ = -1; // no baseline taken yet
	int previous_ = -1;
};

struct Alert {
	int life = -1, mana = -1;
	bool interrupt = false;
	bool Any() const { return life >= 0 || mana >= 0; }
};

// How much mana is spoken unasked: every step as for life, only a fall to
// ManaLowPercent or under, or nothing (a tester: casting drowned out life).
enum class ManaAlerts { All, Low, Off };
inline constexpr int ManaAlertLevels = 3;
inline constexpr int ManaLowPercent = 20; // one of Steps

// Produces at most one announcement per HUD sample. A critical value interrupts
// only when it has fallen, so mana recovery cannot erase a life warning.
class Alerts {
public:
	Alert Update(int life, int mana, ManaAlerts manaAlerts = ManaAlerts::All);
	void Reset();
private:
	Meter life_, mana_;
};

inline constexpr int MissingConfirmationSamples = 10;
class OrbAvailability {
public:
	// Bit 0 is life; bit 1 is mana. A missing reading must persist across
	// consecutive samples before it is treated as an option needing attention.
	unsigned Update(bool lifeReadable, bool manaReadable);
	void Reset();
private:
	int missing_[2]{};
};

// The character's gait as the game moves it. The run toggle itself is not
// read: a press of the game's toggle says the flip, and only that is spoken.
// The game also walks on its own, out of stamina or pushing into a wall
// (controllerskillsettings forceWalk*), so an observed step only re-syncs the
// toggle silently, and only from free running or walking (a tester heard
// "Walking" and "Running" against walls with no press).
enum class Gait { Walk, Run };
// Samples in a row a new gait must hold before the toggle is taken to be it.
inline constexpr int GaitConfirmSamples = 3;
class GaitTracker {
public:
	// One sample: the gait while walking or running, nothing otherwise; `free`
	// is false while the game may be forcing the walk. Returns the gait the
	// toggle was re-synced to, for the log; the first is a baseline.
	std::optional<Gait> Observe(std::optional<Gait> moving, bool free = true);
	// The game's run toggle was pressed: the flipped gait, nothing before a baseline.
	std::optional<Gait> Toggle();
	void Reset();
private:
	std::optional<Gait> known_, pending_;
	int held_ = 0;
};

// Stamina falling through each step, and running out. A step is armed again
// only once stamina has recovered StaminaRearm points above it, so the drain
// and regeneration of running in short bursts does not repeat it.
inline constexpr int StaminaSteps[] = {50, 25, 10};
inline constexpr int StaminaRearm = 10;
class StaminaMeter {
public:
	// The percentage to announce, 0 when stamina ran out, -1 for nothing; the
	// first reading is a baseline.
	int Update(int percent);
	void Reset();
private:
	int depth_ = -1;
};

// A stack held in hand (arrows, bolts, throwing weapons) falling through each
// step. An item counts as a stack once it has been seen holding more than one.
inline constexpr int AmmoSteps[] = {50, 20, 10, 5};
class AmmoMeter {
public:
	struct Stack {
		uint32_t id = 0;
		int quantity = 0;
	};
	// The stack to announce, if one fell through a step; a stack first seen is a baseline.
	std::optional<Stack> Update(const std::vector<Stack>& inHand);
	// The stacks among the items in hand.
	std::vector<Stack> Known(const std::vector<Stack>& inHand) const;
	void Reset();
private:
	struct Tracked {
		uint32_t id = 0;
		int depth = 0;
	};
	std::vector<Tracked> tracked_;
};

class MercenaryMeter {
public:
	struct Alert {
		int life = -1, level = -1;
		bool interrupt = false;
	};
	Alert Update(std::optional<uint32_t> id, int percent, int level);
	void Reset();
private:
	std::optional<uint32_t> id_;
	Meter life_;
	int level_ = -1;
};

// Equipment durability. An item that reaches zero from a positive value has
// just broken; indestructible items (no maximum) never do. Items that stop
// being worn are forgotten, so re-equipping a broken item stays silent.
class Breakage {
public:
	struct Item {
		uint32_t id = 0;
		int durability = 0, maximumDurability = 0, bodyLocation = 0;
		uint32_t classId = 0, quality = 0; // for naming it
		int quantity = 1; // one for an item that does not stack
	};
	// The body locations of the items that broke since the previous sweep, in
	// the order they were reported.
	std::vector<int> Sweep(const std::vector<Item>& worn);
	void Reset();

private:
	struct Known {
		uint32_t id = 0;
		bool broken = false;
	};
	std::vector<Known> known_;
};

// Items that start being worn, whichever way they got there. The first sweep
// after a reset is the baseline, so entering a game says nothing, and a weapon
// swap only moves items already worn.
class Equipping {
public:
	std::vector<Breakage::Item> Sweep(const std::vector<Breakage::Item>& worn);
	void Reset();

private:
	std::vector<uint32_t> worn_;
	bool known_ = false;
};

// The heartbeat under a share of life (a tester, 2026-10-15): on once life falls to
// the threshold, off only once it climbs ReleasePoints over it, so a regeneration
// tick at the line does not start it again. Unknown or no life (dead) is off.
class LowLife {
public:
	static constexpr int ReleasePoints = 5;
	// True while the heartbeat should sound; `started` is set on the update that turns it on.
	bool Update(int lifePercent, int threshold, bool* started = nullptr);
	void Reset() { on_ = false; }
private:
	bool on_ = false;
};
// The thresholds the settings list offers, percent of life; it starts at a quarter.
inline constexpr int LowLifeThresholds[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 60, 70};
inline constexpr int DefaultLowLifeLevel = 4;
static_assert(LowLifeThresholds[DefaultLowLifeLevel] == 25);

}
