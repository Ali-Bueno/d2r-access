#include "key_repeat.h"
#include "test_support.h"

#include <cstdint>

using namespace D2RAccess;
using KeyRepeat::Press;
using KeyRepeat::Settings;
using KeyRepeat::Timing;

namespace {
// How many steps an arrow held from `from` to `to` takes, polled every `tick` ms.
int Held(Press& press, const Timing& timing, uint64_t from, uint64_t to, uint64_t tick) {
	int steps = 0;
	for (uint64_t now = from; now < to; now += tick) steps += KeyRepeat::Step(press, true, false, now, timing);
	(void)KeyRepeat::Step(press, false, false, to, timing);
	return steps;
}
} // namespace

int main() {
	constexpr uint64_t Tick = 16;
	{
		// Windows' defaults (delay 1, speed 31) are faster than the mod's own pace, which stays.
		const Timing timing = KeyRepeat::From(Settings{}, true);
		Check(timing.delayMs == KeyRepeat::BaseDelayMs && timing.periodMs == KeyRepeat::BasePeriodMs,
			"Windows' default repeat leaves the mod's pace as it was");
		Check(timing.bounceMs == 0 && timing.repeats, "no bounce time and repeats on without FilterKeys");
	}
	{
		// A slow keyboard in Windows slows the arrows too.
		Settings slow;
		slow.delay = 3;
		slow.speed = 0;
		const Timing timing = KeyRepeat::From(slow, true);
		Check(timing.delayMs == 1000, "the longest Windows delay is a second before the first repeat");
		Check(timing.periodMs == 400, "the slowest Windows rate is 2.5 a second");
		Settings past;
		past.delay = 9;
		past.speed = 99;
		Check(KeyRepeat::From(past, true).delayMs == 1000, "a delay past the documented range counts as the longest");
	}
	{
		// FilterKeys' own repeat and bounce times win when they are slower.
		Settings filter;
		filter.filterKeys = true;
		filter.filterDelayMs = 2000;
		filter.filterRepeatMs = 1000;
		filter.filterBounceMs = 300;
		const Timing timing = KeyRepeat::From(filter, true);
		Check(timing.delayMs == 2000 && timing.periodMs == 1000 && timing.bounceMs == 300, "FilterKeys' times are kept");
		Settings off = filter;
		off.filterKeys = false;
		Check(KeyRepeat::From(off, true).bounceMs == 0, "FilterKeys' times are ignored while it is off");
	}
	{
		const Timing timing = KeyRepeat::From(Settings{}, true);
		Press press;
		Check(Held(press, timing, 0, 300, Tick) == 1, "an arrow held under the delay steps once");
		Check(Held(press, timing, 1000, 1000 + 520, Tick) == 2, "held just past the delay, it steps again");
		Check(Held(press, timing, 3000, 3000 + 1100, Tick) == 5, "then once a period");
	}
	{
		const Timing timing = KeyRepeat::From(Settings{}, false);
		Press press;
		Check(Held(press, timing, 0, 5000, Tick) == 1, "with repeat off, a held arrow steps once");
		Check(KeyRepeat::Step(press, false, true, 6000, timing), "a press between two updates still steps");
		Check(Held(press, timing, 6100, 6400, Tick) == 1, "and the next press does too");
	}
	{
		Settings filter;
		filter.filterKeys = true;
		filter.filterBounceMs = 300;
		const Timing timing = KeyRepeat::From(filter, true);
		Press press;
		Check(Held(press, timing, 0, 100, Tick) == 1, "a first press steps");
		Check(Held(press, timing, 200, 260, Tick) == 0, "a press inside the bounce time does not");
		Check(Held(press, timing, 1000, 1100, Tick) == 1, "one after it does");
		Check(KeyRepeat::Step(press, false, true, 1200, timing) == false, "nor does a quick tap inside it");
	}
	{
		// The guard: a bounce of the same arrow, and a neighbour brushed while one is down.
		const Timing timing = KeyRepeat::From(Settings{}, true, true);
		Check(timing.guard && timing.bounceMs == KeyRepeat::GuardBounceMs, "the guard sets its own bounce time");
		Settings slowBounce;
		slowBounce.filterKeys = true;
		slowBounce.filterBounceMs = 1000;
		Check(KeyRepeat::From(slowBounce, true, true).bounceMs == 1000, "a longer FilterKeys bounce wins over the guard's");
		Check(KeyRepeat::From(Settings{}, true, false).bounceMs == 0, "without the guard there is no bounce time");
		Press down, right;
		Check(KeyRepeat::Step(down, true, false, 0, timing), "the first arrow steps");
		Check(!KeyRepeat::Step(right, true, false, 16, timing, true), "a second arrow pressed while it is down does not");
		bool repeated = false;
		for (uint64_t now = 32; now < 2000; now += Tick) repeated = KeyRepeat::Step(right, true, false, now, timing, true) || repeated;
		Check(!repeated, "nor does it repeat while held");
		(void)KeyRepeat::Step(right, false, false, 2000, timing, true);
		Check(KeyRepeat::Step(right, true, false, 2000 + KeyRepeat::GuardBounceMs, timing),
			"pressed again on its own once up, it steps");
		Press free;
		Check(KeyRepeat::Step(free, true, false, 0, KeyRepeat::From(Settings{}, true, false), true),
			"without the guard, a second arrow steps as before");
	}
	Passed();
	return 0;
}
