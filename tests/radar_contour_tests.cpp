#include "radar.h"
#include "test_support.h"
#include <cmath>

using namespace D2RAccess::Radar;
namespace {
constexpr int Size=240, Centre=Size/2;
Position Rotate(Position point, size_t direction) {
	const double x=point.x-Centre, y=point.y-Centre;
	switch (direction) {
	case 0: return {Centre-x,Centre-y};
	case 1: return {Centre-y,Centre+x};
	case 2: return {Centre+y,Centre-x};
	default: return point;
	}
}
World Field() {
	World world; world.session=world.level=1; world.bodyPattern=1; world.walkingMask=0x1C09;
	world.rooms={{0,0,Size,Size,std::vector<uint16_t>(Size*Size)}};
	return world;
}
void Put(World& world,int x,int y,size_t direction) {
	const auto p=Rotate({double(x),double(y)},direction);
	if (p.x>=0 && p.y>=0 && p.x<Size && p.y<Size)
		world.rooms[0].cells[static_cast<size_t>(p.y)*Size+static_cast<size_t>(p.x)]=1;
}
int Edge(int x,bool curved) {
	// Static solid walls: a two-subtile raster jog, or a gently curved face.
	// Neither contains a doorway, wall end or separate backing wall.
	return 114+(curved ? int(std::floor(.007*(x-80)*(x-80))) : (x>=100 ? 2 : 0));
}
void ContinuousFace(size_t direction,int stride,bool curved) {
	auto world=Field();
	for (int y=0;y<Size;++y) for (int x=0;x<Size;++x) if (y>=Edge(x,curved)) Put(world,x,y,direction);
	world.player=Rotate({80,double(Edge(80,curved)-4)},direction);
	Engine engine; uint64_t now=1000; (void)engine.Tick(world,now);
	for (int i=1;i<=12;++i) {
		world.player=Rotate({80,double(Edge(80,curved)-4-i)},direction);
		(void)engine.Tick(world,now+=50);
	}
	unsigned openings=0;
	// Keep the curved face inside the loaded structural window. Where it
	// turns out of the window, local contour identity cannot be established.
	const int end=curved ? 100 : 145;
	for (int x=80;x<end;x+=stride) {
		world.player=Rotate({double(x),double(Edge(x,curved)-16)},direction);
		openings+=engine.Tick(world,now+=stride*50)[direction]==State::Open;
	}
	Check(openings==0,"retreat and travel along an observed continuous raster/curved face cannot invent an exit");
}
void HeldDirectionSlide(size_t direction) {
	auto world=Field();
	for (int y=0;y<Size;++y) for (int x=0;x<Size;++x) if (y>=Edge(x,false)) Put(world,x,y,direction);
	Position player{80,110}; world.player=Rotate(player,direction);
	Engine engine; uint64_t now=1000; (void)engine.Tick(world,now);
	unsigned openings=0,slides=0;
	for (int i=0;i<60;++i) {
		const auto wanted=Rotate({player.x+1,player.y+1},direction);
		// A fixture for continued stick intent, not a claim to emulate D2R's
		// controller solver: actual motion follows legal body destinations.
		if (world.Body(int(wanted.x),int(wanted.y))==State::Open) player.y+=1;
		else ++slides;
		player.x+=1; world.player=Rotate(player,direction);
		Check(world.Body(int(world.player.x),int(world.player.y))==State::Open,"sliding fixture never crosses the wall");
		openings+=engine.Tick(world,now+=50)[direction]==State::Open;
	}
	Check(slides>0 && openings==0,"holding toward a continuous wall while sliding cannot report a usable forward exit");
}
void ConnectedDoorway() {
	auto world=Field();
	// The doorway jambs join a finite back wall. A contour match must not
	// wander through that connected room boundary and hide the actual door.
	for (int y=114;y<Size;++y) for (int x=0;x<Size;++x)
		if (x<100 || x>118 || y>=135) Put(world,x,y,3);
	world.player={85,110}; Engine engine; uint64_t now=1000; (void)engine.Tick(world,now);
	unsigned openings=0;
	for (int x=86;x<110;++x) {
		world.player.x=x; openings+=engine.Tick(world,now+=50)[3]==State::Open;
	}
	Check(openings==1,"a real doorway remains audible even when its jamb and back wall are connected");
}
}
int main() {
	for (size_t direction=0;direction<DirectionCount;++direction) {
		for (int stride:{1,2,3,4}) for (bool curved:{false,true}) ContinuousFace(direction,stride,curved);
		HeldDirectionSlide(direction);
	}
	ConnectedDoorway(); Passed();
}
