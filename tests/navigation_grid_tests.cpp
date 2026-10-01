#include "navigation_grid.h"
#include "camp_grid.h"
#include "test_support.h"
#include <algorithm>
#include <array>
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
void SameAsTheWorld() {
	// Build copies the rooms into one flat field; World::Cell and World::Body,
	// which search the room list per cell, stay the rule it must agree with.
	uint32_t seed = 12345;
	const auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
	for (int pattern = 0; pattern <= 5; ++pattern) {
		Radar::World w; w.session = w.level = 1; w.player = {60, 60};
		w.bodyPattern = pattern; w.walkingMask = 0x1C09;
		// Rooms that overlap, leave gaps, and reach past the window on every side.
		for (const auto& box : {std::array{40, 40, 30, 30}, std::array{55, 35, 40, 20}, std::array{20, 60, 45, 45},
			std::array{70, 50, 25, 60}, std::array{0, 0, 30, 30}}) {
			Radar::RoomGrid room{box[0], box[1], box[2], box[3], std::vector<uint16_t>(static_cast<size_t>(box[2]) * box[3])};
			for (auto& cell : room.cells) {
				static constexpr uint16_t Kinds[] = {0, 0, 0, 0, 0, 0x0007, 0x0020, 0x1080, 0x0080, 0x1100, 0x1000, 0x00C0, 0x0800, 0x2100, 0x8000};
				cell = Kinds[next() % std::size(Kinds)];
			}
			w.rooms.push_back(std::move(room));
		}
		w.rooms[2].cells.resize(w.rooms[2].cells.size() - 100); // a short grid reads as unknown, not past its end
		for (const auto layer : {Radar::Layer::Movement, Radar::Layer::Terrain}) {
			const auto grid = Navigation::Build(w, 45, layer);
			bool same = true;
			for (int y = grid.y; same && y < grid.y + grid.height; ++y) for (int x = grid.x; x < grid.x + grid.width; ++x) {
				uint16_t flags = 0, blocker = 0;
				(void)w.Cell(x, y, layer, &flags);
				const auto body = w.Body(x, y, layer, &blocker);
				const auto* cell = grid.At(x, y);
				if (cell->flags != flags || cell->body != body || cell->blocker != blocker) { same = false; break; }
			}
			Check(same, "the grid's cells are the world's own, for every body and both layers");
		}
	}
}
void Camp(bool dump) {
	auto w=RecordedCampGrid(); const auto grid=Navigation::Build(w,12);
	Check(grid.originKnown && grid.reachable > 0, "the actual camp capture supplies a connected player region");
	Check(!Reached(grid,4466,4629) && !Reached(grid,4466,4628), "the captured post and its full-body side hit remain physically impassable");
	Check(Reached(grid,4469,4631), "the recorded camp has a real route around the post without erasing it");
	Check(grid.touchesUnknown, "the uncaptured map outside the camp fixture remains unknown");
	if (dump) std::cout << "camp player=" << grid.playerX << ',' << grid.playerY << " reachable=" << grid.reachable << '\n' << grid.Dump();
}
// What the game's single-cell reset does to the map, for each cell a marker names.
void TakeOff(Radar::World& w, const Radar::LeftMarker& marker) {
	for (const auto& cell : Radar::MarkerCells(w, marker)) w.rooms[0].cells[cell.y*50+cell.x] &= static_cast<uint16_t>(~cell.mask);
}
bool Has(const std::vector<Radar::LeftMarker>& left, int x, int y) {
	return std::any_of(left.begin(), left.end(), [&](const Radar::LeftMarker& m) { return m.x == x && m.y == y; });
}
void LeftMarkers() {
	const std::vector<Radar::Position> nobody;
	// The 2026-09-29 stuck line at 5851,4970, rows y-2..y+2, the player's own
	// stamp already taken out: two left north and west, the shrine south.
	const uint16_t boxed[5][5] = {
		{0x0000, 0x0000, 0x8080, 0x8000, 0x8000},
		{0x0000, 0x0080, 0x1080, 0x8080, 0x0000},
		{0x0080, 0x1080, 0x0000, 0x0000, 0x0000},
		{0x0000, 0x0080, 0x0400, 0x0400, 0x0400},
		{0x0000, 0x0000, 0x0400, 0x0400, 0x0400}};
	const auto place = [](Radar::World& w, const uint16_t rows[5][5]) {
		for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) Put(w, 20 + dx, 20 + dy, rows[dy + 2][dx + 2]);
	};
	auto w = Field(1);
	place(w, boxed);
	Check(Radar::LeftMarkers(w, nobody, false).empty(), "a stamp on the cells round the player is left alone until a move is refused");
	auto left = Radar::LeftMarkers(w, nobody, true);
	Check(left.size() == 2 && Has(left, 20, 19) && Has(left, 19, 20), "both stamps left beside the player are found once one is");
	int open = 0;
	for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) open += (dx || dy) && w.Body(20 + dx, 20 + dy) == Radar::State::Open;
	Check(open == 0, "with them and the shrine no first step is open");
	for (const auto& marker : left) TakeOff(w, marker);
	Check(w.Body(20, 19) == Radar::State::Open && w.Body(19, 20) == Radar::State::Open, "taken off, north and west are open");
	Check(w.rooms[0].cells[18*50+20] == 0x8000 && w.rooms[0].cells[19*50+21] == 0x8000, "a corpse under an arm keeps its own bit");
	Check(w.rooms[0].cells[21*50+20] == 0x0400, "the shrine is not touched");
	Check(Radar::LeftMarkers(w, nobody, true).empty(), "nothing is found once they are off");

	// 2026-09-30 10:59:04 at 22712,6680: the stamp a cell north, its east arm
	// already gone. The first remedy wanted a whole cross and took nothing off.
	const uint16_t partial[5][5] = {
		{0x0007, 0x0000, 0x0080, 0x0000, 0x0000},
		{0x0007, 0x0080, 0x1080, 0x0000, 0x0000},
		{0x0007, 0x0000, 0x0000, 0x0000, 0x0000},
		{0x0007, 0x0000, 0x0000, 0x0000, 0x0000},
		{0x0007, 0x0000, 0x0000, 0x0000, 0x0000}};
	w = Field(1);
	place(w, partial);
	Check(w.Body(20, 19) == Radar::State::Blocked, "a stamp with an arm gone still refuses the step onto it");
	left = Radar::LeftMarkers(w, nobody, true);
	Check(left.size() == 1 && Has(left, 20, 19), "a stamp with an arm gone is found");
	TakeOff(w, left.front());
	Check(w.Body(20, 19) == Radar::State::Open && w.rooms[0].cells[19*50+19] == 0 && w.rooms[0].cells[18*50+20] == 0
		&& w.rooms[0].cells[20*50+18] == 0x0007, "it comes off with what is left of its arms, and the wall stays");

	// 2026-09-30 11:33:06: a marker with nothing round it, two cells from the player.
	w = Field(1);
	Put(w, 22, 18, 0x1000);
	Check(w.Cell(22, 18, Radar::Layer::Terrain) == Radar::State::Blocked, "a bare marker is a wall to the routes");
	left = Radar::LeftMarkers(w, nobody, false);
	Check(left.size() == 1 && Has(left, 22, 18), "a bare marker away from the player is found without a refusal");
	Check(Radar::MarkerCells(w, left.front()).size() == 1 && Radar::MarkerCells(w, left.front()).front().mask == 0x1000,
		"only its own bit is taken off, and no cell beside it");
	const std::vector<Radar::Position> beside{{23, 19}};
	Check(Radar::LeftMarkers(w, beside, false).empty(), "a marker within a cell of a unit is that unit's");

	// What is never a left marker.
	w = Field(1);
	Put(w, 30, 30, 0x1100); Put(w, 32, 30, 0x3000); Put(w, 34, 30, 0x10C0); Put(w, 36, 30, 0x1020);
	Check(Radar::LeftMarkers(w, nobody, true).empty(), "a monster's marker, a pet's, water and an unknown cell are left alone");
	Put(w, 20, 20, 0x1000);
	Check(Radar::LeftMarkers(w, nobody, true).empty(), "the player's own cell is never listed");
	w = Field(1);
	Put(w, 30, 30, 0x1080); Put(w, 29, 30, 0x0080); Put(w, 31, 30, 0x0080); Put(w, 30, 29, 0x0080); Put(w, 30, 31, 0x0080);
	const std::vector<Radar::Position> other{{30, 30}};
	Check(Radar::LeftMarkers(w, other, false).empty(), "another player's stamp under that player is left alone");
	Check(Radar::LeftMarkers(w, nobody, false).size() == 1, "the same stamp with no player on it is a left one");
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
	WallAndDoorway(); FirstDirection(); CoarseCellTrap(); BodyAndDiagonals(); ClearanceAndCoverage(); RoomSeamsAndIdentity(); SameAsTheWorld(); Camp(false); LeftMarkers(); BoundsAndCost();
	Passed();
}
