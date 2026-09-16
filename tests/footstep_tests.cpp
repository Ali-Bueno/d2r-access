#include "footsteps.h"
#include "test_support.h"

using namespace D2RAccess::Footsteps;
int main() {
	Filter filter;
	Sample player{42, 0x00648000006E8000ULL, true, true, true};
	Check(!filter.Suppress(player, 1000), "the first local footfall is preserved");
	Check(!filter.Suppress(player, 1050), "a duplicate animation event is not immediate proof of a stall");
	Check(filter.Suppress(player, 1200), "repeated local walking steps without displacement become silent");
	Check(filter.Suppress(player, 1500), "continued blocked footfalls stay silent");
	++player.position;
	Check(!filter.Suppress(player, 1800), "one fixed-point fractional bit of motion immediately restores footsteps");
	for (uint64_t tick = 1; tick <= 8; ++tick) {
		++player.position;
		Check(!filter.Suppress(player, 1800 + tick * 250), "very slow real movement is never treated as a collision");
	}
	Check(filter.Suppress(player, 4050), "a later real stop can become silent again");
	auto npc = player; npc.entity = 99; npc.local = false;
	Check(!filter.Suppress(npc, 4200), "nearby NPC footfalls retain their normal sounds");
	Check(filter.Suppress(player, 4250), "NPC footfalls cannot reset the player's stalled baseline");
	player.locomotion = false;
	Check(!filter.Suppress(player, 4300), "attack, cast and other non-walking animation events remain audible");
	player.locomotion = true;
	Check(!filter.Suppress(player, 4400), "walking after an action starts with a fresh audible footfall");
	Check(filter.Suppress(player, 4650), "a fresh walking sequence still detects a sustained stall");
	Check(!filter.Suppress({}, 4700), "unreadable game state preserves stock audio");
	Check(!filter.Suppress(player, 4750), "an unreadable interval cannot leave footsteps stuck muted");
	Check(filter.Suppress(player, 4950), "stall detection resumes after valid observations");
	++player.entity;
	Check(!filter.Suppress(player, 5000), "a new entity generation never inherits a previous stall");
	Check(filter.Suppress(player, 5200), "new identity can independently become stalled");
	Check(!filter.Suppress(player, 7000), "pause or interaction gaps rebase the footfall sequence");
	Check(!filter.Suppress(player, 100), "clock discontinuities preserve normal sound");
	filter.Reset();
	Check(!filter.Suppress(player, 200), "disabling and re-enabling starts from a clear baseline");
	Passed();
}
