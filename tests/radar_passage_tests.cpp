#include "radar_passage.h"
#include "camp_grid.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>

using namespace D2RAccess;
using namespace Radar;
namespace {
constexpr int Size = 96;
const Position Down{std::sqrt(0.5), std::sqrt(0.5)};
World Field() {
	World world;
	world.session = world.level = 1; world.player = {40,40};
	world.bodyPattern = 1; world.walkingMask = 0x1C09;
	world.rooms = {{0,0,Size,Size,std::vector<uint16_t>(Size*Size)}};
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) { world.rooms[0].cells[y*Size+x] = flags; }
void Wall(World& world, int sum, uint16_t flags = 1) {
	for (int y=0; y<Size; ++y) if (sum-y >= 0 && sum-y < Size) Put(world,sum-y,y,flags);
}
Passage Analyze(const World& world) { return ScanPassage(Navigation::Build(world,PassageRadius({},world.bodyPattern)),Down,{}); }
bool Silent(Events events) { return std::all_of(events.begin(),events.end(),[](auto s) { return s == State::Unknown; }); }
void NativeBodies() {
	for (int pattern=0; pattern<=5; ++pattern) {
		auto world = Field(); world.bodyPattern = pattern;
		Check(Analyze(world).observed == State::Open, "every native body can advance in an open field");
		// A horizontal barrier with a one-cell point opening. A cross or box
		// must test the wall beside its centre, not inflate an endpoint ray.
		for (int x=0; x<Size; ++x) Put(world,x,44,1);
		Put(world,44,44,0);
		Check((Analyze(world).observed == State::Open) == (pattern == 0),
			"one-cell opening in a horizontal wall admits the native point footprint only");
	}
}
void OffsetAndDisconnected() {
	auto world = Field(); Wall(world,88);
	for (int y=0; y<Size; ++y) {
		const int x = 88-y;
		if (x >= 0 && x < Size && x-y >= 2 && x-y <= 8) Put(world,x,y,0);
	}
	Check(Scan(world,Down,{}).observed != State::Open && Analyze(world).observed == State::Open,
		"connectivity finds a body-wide slightly offset doorway that the centre ray misses");
	Engine engine;
	for (uint64_t now=1000; now<=1300; now+=50)
		Check(Silent(engine.Tick(world,now)), "an offset doorway never emits a misleading direct opening");
	world.player = {42,38}; (void)engine.Tick(world,1350);
	Check(engine.Tick(world,1400)[3] == State::Open && engine.Sensors()[3].probe.observed == State::Open,
		"aligning with the connected doorway announces a physically clear turn once");
	world = Field(); Wall(world,88);
	for (int y=0; y<Size; ++y) {
		const int x = 88-y;
		if (x >= 0 && x < Size && x-y >= 14 && x-y <= 24) Put(world,x,y,0);
	}
	const auto graph = Navigation::Build(world,20);
	Check(graph.At(48,48)->steps >= 0, "the distant door makes space behind the wall globally reachable");
	Check(Analyze(world).observed == State::Blocked,
		"global reachability through a distant side door is not a forward opening");
	Wall(world,88);
	Check(Navigation::Build(world,20).At(48,48)->steps < 0 && Analyze(world).observed == State::Blocked,
		"disconnected empty space behind a wall cannot count as a passage");
	world = Field(); Put(world,41,41,0x800);
	Check(Analyze(world).observed == State::Blocked,
		"a blocked first step cannot become open via a lateral or backward initial detour");
}
void UnknownAndRooms() {
	auto world = Field(); Wall(world,84,0x20);
	Check(!Analyze(world).available, "unknown cells across an early route are not classified as walls");
	world = Field(); Wall(world,104,0x20);
	Check(Analyze(world).observed == State::Open, "unknown distant rooms do not erase a proved local passage");
	world = Field(); world.rooms.clear();
	world.rooms.push_back({30,30,15,30,std::vector<uint16_t>(15*30)});
	world.rooms.push_back({45,30,15,30,std::vector<uint16_t>(15*30)});
	Check(Analyze(world).observed == State::Open, "a complete body route crosses resident room seams");
	world.rooms.pop_back();
	Check(!Analyze(world).available, "unloading the second room withdraws proof instead of inventing closure");
}
void StablePassages() {
	auto world = Field(); Wall(world,96); Wall(world,108);
	Engine engine; (void)engine.Tick(world,1000);
	unsigned events = 0;
	for (uint64_t tick=1; tick<=80; ++tick) {
		// Each facet persists for multiple polls, so this regression cannot
		// pass merely by extending a debounce timer.
		Wall(world,96,(tick/4)%2 ? 1 : 0);
		world.subtileFraction = tick%2 ? Position{0.9,0.1} : Position{0.1,0.9};
		events += !Silent(engine.Tick(world,1000+tick*50));
	}
	Check(events == 0, "twenty sustained far-wall changes produce no opening/closing spam");
	world = Field(); Put(world,41,41,0x1100); engine.Reset();
	(void)engine.Tick(world,1000); Put(world,41,41,0);
	for (uint64_t now=1050; now<=1350; now+=50)
		Check(Silent(engine.Tick(world,now)), "an actor clearing the initial baseline is not a newly opened wall");
}
void ActualPassagesAtSpeed() {
	for (int stride : {1,3,4}) {
		auto world = Field(); world.player = {25,40};
		for (int x=0; x<Size; ++x) {
			Put(world,x,25,1);
			if (x < 38 || x > 55) Put(world,x,36,1);
		}
		Engine engine; (void)engine.Tick(world,1000);
		std::array<int,DirectionCount> opened{},closed{};
		uint64_t now = 1000;
		for (int sign : {1,-1}) for (int distance=0; distance<48; distance+=stride) {
			world.player.x += sign*stride;
			const auto events = engine.Tick(world,now += stride*50);
			for (size_t i=0; i<DirectionCount; ++i) {
				opened[i] += events[i] == State::Open; closed[i] += events[i] == State::Blocked;
				if (events[i] == State::Open)
					Check(engine.Sensors()[i].probe.observed == State::Open,
						"every running opening has current full-body straight clearance");
			}
		}
		if (opened[0] != 2 || closed[0] != 2 || opened[2] != 2 || closed[2] != 2)
			std::cerr << "stride=" << stride << " up=" << opened[0] << '/' << closed[0]
				<< " right=" << opened[2] << '/' << closed[2] << '\n';
		Check(opened[0] == 2 && closed[0] == 2 && opened[2] == 2 && closed[2] == 2,
			"a real wall opening sounds once on entry/exit in each direction at 50/150/200 ms polls");
		Check(opened[1] == 0 && closed[1] == 0 && opened[3] == 0 && closed[3] == 0,
			"a side opening leaves the opposite open space silent");
	}
}
void HysteresisAndRecovery() {
	PassageTracker tracker;
	Passage clear{8,5,State::Open,true}, closed{4,5,State::Blocked,true}, edge{6,5,State::Unknown,true};
	Check(tracker.Observe(clear,true,1000,50) == State::Unknown, "a known clear baseline is silent");
	for (uint64_t now=1050; now<=1500; now+=50)
		Check(tracker.Observe(now%100 ? edge : clear,true,now,50) == State::Unknown,
			"one body span of uncertain boundary movement retains the same open passage");
	(void)tracker.Observe(closed,false,1550,50);
	Check(!tracker.Accepts(State::Open,closed,false), "the first blocked observation invalidates queued openings immediately");
	Check(tracker.Observe(closed,false,1600,50) == State::Blocked, "a persistent short route closes after two real observations");
	(void)tracker.Observe(clear,false,1650,50); (void)tracker.Observe(clear,false,1700,50);
	Check(tracker.Pending() == State::Open, "a connected but physically occupied entrance waits");
	(void)tracker.Observe(closed,false,1750,50);
	Check(tracker.Observe(closed,false,1800,50) == State::Unknown, "an opening that never cleared cannot leave an orphan closing");
	(void)tracker.Observe({},false,1850,50);
	Check(tracker.Current() == State::Unknown && tracker.Pending() == State::Unknown,
		"coverage loss discards both passage identity and pending announcements");
	Check(tracker.Observe(clear,true,1900,50) == State::Unknown, "restored clear coverage takes a fresh silent baseline");
}
void RecordedCamp() {
	auto world = RecordedCampGrid(); world.player = {4473,4621};
	const Position left{-Down.x,Down.y};
	const auto physical = Navigation::Build(world,PassageRadius({},world.bodyPattern));
	Check(physical.At(4466,4628)->body == State::Blocked,
		"the captured camp post still blocks the native body's side cell");
	Check(ScanPassage(physical,left,{}).observed == State::Open,
		"the real camp capture proves short usable travel before the post without fabricating the unseen map");
}
}
int main() {
	NativeBodies(); OffsetAndDisconnected(); UnknownAndRooms(); StablePassages();
	ActualPassagesAtSpeed(); HysteresisAndRecovery(); RecordedCamp(); Passed();
}
