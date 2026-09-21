#include "navigation_grid.h"
#include "camp_grid.h"
#include "test_support.h"
#include <algorithm>
#include <chrono>
#include <limits>

using namespace D2RAccess;
namespace {
Radar::World Field(int body = 0) {
	Radar::World w; w.session = w.level = 1; w.player = {20,20};
	w.bodyPattern = body; w.walkingMask = 0x1C09;
	w.rooms = {{0,0,50,50,std::vector<uint16_t>(2500)}};
	return w;
}
void Put(Radar::World& w, int x, int y, uint16_t flags = 1) { w.rooms[0].cells[y*50+x] = flags; }
bool Reached(const Navigation::Grid& grid, int x, int y) { const auto* c=grid.At(x,y); return c && c->steps >= 0; }
void WallAndDoorway() {
	auto w = Field(); for (int x=0; x<50; ++x) Put(w,x,16);
	auto grid = Navigation::Build(w,12);
	Check(!Reached(grid,20,12) && grid.At(20,12)->body == Radar::State::Open, "empty space behind a complete wall is not reachable");
	Check(grid.At(20,17)->exits & Navigation::PlusY, "the wall still allows travel away from it");
	for (int x=25; x<=27; ++x) Put(w,x,16,0);
	grid = Navigation::Build(w,12);
	Check(Reached(grid,20,12), "an offset doorway connects the region despite central straight-ray obstruction");
	Check(w.Body(static_cast<int>(w.player.x),16) == Radar::State::Blocked,
		"reachability through a side doorway does not change the true straight-line collision");
	Check(grid.At(20,12)->steps > 8, "the detour's destination-step count exceeds the direct displacement");
	for (int x=25; x<=27; ++x) Put(w,x,16,0x800);
	Check(!Reached(Navigation::Build(w,12),20,12), "closing the actual door disconnects the region on the next snapshot");
}
void FirstDirection() {
	auto w=Field(); w.player={20,14};
	for (int y=0; y<30; ++y) Put(w,23,y);
	const auto grid=Navigation::Build(w,20);
	const auto* target=grid.At(27,14);
	Check(target && target->steps > 0, "a destination across a wall remains reachable by going around its far end");
	Check(!(target->firstSteps & Navigation::PlusX), "a reachable destination to +X does not imply that +X starts a shortest route");
	Check(target->firstSteps & (Navigation::PlusY|Navigation::MinusXPlusY|Navigation::PlusXPlusY),
		"first-step metadata identifies movement toward the actual detour entrance");
}
void CoarseCellTrap() {
	auto w=Field(); w.player={10,20};
	std::fill(w.rooms[0].cells.begin(),w.rooms[0].cells.end(),uint16_t{1});
	for (int y=18; y<23; ++y) for (int x=8; x<23; ++x) Put(w,x,y,x == 15 ? 1 : 0);
	// Three 5x5 nodes: the two four-cell seam strips accepted by d2mapapi
	// are clear, but a wall inside the middle node separates those strips.
	bool seams=true;
	for (int x : {11,12,13,14,16,17,18,19}) seams &= w.Cell(x,20) == Radar::State::Open;
	Check(seams, "both coarse-node seam strips can be clear despite an internal partition");
	const auto grid=Navigation::Build(w,12);
	Check(grid.At(20,20)->body == Radar::State::Open && !Reached(grid,20,20),
		"native-resolution connectivity rejects a false connection through a divided 5x5 coarse node");
}
void BodyAndDiagonals() {
	auto w=Field(); for (int x=0; x<50; ++x) Put(w,x,16);
	Put(w,25,16,0);
	Check(Reached(Navigation::Build(w,12),20,12), "a point body fits through a one-cell opening");
	w.bodyPattern=1;
	Check(!Reached(Navigation::Build(w,12),20,12), "the native cross body cannot pass through the same point-sized hole");
	// Two walls touching at a corner, with a clear cell behind it: only the body
	// test on the destination decides the diagonal step, and the cross covers
	// both wall cells, so it cannot squeeze between them.
	for (int pattern=0; pattern<=5; ++pattern) {
		w=Field(pattern); Put(w,21,20); Put(w,20,21);
		const auto grid=Navigation::Build(w,5);
		Check(w.Cell(21,21) == Radar::State::Open && w.Cell(21,20) == Radar::State::Blocked
			&& w.Cell(20,21) == Radar::State::Blocked, "the corner really is two diagonally touching walls with a clear cell behind");
		Check((grid.At(21,21)->steps == 1) == (pattern == 0), "native diagonal destination rules use the actual body instead of a generic corner-cut rule");
		Check((grid.At(21,21)->body == Radar::State::Open) == (pattern == 0),
			"the destination's own body test is what refuses the corner cut");
	}
	w=Field(); Put(w,20,20);
	const auto escape=Navigation::Build(w,5);
	Check(escape.At(21,20)->steps == 1 && !(escape.At(21,20)->exits & Navigation::MinusX),
		"a native origin overlap permits escape but is not traversable from neighboring cells");
}
void ClearanceAndCoverage() {
	auto w=Field(1);
	const int room=Navigation::Build(w,12).At(20,20)->clearance;
	for (int x=0; x<50; ++x) { Put(w,x,17); Put(w,x,23); }
	const auto corridor=Navigation::Build(w,12);
	Check(corridor.At(20,20)->clearance < room && Reached(corridor,29,20), "clearance distinguishes a traversable narrow corridor from a large room");
	Put(w,22,20,0x20);
	const auto unknown=Navigation::Build(w,12);
	Check(!Reached(unknown,22,20) && unknown.touchesUnknown, "unknown terrain stays separate from blocked and is never crossed");
	Check(unknown.touchesWindow, "local reachability records its clipped analysis horizon");
	Put(w,20,20,0x20);
	const auto missing=Navigation::Build(w,12);
	Check(!missing.originKnown && missing.reachable == 0, "an unknown origin does not seed a fictitious connected region");
}
void RoomSeamsAndIdentity() {
	auto w=Field(1); const auto original=w.rooms[0]; w.rooms.clear();
	for (int left : {0,25}) {
		Radar::RoomGrid room{left,0,25,50,std::vector<uint16_t>(1250)};
		for (int y=0; y<50; ++y) for (int x=0; x<25; ++x) room.cells[y*25+x]=original.cells[y*50+left+x];
		w.rooms.push_back(std::move(room));
	}
	Check(Reached(Navigation::Build(w,12),30,20), "adjacent active-room grids stitch in absolute subtile coordinates");
	w.player.x=26;
	Check(Reached(Navigation::Build(w,12),20,20), "crossing a room boundary keeps the same world coordinates and return connection");
	w.player.x=20; w.rooms.pop_back();
	Check(!Reached(Navigation::Build(w,12),30,20), "an unloaded neighboring room cannot retain old reachable cells");
	w=Field(); w.session=2; w.level=3;
	const auto fresh=Navigation::Build(w,12);
	Check(fresh.session == 2 && fresh.level == 3, "each result carries its own world identity without stale level state");
	const auto saved=w.rooms[0].cells; (void)Navigation::Build(w,12);
	Check(w.rooms[0].cells == saved, "navigation analysis never changes its physical source snapshot");
}
void Camp(bool dump) {
	auto w=RecordedCampGrid(); const auto grid=Navigation::Build(w,12);
	Check(grid.originKnown && grid.reachable > 0, "the actual camp capture supplies a connected player region");
	Check(!Reached(grid,4466,4629) && !Reached(grid,4466,4628), "the captured post and its full-body side hit remain physically impassable");
	Check(Reached(grid,4469,4631), "the recorded camp has a real route around the post without erasing it");
	Check(grid.touchesUnknown, "the uncaptured map outside the camp fixture remains unknown");
	if (dump) std::cout << "camp player=" << grid.playerX << ',' << grid.playerY << " reachable=" << grid.reachable << '\n' << grid.Dump();
}
void BoundsAndCost() {
	auto w=Field();
	Check(Navigation::Build(w,0).cells.empty() && Navigation::Build(w,Navigation::MaxRadius+1).cells.empty(), "invalid analysis radii cannot allocate an unbounded grid");
	// A request above the cap returns an EMPTY grid rather than a clamped one,
	// so the widest accepted request has to still be answered: the map survey
	// asked for more than this and reported every object as unreachable.
	const auto widest = Navigation::Build(w,Navigation::MaxRadius);
	Check(!widest.cells.empty() && widest.originKnown, "the widest accepted radius still builds a graph");
	Check(widest.At(20,20) && widest.At(20,20)->steps == 0, "the player stands at zero steps in the widest graph");
	w.player.x=std::numeric_limits<double>::quiet_NaN();
	Check(Navigation::Build(w,12).cells.empty(), "invalid player coordinates fail without a graph");
	w=Field(1); constexpr int Iterations=200;
	const auto start=std::chrono::steady_clock::now();
	for (int i=0; i<Iterations; ++i) (void)Navigation::Build(w,20);
	const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/Iterations;
	std::cout << "local navigation build ms=" << ms << '\n';
	Check(ms < Radar::PollMs, "the local graph experiment fits the existing radar poll budget");
}
}
int main(int argc, char** argv) {
	if (argc > 1 && std::string_view(argv[1]) == "camp") { Camp(true); Passed(); return 0; }
	WallAndDoorway(); FirstDirection(); CoarseCellTrap(); BodyAndDiagonals(); ClearanceAndCoverage(); RoomSeamsAndIdentity(); Camp(false); BoundsAndCost();
	Passed();
}
