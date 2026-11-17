#pragma once

#include <cstdint>

// When a held menu arrow steps again. The mod polls the arrows itself, so the
// keyboard's repeat in Windows settings and FilterKeys never reached it: a key
// held a little long walked on every 180 ms whatever the player had set (a
// tester with cerebral palsy, 2026-11-17: "it loves to jump around"). The
// mod's own pace stays the fastest a held arrow goes; a slower Windows delay
// or rate, FilterKeys' repeat and bounce times, and the mod's switch to not
// repeat at all, are kept. Pure logic, unit-tested without the game.
namespace D2RAccess::KeyRepeat {
// The mod's own pace, the fastest a held arrow steps (the pace the menu cursor
// had from the start): the first repeat after this long, then this often.
inline constexpr uint64_t BaseDelayMs = 500;
inline constexpr uint64_t BasePeriodMs = 180;
// With the guard on (Ctrl+S `menu-arrow-guard`), a press of the same arrow this soon after it came
// up is a hand bouncing on the key, not a second press. Chosen, not measured: longer than a
// tremor's second touch, shorter than a deliberate second step heard out first.
inline constexpr uint64_t GuardBounceMs = 250;

// Windows' keyboard settings as SystemParametersInfo gives them.
struct Settings {
	unsigned delay = 1;  // SPI_GETKEYBOARDDELAY: 0 (about 250 ms) to 3 (about 1 s)
	unsigned speed = 31; // SPI_GETKEYBOARDSPEED: 0 (about 2.5 a second) to 31 (about 30 a second)
	bool filterKeys = false; // SPI_GETFILTERKEYS, FKF_FILTERKEYSON
	uint32_t filterDelayMs = 0, filterRepeatMs = 0, filterBounceMs = 0;
};
struct Timing {
	uint64_t delayMs = BaseDelayMs, periodMs = BasePeriodMs;
	uint64_t bounceMs = 0; // a press this soon after the key came up is not one
	bool repeats = true;
	// An arrow pressed while another is held, or with another in the same update, is a
	// neighbouring key brushed: it does nothing until it comes up and is pressed again.
	bool guard = false;
};
Timing From(const Settings& settings, bool repeat, bool guard = false);
// The settings as Windows has them now; the defaults where it does not say.
Settings Read();

// One arrow: whether this update steps. `tapped` is a press the window was told
// of, which may have come and gone between two updates.
// `otherHeld`: another arrow was held already, or went down in this same update.
struct Press { bool down = false, blocked = false; uint64_t next = 0, released = 0; };
bool Step(Press& press, bool down, bool tapped, uint64_t now, const Timing& timing, bool otherHeld = false);
} // namespace D2RAccess::KeyRepeat
