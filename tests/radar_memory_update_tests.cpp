#include "radar.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>

using namespace D2RAccess::Radar;
namespace {
constexpr int Size=240, Centre=120;
constexpr std::array<Position,4> Steps{{{-1,-1},{-1,1},{1,-1},{1,1}}};
World Field() {
	World world; world.player={Centre,Centre}; world.session=world.level=1;
	world.bodyPattern=1; world.walkingMask=0x1C09;
	world.rooms={{0,0,Size,Size,std::vector<uint16_t>(Size*Size)}};
	return world;
}
void Put(World& world,int x,int y,uint16_t flags) { world.rooms[0].cells[y*Size+x]=flags; }
void NearestWallFirstPass(size_t direction,int stride) {
	auto world=Field(); const auto step=Steps[direction];
	// A near outer wall is remembered first. After retreat, a nearer inner
	// wall enters the ray, but remains outside the five-subtile warning.
	// Its doorway ends at a third wall BEFORE the old remembered plane.
	for (int y=0;y<Size;++y) for (int x=0;x<Size;++x) {
		const double forward=(x-Centre)*step.x+(y-Centre)*step.y;
		const double side=-(x-Centre)*step.y+(y-Centre)*step.x;
		if (forward==6 || (side>=12 && (forward==4 || (forward==-8 && (side<26 || side>42))))) Put(world,x,y,1);
	}
	Engine engine; uint64_t now=1000; (void)engine.Tick(world,now);
	for (int i=0;i<12;++i) { world.player.x-=step.x; world.player.y-=step.y; (void)engine.Tick(world,now+=50); }
	unsigned outward=0,returning=0;
	// Finish the return before the inner wall's separate end at side=12.
	for (int sign : {1,-1}) for (int i=0;i<(sign>0 ? 27 : 18);i+=stride) {
		world.player.x-=sign*stride*step.y; world.player.y+=sign*stride*step.x;
		const auto event=engine.Tick(world,now+=stride*50)[direction];
		(sign>0 ? outward : returning)+=event==State::Open;
		if (event==State::Open) Check(engine.Sensors()[direction].guidance.openingSafe,
			"the refreshed opening still has full-body direct clearance");
	}
	if (outward!=1 || returning!=1) std::cerr << "nearest direction=" << direction << " outward=" << outward << " returning=" << returning << '\n';
	Check(outward==1 && returning==1,
		"a nearer wall replaces the old plane and its doorway is detected on both passes without approaching it first");
}
void RevisitAfterRetreat(size_t direction,int stride) {
	auto world=Field(); const auto step=Steps[direction];
	for (int y=0;y<Size;++y) for (int x=0;x<Size;++x) {
		const double forward=(x-Centre)*step.x+(y-Centre)*step.y;
		const double side=-(x-Centre)*step.y+(y-Centre)*step.x;
		if (forward==6 && (side<14 || side>30)) Put(world,x,y,1);
	}
	Engine engine; uint64_t now=1000; (void)engine.Tick(world,now);
	for (int i=0;i<8;++i) { world.player.x-=step.x; world.player.y-=step.y; (void)engine.Tick(world,now+=50); }
	unsigned outward=0,returning=0;
	for (int sign : {1,-1}) for (int i=0;i<22;i+=stride) {
		world.player.x-=sign*stride*step.y; world.player.y+=sign*stride*step.x;
		const auto event=engine.Tick(world,now+=stride*50)[direction];
		(sign>0 ? outward : returning)+=event==State::Open;
	}
	if (outward!=1 || returning!=1) std::cerr << "revisit direction=" << direction << " outward=" << outward << " returning=" << returning << '\n';
	Check(outward==1 && returning==1,
		"a doorway rearms when its wall returns even if the player never comes within the near-wall warning distance");
}
void CloseBackWall() {
	const double diagonal=std::sqrt(0.5); WallMemory memory; Settings settings;
	Probe wall; wall.end=State::Blocked; wall.hit={126,126}; wall.clearDepth=7.5;
	wall.surfaceNormal={diagonal,diagonal};
	(void)memory.Update(wall,{120,120},{diagonal,diagonal},true,1000,settings);
	// The input is already a full-body scan. One native diagonal step past
	// the remembered body boundary must not require another body inflation.
	wall.hit={128,128}; wall.clearDepth=10.5;
	(void)memory.Update(wall,{120,120},{diagonal,diagonal},false,1050,settings);
	Check(memory.Update(wall,{120,120},{diagonal,diagonal},false,1100,settings),
		"a physically clear opening before a close back wall is not hidden by inflating the body twice");
}
void ConfirmedReturnOnly() {
	auto world=Field();
	const auto door=[&](bool open) {
		for (int y=0;y<Size;++y) for (int x=0;x<Size;++x)
			if (x+y==2*Centre+6) Put(world,x,y,open && std::abs(x-y)<=6 ? 0 : 1);
	};
	door(false); Engine engine; uint64_t now=1000; (void)engine.Tick(world,now);
	for (int i=0;i<8;++i) { --world.player.x; --world.player.y; (void)engine.Tick(world,now+=50); }
	door(true); (void)engine.Tick(world,now+=50);
	Check(engine.Tick(world,now+=50)[3]==State::Open,"a real doorway announces after retreat");
	door(false); (void)engine.Tick(world,now+=50);
	Check(!engine.Sensors()[3].wallMemory.BarrierConfirmed(),"a single wall observation cannot rearm the opening");
	door(true);
	for (int i=0;i<5;++i) Check(engine.Tick(world,now+=50)[3]==State::Unknown,
		"one-poll wall flicker cannot repeat the already announced doorway");
	door(false); (void)engine.Tick(world,now+=50);
	Check(engine.Tick(world,now+=50)[3]==State::Unknown && engine.Sensors()[3].wallMemory.BarrierConfirmed(),
		"a confirmed returning wall rearms silently outside the proximity-warning range");
	door(true); (void)engine.Tick(world,now+=50);
	Check(engine.Tick(world,now+=50)[3]==State::Open,"a second real doorway encounter sounds without requiring a near-wall closure");
	PassageTracker tracker; const Passage clear{8,5,State::Open,true};
	(void)tracker.Observe(clear,true,1000,50); tracker.RearmOpening();
	Check(tracker.Observe(clear,true,1050,50)==State::Unknown,
		"an unannounced open baseline cannot be rearmed into far-wall chatter");
}
}
int main(int argc,char** argv) {
	const std::string_view test=argc>1 ? argv[1] : "all";
	if (test=="all" || test=="nearest") for (int stride : {1,2,3,4}) for (size_t d=0;d<4;++d) NearestWallFirstPass(d,stride);
	if (test=="all" || test=="revisit") for (int stride : {1,2,3,4}) for (size_t d=0;d<4;++d) RevisitAfterRetreat(d,stride);
	if (test=="all" || test=="backwall") CloseBackWall();
	if (test=="all" || test=="confirmation") ConfirmedReturnOnly();
	Passed();
}
