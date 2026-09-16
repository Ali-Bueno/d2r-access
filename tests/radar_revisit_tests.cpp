#include "radar.h"
#include "recorded_return_grid.h"
#include "test_support.h"
#include <cmath>

using namespace D2RAccess::Radar;
namespace {
constexpr Position Mouth{4784,4582};
constexpr std::array<int,DirectionCount> Turns{0,3,1,2};
Position Rotate(Position p,int turns) {
	double x=p.x-Mouth.x,y=p.y-Mouth.y;
	for (int i=0;i<turns;++i) { const double before=x; x=-y; y=before; }
	return {Mouth.x+x,Mouth.y+y};
}
World Map(size_t direction) {
	auto world=RecordedReturnGrid(); const auto original=world.rooms[0];
	for (int y=0;y<original.height;++y) for (int x=0;x<original.width;++x) {
		const auto p=Rotate({double(original.x+x),double(original.y+y)},Turns[direction]);
		world.rooms[0].cells[static_cast<size_t>(p.y-original.y)*original.width+static_cast<size_t>(p.x-original.x)]
			=original.cells[static_cast<size_t>(y)*original.width+x];
	}
	return world;
}
struct Walk {
	World world;
	Engine engine;
	size_t direction;
	uint64_t now=1000;
	bool legal=true;
	explicit Walk(size_t d):world(Map(d)),direction(d) {}
	State At(Position p,uint64_t elapsed=PollMs,AudioLoad audio={}) {
		world.player=Rotate(p,Turns[direction]);
		legal &= world.Body(int(world.player.x),int(world.player.y))==State::Open;
		return engine.Tick(world,now+=elapsed,audio)[direction];
	}
	void FirstVisit() {
		unsigned openings=0;
		for (auto p:{Position{4781,4583},{4782,4583},{4783,4582},Mouth,Mouth}) openings+=At(p)==State::Open;
		Check(openings==1 && legal,"the captured collision map reproduces the original first opening along body-valid steps");
	}
	unsigned Return(int stride) {
		unsigned openings=0;
		bool outwardSilent=true;
		for (int n=stride;n<=4;n+=stride) outwardSilent &= At({Mouth.x+n,Mouth.y+n},stride*PollMs)!=State::Open;
		// Always leave the full footprint even when the stride does not divide four.
		outwardSilent &= At({Mouth.x+4,Mouth.y+4})!=State::Open;
		Check(outwardSilent,"leaving the remembered turn alone never generates a repeat");
		for (int n=4-stride;n>0;n-=stride) openings+=At({Mouth.x+n,Mouth.y+n},stride*PollMs)==State::Open;
		openings+=At(Mouth,stride*PollMs)==State::Open;
		openings+=At(Mouth)==State::Open;
		return openings;
	}
};
void CapturedShortReturns() {
	for (size_t d=0;d<DirectionCount;++d) for (int stride:{1,2,3,4}) {
		Walk walk(d); walk.FirstVisit();
		for (int trip=0;trip<3;++trip) {
			Check(walk.Return(stride)==1 && walk.legal,"leaving and returning to an announced turn recalls it once without an intervening wall");
			bool silent=true;
			for (int i=0;i<8;++i) silent &= walk.At(Mouth)==State::Unknown;
			Check(silent,"a recalled opening remains silent while standing at the same turn");
		}
	}
}
void JitterAndBaseline() {
	Walk walk(0); walk.FirstVisit();
	bool silent=true;
	for (int i=0;i<20;++i) {
		walk.world.subtileFraction={i%2 ? .9:.1,.5};
		silent &= walk.At({Mouth.x+(i%2),Mouth.y+(i%2)})==State::Unknown;
	}
	Check(silent,"subcell motion and a small wobble inside the same opening cannot rearm it");
	Walk baseline(0); (void)baseline.At(Mouth);
	Check(baseline.Return(1)==0,"an initially open direction that was never announced cannot become a return reminder");
	++walk.world.session; (void)walk.At(Mouth);
	Check(walk.Return(1)==0,"a new session clears remembered opening visits");
}
void ReminderDelivery() {
	Walk walk(0); walk.FirstVisit();
	Check(walk.Return(4)==1,"a fresh return generates the reminder used by delivery checks");
	const auto emitted=walk.engine.Sensors()[0].lastEmit;
	Check(walk.engine.Sensors()[0].revisitCue && walk.engine.Sensors()[0].cueState==State::Open,
		"the consumed return remains valid for queued audio");
	AudioLoad failed; failed.failed[0]={State::Open,emitted,false};
	Check(walk.At(Mouth,PollMs,failed)==State::Unknown,"a failed reminder respects normal retry spacing");
	Check(walk.At(Mouth,200)==State::Open && walk.engine.Sensors()[0].retrying,
		"a still-clear reminder can recover from failure before audio submission");
	Check(walk.At(Mouth)==State::Unknown,"successful reminder retry does not rearm another visit");
	const auto& room=walk.world.rooms[0];
	walk.world.rooms[0].cells[static_cast<size_t>(4581-room.y)*room.width+4783-room.x]=0x1100;
	(void)walk.At(Mouth);
	Check(walk.engine.Sensors()[0].cueState==State::Unknown,"an actor entering the returned passage invalidates queued reminder audio");
}
void TemporaryCoverageLoss() {
	Walk walk(0); walk.FirstVisit();
	(void)walk.At({Mouth.x+4,Mouth.y+4});
	const auto rooms=walk.world.rooms; walk.world.rooms.clear();
	Check(walk.At(Mouth)==State::Unknown && walk.engine.Sensors()[0].cueState==State::Unknown,
		"returning into missing map coverage produces no remembered cue");
	walk.world.rooms=rooms;
	Check(walk.At(Mouth)==State::Unknown,"restoring the map starts fresh physical confirmation");
	Check(walk.At(Mouth)==State::Open,"a previously announced visit survives a temporary observation gap once the real approach clears");
}
void BackWallDoesNotEraseVisit() {
	World world; world.player={40,40}; world.session=world.level=1; world.bodyPattern=1; world.walkingMask=0x1C09;
	constexpr int Size=100, FrontSum=86, BackSum=110;
	world.rooms={{0,0,Size,Size,std::vector<uint16_t>(Size*Size)}};
	const auto wall=[&](int sum,uint16_t flags) {
		for (int y=0;y<Size;++y) if (sum-y>=0 && sum-y<Size) world.rooms[0].cells[y*Size+sum-y]=flags;
	};
	wall(FrontSum,1); wall(BackSum,1);
	Engine engine; uint64_t now=1000; (void)engine.Tick(world,now);
	wall(FrontSum,0); (void)engine.Tick(world,now+=50);
	Check(engine.Tick(world,now+=50)[3]==State::Open,"a finite passage establishes the remembered turn before approaching its back wall");
	for (int n=1;n<=11;++n) { world.player={40.0+n,40.0+n}; (void)engine.Tick(world,now+=50); }
	(void)engine.Tick(world,now+=50);
	unsigned reminders=0;
	for (int n=10;n>=0;--n) {
		world.player={40.0+n,40.0+n}; reminders+=engine.Tick(world,now+=50)[3]==State::Open;
	}
	reminders+=engine.Tick(world,now+=50)[3]==State::Open;
	Check(reminders==1 && engine.Sensors()[3].revisitCue && !engine.Sensors()[3].newOpening,
		"a newly remembered back wall does not erase a still-usable return to the original turn");
}
void RememberedApproach() {
	OpeningVisit visit; Settings settings;
	const Position axis{1,0}, point{100,100};
	Probe clear; clear.available=true; clear.end=State::Blocked; clear.clearDepth=30;
	visit.Remember(point,axis,20);
	visit.Update({104,100},axis,clear,clear,true,1,1000,settings);
	Probe actor=clear; actor.clearDepth=12;
	visit.Update(point,axis,clear,actor,true,1,1050,settings);
	visit.Update(point,axis,clear,actor,true,1,1100,settings);
	Check(!visit.Ready(),"a returning visit cannot ignore an actor blocking its remembered approach beyond eight subtiles");
	visit.Update(point,axis,clear,clear,true,1,1150,settings);
	visit.Update(point,axis,clear,clear,true,1,1200,settings);
	Check(visit.Ready(),"the remembered approach needs two fresh clear observations after occupancy clears");
	visit.Reported();
	visit.Update({101,100},axis,actor,actor,false,1,1250,settings);
	visit.Update(point,axis,clear,clear,true,1,1300,settings);
	visit.Update(point,axis,clear,clear,true,1,1350,settings);
	Check(!visit.Ready(),"a transient obstruction outside the mouth cannot rearm a tiny movement within the visit");
	Probe jamb=clear; jamb.clearDepth=0;
	visit.Update({101,100},axis,jamb,jamb,false,1,1400,settings);
	visit.Update(point,axis,clear,clear,true,1,1450,settings);
	visit.Update(point,axis,clear,clear,true,1,1500,settings);
	Check(visit.Ready(),"a body-valid moved cell losing a narrow structural mouth can rearm a short return");
	visit.Reported();
	visit.Update(point,axis,jamb,jamb,false,1,1550,settings);
	visit.Update(point,axis,clear,clear,true,1,1600,settings);
	visit.Update(point,axis,clear,clear,true,1,1650,settings);
	Check(!visit.Ready(),"stationary wall flicker never counts as leaving the opening");
	visit.Update({104,100},axis,clear,clear,true,1,1700,settings);
	Probe unknown=clear; unknown.end=State::Unknown; unknown.clearDepth=10;
	visit.Update(point,axis,unknown,unknown,false,1,1750,settings);
	visit.Update(point,axis,unknown,unknown,false,1,1800,settings);
	Check(!visit.Valid() && !visit.Ready(),"remembered geometry cannot replace missing coverage on a return");
}
}
int main() { CapturedShortReturns(); JitterAndBaseline(); ReminderDelivery(); TemporaryCoverageLoss(); BackWallDoesNotEraseVisit(); RememberedApproach(); Passed(); }
