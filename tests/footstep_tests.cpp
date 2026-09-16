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
	auto unresolved = npc; unresolved.valid = false;
	Check(!filter.Suppress(unresolved, 4260), "an unresolved foreign entity keeps its original sound path");
	Check(filter.Suppress(player, 4270), "an unresolved foreign entity must not restart audible local footsteps");
	unresolved.entity = player.entity ^ (1U << 20); // different full ECS generation
	Check(!filter.Suppress(unresolved, 4280), "an unresolved different ECS generation remains unrelated");
	Check(filter.Suppress(player, 4290), "generation-distinct foreign events cannot clear a local stall");
	Check(filter.LastDecision().stationaryMs == 490, "foreign events do not change the measured stationary interval");
	player.locomotion = false;
	Check(!filter.Suppress(player, 4300), "attack, cast and other non-walking animation events remain audible");
	player.locomotion = true;
	Check(!filter.Suppress(player, 4400), "walking after an action starts with a fresh audible footfall");
	Check(filter.Suppress(player, 4650), "a fresh walking sequence still detects a sustained stall");
	auto unreadable = player; unreadable.valid = false;
	Check(!filter.Suppress(unreadable, 4700), "unreadable local game state preserves stock audio");
	Check(!filter.Suppress(player, 4750), "an unreadable interval cannot leave footsteps stuck muted");
	Check(filter.Suppress(player, 4950), "stall detection resumes after valid observations");
	++player.entity;
	Check(!filter.Suppress(player, 5000), "a new entity generation never inherits a previous stall");
	Check(filter.Suppress(player, 5200), "new identity can independently become stalled");
	unresolved.entity = player.entity;
	Check(!filter.Suppress(unresolved, 5300), "an unresolved event with the local ECS identity fails audible");
	Check(!filter.Suppress(player, 5400), "the local unresolved identity must clear a stale muted baseline");
	Check(filter.Suppress(player, 5600), "valid stationary local observations restore suppression");
	Check(!filter.Suppress(player, 7000), "pause or interaction gaps rebase the footfall sequence");
	Check(!filter.Suppress(player, 100), "clock discontinuities preserve normal sound");
	filter.Reset();
	Check(!filter.Suppress(player, 200), "disabling and re-enabling starts from a clear baseline");
	player.position += (uint64_t{1} << 32);
	Check(!filter.Suppress(player, 400), "fractional Y-only sliding remains audible");
	Check(filter.LastDecision().reason == Reason::Moved && filter.LastDecision().dx == 0 && filter.LastDecision().dy == 1,
		"diagnostics distinguish fractional sliding from a completely stationary footfall");
	Passed();
}
