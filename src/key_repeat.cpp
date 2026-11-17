#include "key_repeat.h"

#include <windows.h>
#include <algorithm>

namespace D2RAccess::KeyRepeat {
namespace {
// SystemParametersInfo's documented ranges (SPI_GETKEYBOARDDELAY, SPI_GETKEYBOARDSPEED).
constexpr unsigned MaxDelaySetting = 3;
constexpr uint64_t DelayStepMs = 250; // 0 is about 250 ms, each step about 250 ms more
constexpr unsigned MaxSpeedSetting = 31;
constexpr double SlowestPerSecond = 2.5, FastestPerSecond = 30.0; // speed 0 and speed 31, "approximately"
constexpr double MsPerSecond = 1000.0;
} // namespace

Timing From(const Settings& settings, bool repeat, bool guard) {
	Timing timing;
	timing.repeats = repeat;
	timing.guard = guard;
	const unsigned delay = std::min(settings.delay, MaxDelaySetting);
	timing.delayMs = std::max(timing.delayMs, (delay + 1) * DelayStepMs);
	const unsigned speed = std::min(settings.speed, MaxSpeedSetting);
	const double perSecond = SlowestPerSecond + (FastestPerSecond - SlowestPerSecond) * speed / MaxSpeedSetting;
	timing.periodMs = std::max(timing.periodMs, static_cast<uint64_t>(MsPerSecond / perSecond));
	if (settings.filterKeys) {
		timing.delayMs = std::max<uint64_t>(timing.delayMs, settings.filterDelayMs);
		timing.periodMs = std::max<uint64_t>(timing.periodMs, settings.filterRepeatMs);
		timing.bounceMs = settings.filterBounceMs;
	}
	if (guard) timing.bounceMs = std::max(timing.bounceMs, GuardBounceMs);
	return timing;
}

Settings Read() {
	Settings settings;
	UINT delay = settings.delay;
	if (SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) settings.delay = delay;
	DWORD speed = settings.speed;
	if (SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &speed, 0)) settings.speed = speed;
	FILTERKEYS filter{sizeof(FILTERKEYS)};
	if (SystemParametersInfoW(SPI_GETFILTERKEYS, sizeof(filter), &filter, 0) && (filter.dwFlags & FKF_FILTERKEYSON)) {
		settings.filterKeys = true;
		settings.filterDelayMs = filter.iDelayMSec;
		settings.filterRepeatMs = filter.iRepeatMSec;
		settings.filterBounceMs = filter.iBounceMSec;
	}
	return settings;
}

bool Step(Press& press, bool down, bool tapped, uint64_t now, const Timing& timing, bool otherHeld) {
	bool step = false;
	if (!press.down && (down || tapped)) {
		// A press this soon after the key came up is the key bouncing.
		const bool bounced = timing.bounceMs && press.released && now - press.released < timing.bounceMs;
		press.blocked = timing.guard && otherHeld;
		step = !bounced && !press.blocked;
		press.next = now + timing.delayMs;
	} else if (down && !press.blocked && timing.repeats && now >= press.next) {
		step = true;
		press.next = now + timing.periodMs;
	}
	// Up now, or a press that came and went between two updates.
	if ((press.down || tapped) && !down) press.released = now;
	press.down = down;
	if (!down) press.blocked = false;
	return step;
}
} // namespace D2RAccess::KeyRepeat
