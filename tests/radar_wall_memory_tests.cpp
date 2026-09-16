#include "radar.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>

using namespace D2RAccess::Radar;
namespace {
constexpr int Size = 400, Centre = Size/2;
constexpr std::array<Position,DirectionCount> Steps{{{-1,-1},{-1,1},{1,-1},{1,1}}};
World Field() {
	World world; world.session = world.level = 1;
	world.player = {Centre,Centre}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
	world.rooms = {{0,0,Size,Size,std::vector<uint16_t>(Size*Size)}};
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) { world.rooms[0].cells[y*Size+x] = flags; }
void Barrier(World& world, Position axis, bool doorway = false, uint16_t door = 0) {
	for (int y=0; y<Size; ++y) for (int x=0; x<Size; ++x) {
		const double forward = (x-Centre)*axis.x + (y-Centre)*axis.y;
		const double side = -(x-Centre)*axis.y + (y-Centre)*axis.x;
		if (forward == 6) Put(world,x,y,doorway && std::abs(side) <= 6 ? door : 1);
	}
}
bool Silent(Events events) { return std::all_of(events.begin(),events.end(),[](auto s) { return s == State::Unknown; }); }
void AllDirections() {
	for (size_t direction=0; direction<DirectionCount; ++direction) {
		const auto step = Steps[direction]; auto world = Field(); Barrier(world,step);
		world.player = {Centre-12*step.x,Centre-12*step.y};
		Engine engine; (void)engine.Tick(world,1000); uint64_t now = 1000;
		unsigned openings = 0, closures = 0;
		const auto tick = [&] {
			const auto event = engine.Tick(world,now+=50)[direction];
			openings += event == State::Open; closures += event == State::Blocked;
		};
		for (int distance=0; distance<12; ++distance) { world.player.x+=step.x; world.player.y+=step.y; tick(); }
		tick(); Check(closures == 1, "approaching an initially distant wall still warns once in every direction");
		for (int trip=0; trip<2; ++trip) {
			for (int distance=0; distance<60; ++distance) { world.player.x-=step.x; world.player.y-=step.y; tick(); }
			Check(engine.Sensors()[direction].probe.observed == State::Open,
				"retreat really creates physical clearance toward the still-existing wall");
			Check(engine.Sensors()[direction].structure.end == State::Unknown && engine.Sensors()[direction].wallMemory.Known(),
				"the remembered wall survives leaving the structural snapshot window");
			for (int distance=0; distance<20; ++distance) tick();
			for (int distance=0; distance<60; ++distance) { world.player.x+=step.x; world.player.y+=step.y; tick(); }
			tick();
		}
		Check(openings == 0 && closures == 1,
			"retreat, stopping and returning preserve a known wall without invented exits or repeated closures");
	}
}
void RecordedRetreat() {
	// Inferred wall regression, not a reconstructed full map: latest live
	// left events at (4819,4770) and (4821,4767) hit body centres on Y=4774,
	// at 4.5 and 9.0 subtiles. A cross beside a wall on Y=4775 reproduces them.
	auto world = Field(); world.rooms = {{4700,4600,Size,Size,std::vector<uint16_t>(Size*Size)}};
	for (int x=0; x<Size; ++x) world.rooms[0].cells[(4775-4600)*Size+x] = 5;
	world.player = {4819,4770}; Engine engine; (void)engine.Tick(world,1000);
	Check(engine.Sensors()[1].probe.clearDepth == 4.5 && engine.Sensors()[1].structure.hit.y == 4774,
		"the inferred wall reproduces the logged near left hit");
	world.player = {4821,4767}; (void)engine.Tick(world,1050);
	Check(engine.Sensors()[1].probe.clearDepth == 9 && engine.Sensors()[1].structure.hit.y == 4774,
		"diagonal retreat reproduces the logged far left hit on the same wall");
	for (uint64_t now=1100; now<=2000; now+=50)
		Check(engine.Tick(world,now)[1] == State::Unknown && !engine.Sensors()[1].newOpening,
			"the recorded retreat pattern is silent even after stopping beyond the opening threshold");
}
void DoorBeyondRememberedWall() {
	auto world = Field(); const auto step = Steps[3]; Barrier(world,step);
	Engine engine; (void)engine.Tick(world,1000); uint64_t now = 1000;
	for (int i=0; i<12; ++i) { --world.player.x; --world.player.y; (void)engine.Tick(world,now+=50); }
	Barrier(world,step,true,0x20);
	for (int i=0; i<6; ++i)
		Check(engine.Tick(world,now+=50)[3] == State::Unknown,
			"unknown coverage at the remembered wall cannot become a distant exit");
	Barrier(world,step,true);
	Put(world,199,199,0x1100); // before the old wall, but beyond eight free subtiles
	(void)engine.Tick(world,now+=50); (void)engine.Tick(world,now+=50);
	Check(engine.Sensors()[3].guidance.openingSafe && engine.Sensors()[3].newOpening
		&& engine.Sensors()[3].cueState == State::Unknown,
		"a real distant doorway waits when an actor blocks travel beyond the short physical range");
	Put(world,199,199,0);
	Check(engine.Tick(world,now+=50)[3] == State::Open,
		"a real door through the remembered wall announces once its full approach clears");
	for (int i=0; i<8; ++i) Check(Silent(engine.Tick(world,now+=50)), "the newly confirmed exit does not repeat at rest");
}
void LateralExitAfterRetreat() {
	for (bool wallEnd : {false,true}) for (size_t direction=0; direction<DirectionCount; ++direction) {
		auto world = Field(); const auto step = Steps[direction]; Barrier(world,step);
		Engine engine; (void)engine.Tick(world,1000); uint64_t now=1000;
		for (int i=0; i<8; ++i) { world.player.x-=step.x; world.player.y-=step.y; (void)engine.Tick(world,now+=50); }
		// A real gap lies along the wall, away from the original ray.
		for (int y=0; y<Size; ++y) for (int x=0; x<Size; ++x) {
			const double forward=(x-Centre)*step.x+(y-Centre)*step.y;
			const double side=-(x-Centre)*step.y+(y-Centre)*step.x;
			if (forward == 6 && side >= 14 && (wallEnd || side <= 30)) Put(world,x,y,0);
		}
		unsigned openings=0;
		for (int i=0; i<11; ++i) {
			world.player.x-=step.y; world.player.y+=step.x;
			openings += engine.Tick(world,now+=50)[direction] == State::Open;
		}
		openings += engine.Tick(world,now+=50)[direction] == State::Open;
		Check(openings == 1, "moving along a remembered wall still finds a real doorway or wall end after retreat");
	}
}
void PlaneConfirmationAndReset() {
	WallMemory memory; Settings settings;
	Probe wall; wall.end = State::Blocked; wall.clearDepth = 3; wall.hit = {104,100}; wall.surfaceNormal = {1,0};
	Check(!memory.Update(wall,{100,100},{1,0},true,1000,settings), "a nearby wall establishes a world-space reference");
	wall.clearDepth = 12; wall.hit = {104,101};
	Check(!memory.Update(wall,{91,101},{1,0},false,1050,settings), "tangential drift of the same hit cannot prove an exit");
	wall.hit = {124,101}; wall.clearDepth = 32;
	Check(!memory.Update(wall,{91,101},{1,0},false,1100,settings), "crossing the old plane starts independent geometric confirmation");
	wall.end = State::Unknown; wall.clearDepth = 9;
	Check(!memory.Update(wall,{91,101},{1,0},false,1150,settings) && memory.Known(),
		"a partial map before the old wall cancels confirmation without forgetting the wall");
	wall.end = State::Blocked; wall.clearDepth = 32;
	Check(!memory.Update(wall,{91,101},{1,0},false,1200,settings), "restored geometric proof needs a fresh first sample");
	Check(memory.Update(wall,{91,101},{1,0},false,1250,settings), "two fresh beyond-wall samples confirm a new opening");
	WallMemory unfitted; wall.hit = {104,100}; wall.surfaceNormal = {}; wall.clearDepth = 3;
	(void)unfitted.Update(wall,{100,100},{1,0},true,1000,settings);
	wall.clearDepth = 12;
	Check(!unfitted.Update(wall,{91,100},{1,0},false,1050,settings),
		"an ambiguous wall without a fitted normal still retains the same world hit during retreat");
	auto world = Field(); Barrier(world,Steps[3]); Engine engine; (void)engine.Tick(world,1000);
	world = Field(); ++world.session;
	Check(Silent(engine.Tick(world,1050)) && !engine.Sensors()[3].wallMemory.Known(),
		"session reset cannot carry a previous world's remembered wall into the new game");
}
}
int main() { AllDirections(); RecordedRetreat(); DoorBeyondRememberedWall(); LateralExitAfterRetreat(); PlaneConfirmationAndReset(); Passed(); }
