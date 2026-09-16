#include "radar.h"
#include "test_support.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace D2RAccess::Radar;
namespace {
constexpr int Size = 240;
// Native PLAYER_PATH bits: WALL | NO_PLAYER | OBJECT | DOOR | NO_PATH.
constexpr uint16_t PlayerPath = 0x1C09, Wall = 1, Object = 0x400, Door = 0x800;
World Field(int pattern = 1) {
	World w; w.session = w.level = 1; w.player = {100,100};
	w.bodyPattern = pattern; w.walkingMask = PlayerPath;
	w.rooms.push_back({0,0,Size,Size,std::vector<uint16_t>(Size*Size)});
	return w;
}
void Put(World& w, int x, int y, uint16_t flags) { w.rooms[0].cells[y*Size+x] = flags; }
void Rectangle(World& w, int x, int y, int width, int height, uint16_t flags) {
	for (int dy=0; dy<height; ++dy) for (int dx=0; dx<width; ++dx) Put(w,x+dx,y+dy,flags);
}
bool Silent(const Events& events) {
	return std::all_of(events.begin(), events.end(), [](State s) { return s == State::Unknown; });
}
void IsolatedObjects() {
	for (int pattern : {0,1,2,3,4,5}) for (uint16_t flags : {Wall,Object,uint16_t(Wall|Object)}) {
		auto w = Field(pattern); Rectangle(w,108,108,3,2,flags);
		const auto view = StructuralWorld(w,{});
		Check(view.Cell(108,108) == State::Open, "bypassable small furniture is absent from automatic structural geometry for every native body pattern");
		Check(w.Body(108,108) == State::Blocked, "structural filtering never changes the physical source map");
	}
	auto w = Field(); Rectangle(w,108,104,7,12,Object);
	Check(StructuralWorld(w,{}).Cell(108,104) == State::Open, "a larger isolated object can be bypassed without pretending it is a room wall");
	Rectangle(w,108,104,7,12,Wall);
	Check(StructuralWorld(w,{}).Cell(108,104) == State::Blocked, "a building larger than one map tile is preserved despite an exterior route around it");
	Rectangle(w,108,104,7,12,0); Rectangle(w,105,105,21,2,Object);
	Check(StructuralWorld(w,{}).Cell(105,105) == State::Blocked, "objects extending beyond the local classification scale remain structural");
}
void Barriers() {
	for (uint16_t flags : {Door,uint16_t(0x8),uint16_t(0x1000),uint16_t(Object|Door),uint16_t(Wall|0x8)}) {
		auto w = Field(); Put(w,105,105,flags);
		Check(StructuralWorld(w,{}).Cell(105,105) == State::Blocked, "doors, no-player barriers and independent no-path cells are never treated as furniture");
	}
	for (int pattern : {1,2}) {
		auto w = Field(pattern);
		Rectangle(w,0,100,Size,1,Wall); Rectangle(w,0,106,Size,1,Wall);
		Put(w,108,103,Object);
		Check(StructuralWorld(w,{}).Cell(108,103) == State::Blocked,
			"furniture plugging a corridor stays structural when the apparent gaps do not fit the body");
		w.bodyPattern = 0;
		Check(StructuralWorld(w,{}).Cell(108,103) == State::Open, "a real smaller body may fit around that same obstacle");
	}
	auto w = Field();
	for (int n=70; n<=130; ++n) Put(w,n,n,Wall);
	Check(StructuralWorld(w,{}).Cell(105,105) == State::Blocked, "diagonally connected wall cells remain one barrier");
	Rectangle(w,0,100,Size,1,Wall); Put(w,105,101,Object);
	Check(StructuralWorld(w,{}).Cell(105,101) == State::Blocked, "clutter attached to an extended wall is retained conservatively");
	w = Field();
	Rectangle(w,104,104,5,5,Wall); Rectangle(w,105,105,3,3,0);
	Check(w.Body(106,106) == State::Open && StructuralWorld(w,{}).Cell(104,104) == State::Blocked,
		"an enclosed walkable pocket is preserved even when its entire exterior can be bypassed");
}
void CoverageAndActors() {
	auto w = Field(); Put(w,108,108,Object); Put(w,111,108,0x20);
	Check(StructuralWorld(w,{}).Cell(108,108) == State::Blocked, "unknown detour coverage cannot justify ignoring a physical obstacle");
	Put(w,111,108,0);
	const auto before = StructuralWorld(w,{});
	// MONSTER | NO_PATH | PET is a native actor stamp, not a new wall.
	Rectangle(w,106,106,5,1,0x3100); Put(w,108,108,Object|0x3100);
	const auto after = StructuralWorld(w,{});
	Check(before.Cell(108,108) == State::Open && after.Cell(108,108) == State::Open,
		"creatures crossing a detour do not toggle the structural classification of its furniture");
	Check(w.Cell(108,108) == State::Blocked && w.Cell(108,106) == State::Blocked,
		"creatures and furniture both remain in the original physical movement map");
	Put(w,108,108,Object|0xC0);
	w.walkingMask |= 0xC0;
	Check(StructuralWorld(w,{}).Cell(108,108) == State::Blocked, "water is never mistaken for a removable player stamp or furniture");
}
void PhysicalGuard() {
	auto w = Field(); Put(w,101,101,Object);
	Engine engine; (void)engine.Tick(w,1000);
	const auto& rear = engine.Sensors()[3];
	Check(rear.structure.clearDepth >= 20 && rear.probe.body == State::Blocked,
		"automatic geometry sees past an isolated object while physical clearance still detects it nearby");
	Check(!rear.guidance.openingSafe, "a structural opening cannot authorize a tin into furniture blocking the straight passage");
	Put(w,101,101,0); (void)engine.Tick(w,1050);
	Check(Silent(engine.Tick(w,1100)), "removing ignored furniture does not announce a new room opening");
	Put(w,101,101,Door); (void)engine.Tick(w,1150);
	Check(engine.Tick(w,1200)[3] == State::Blocked, "a real door closing still sounds in the rear direction");
	Put(w,101,101,0); (void)engine.Tick(w,1250);
	Check(engine.Tick(w,1300)[3] == State::Open, "a real rear doorway reopening still sounds");
}
void RoomSeams() {
	auto w = Field(); Rectangle(w,118,108,4,2,Object);
	const auto source = w.rooms.front(); w.rooms.clear();
	for (int left : {0,Size/2}) {
		RoomGrid room{left,0,Size/2,Size,std::vector<uint16_t>(Size*Size/2)};
		for (int y=0; y<Size; ++y) for (int x=0; x<Size/2; ++x)
			room.cells[y*(Size/2)+x] = source.cells[y*Size+left+x];
		w.rooms.push_back(std::move(room));
	}
	Check(StructuralWorld(w,{}).Cell(119,108) == State::Open,
		"a known neighboring room completes the same object's shape and detour across a room seam");
	w.rooms.pop_back();
	Check(StructuralWorld(w,{}).Cell(119,108) == State::Blocked,
		"unloading that neighboring room cannot create a fictitious detour at the seam");
}
void Travel() {
	for (uint16_t flags : {Wall,Object,uint16_t(Wall|Object)}) for (int stride : {1,3,4}) {
		auto w = Field(); w.player.x = 70;
		for (int x=50; x<=180; x+=12) {
			Rectangle(w,x,89,3,2,flags); Rectangle(w,x+5,109,2,3,flags);
		}
		Engine engine; (void)engine.Tick(w,1000);
		unsigned events = 0;
		for (int step=1; step<=72/stride; ++step) {
			w.player.x += stride;
			for (auto event : engine.Tick(w,1000+step*stride*50)) events += event != State::Unknown;
		}
		std::cout << "clutter flags=" << flags << " stride=" << stride << " events=" << events << '\n';
		Check(events == 0, "walking/running between bypassable objects produces no opening/closing chatter in any direction");
		for (int step=1; step<=72/stride; ++step) {
			w.player.x -= stride;
			for (auto event : engine.Tick(w,4600+step*stride*50)) events += event != State::Unknown;
		}
		Check(events == 0, "returning along the same cluttered route stays silent, including behind the player");
	}
}
void WindowAndCost() {
	auto w = Field(); Put(w,100,110,Object);
	bool stable = true;
	for (int x=80; x<=120; ++x) {
		w.player.x = x;
		stable &= StructuralWorld(w,{}).Cell(100,110) == State::Open;
	}
	Check(stable, "a nearby obstacle remains consistently ignored as the local classification window follows the player");
	for (int y=65; y<=140; y+=6) for (int x=65; x<=140; x+=6) Put(w,x,y,Object);
	Engine engine; constexpr int Ticks = 200;
	const auto start = std::chrono::steady_clock::now();
	for (int tick=0; tick<Ticks; ++tick) (void)engine.Tick(w,1000+tick*PollMs);
	const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/Ticks;
	std::cout << "dense clutter average tick ms=" << ms << '\n';
	Check(ms < PollMs, "dense clutter processing stays within the radar's existing poll budget");
}
}
int main(int argc, char** argv) {
	const std::string_view test = argc > 1 ? argv[1] : "all";
	if (test == "all" || test == "travel") Travel();
	if (test == "all") {
		IsolatedObjects(); Barriers(); CoverageAndActors(); PhysicalGuard(); RoomSeams(); WindowAndCost();
	}
	Passed();
}
