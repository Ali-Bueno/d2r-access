#include "wall_bump.h"
#include "test_support.h"

#include <cmath>
#include <numbers>
#include <vector>

using namespace D2RAccess;
using namespace WallBump;
namespace {
constexpr uint64_t Poll = Radar::PollMs;
constexpr int Size = 40;
// Native PLAYER_PATH walking mask and the collision bits it selects.
constexpr uint16_t PlayerPath = 0x1C09, Wall = 1, Monster = 0x100, NoPath = 0x1000;

// A stick held into a wall: the game refuses every attempt and the player stands.
struct Push {
	Detector detector;
	Sample sample{.position = {10.5, 10.5}, .push = {1, 0}, .contact = true, .now = 1000};
	bool Begin() { return detector.Tick(sample); }
	bool Hold() { sample.attempts += 3; sample.now += Poll; return detector.Tick(sample); }
	bool Idle() { sample.now += Poll; return detector.Tick(sample); }
	bool Move(double dx, double dy) { sample.position.x += dx; sample.position.y += dy; return Hold(); }
	// Thuds heard over that long, one poll at a time.
	int HoldFor(uint64_t ms) { int thuds = 0; for (uint64_t t = 0; t < ms; t += Poll) thuds += Hold(); return thuds; }
	int IdleFor(uint64_t ms) { int thuds = 0; for (uint64_t t = 0; t < ms; t += Poll) thuds += Idle(); return thuds; }
};

void OncePerContact() {
	Push push;
	Check(!push.Begin(), "the first poll is a silent baseline");
	Check(push.Hold(), "a refused push into terrain, standing still, thuds");
	bool again = false;
	for (int poll = 0; poll < 40; ++poll) again |= push.Hold();
	Check(!again, "holding the stick into the same wall never thuds again");
	Push slide;
	slide.Begin();
	bool slid = false;
	for (int poll = 0; poll < 10; ++poll) slid |= slide.Move(0.3, 0);
	Check(!slid, "refused attempts while the game slides the player along are no contact");
	Check(slide.Hold(), "but the slide coming to rest against the wall is");
	Push creature;
	creature.sample.contact = false;
	creature.Begin();
	Check(!creature.Hold() && !creature.Hold(), "a push stopped by a creature, not terrain, is silent");
	Push free;
	free.Begin();
	Check(!free.Idle() && !free.Idle(), "standing by a wall without pushing is silent");
	Push unknown;
	unknown.sample.push = {};
	unknown.Begin();
	Check(!unknown.Hold(), "a refusal without a known direction is silent");
}

void Rearming() {
	const Settings settings;
	Push away;
	away.Begin();
	away.Hold();
	away.sample.contact = false;
	away.Move(-settings.moveDistance, 0);
	away.sample.contact = true;
	Check(!away.Move(settings.moveDistance, 0), "walking back into the wall is moving, not yet contact");
	Check(away.HoldFor(settings.repeatMs) == 1, "a step away re-arms: the next contact thuds once the interval allows");

	Push release;
	release.Begin();
	release.Hold();
	release.IdleFor(settings.releaseMs);
	Check(!release.Hold(), "a new push right after release still waits out the repeat interval");
	Check(release.HoldFor(settings.repeatMs) == 1, "then releasing the stick has re-armed it");
	Push brief;
	brief.Begin();
	brief.Hold();
	brief.IdleFor(settings.releaseMs - Poll);
	Check(brief.HoldFor(settings.repeatMs) == 0, "a pause shorter than the release is the same push");

	Push turn;
	turn.Begin();
	turn.Hold();
	Check(turn.HoldFor(settings.repeatMs) == 0, "a held push stays one contact");
	const double slight = std::numbers::pi / 6;
	turn.sample.push = {std::cos(slight), std::sin(slight)};
	Check(!turn.Hold(), "a push turned by less than 45 degrees meets the same wall");
	turn.sample.push = {0, 1};
	Check(turn.Hold(), "a push turned by more than 45 degrees meets another wall: a corner thuds twice");
	turn.sample.push = {1, 0};
	Check(!turn.Hold(), "turning straight back inside the repeat interval does not machine-gun");
	Check(turn.HoldFor(settings.repeatMs) == 1, "after the interval the turn back is its own contact");
}

void BrokenSequences() {
	const Settings settings;
	Push gap;
	gap.Begin();
	gap.Hold();
	gap.sample.attempts += 3;
	gap.sample.now += settings.maxGapMs + 1;
	Check(!gap.detector.Tick(gap.sample), "a long poll gap starts a silent baseline");
	Check(gap.Hold(), "and the next refusal is a fresh contact");
	Push restarted;
	restarted.Begin();
	restarted.Hold();
	restarted.sample.attempts = 0;
	Check(!restarted.Idle(), "a counter that went back (the hook restarted) is a baseline");
	Push reset;
	reset.Begin();
	reset.Hold();
	reset.detector.Reset();
	Check(!reset.Hold() && reset.Hold(), "a reset forgets the contact after its own baseline");
}

void ContactAndPan() {
	Radar::World world;
	world.player = {10, 10};
	world.subtileFraction = {0.5, 0.5};
	world.walkingMask = PlayerPath;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size)});
	const auto put = [&](int x, int y, uint16_t flags) { world.rooms[0].cells[static_cast<size_t>(y) * Size + x] = flags; };
	Check(!Contact(world, {1, 0}), "open ground is no contact");
	put(12, 10, Wall);
	Check(Contact(world, {1, 0}), "a wall two cells along the push is contact");
	Check(!Contact(world, {-1, 0}) && !Contact(world, {}), "the other way, or no push, is not");
	put(12, 10, 0);
	put(13, 10, Wall);
	Check(!Contact(world, {1, 0}), "a wall three cells away is not yet touching");
	put(11, 10, Monster | NoPath);
	Check(!Contact(world, {1, 0}), "a creature in the way is not terrain");
	put(11, 11, Wall);
	Check(Contact(world, {1, 1}), "a diagonal push meets the wall on its diagonal");
	Check(std::abs(Pan({1, -1}) - 1) < 1e-12 && std::abs(Pan({-1, 1}) + 1) < 1e-12, "screen right and left pan fully");
	Check(std::abs(Pan({-1, -1})) < 1e-12 && std::abs(Pan({1, 1})) < 1e-12, "screen up and down are centred");
	Check(std::abs(Pan({1, 0}) - 2 / std::sqrt(5.0)) < 1e-12, "a world axis pans by its screen share at the 2:1 projection");
	Check(Pan({}) == 0, "no push is centred");
}
}
int main() {
	OncePerContact();
	Rearming();
	BrokenSequences();
	ContactAndPan();
	Passed();
}
