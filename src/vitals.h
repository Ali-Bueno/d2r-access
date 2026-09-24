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

// Produces at most one announcement per HUD sample. A critical value interrupts
// only when it has fallen, so mana recovery cannot erase a life warning.
class Alerts {
public:
	Alert Update(int life, int mana);
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

// Diff-gates a verified player condition. Missing reads preserve the last
// state; entering a game takes a silent baseline.
class Condition {
public:
	std::optional<bool> Update(std::optional<bool> active);
	void Reset();
private:
	std::optional<bool> previous_;
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

}
