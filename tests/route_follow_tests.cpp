#include "route_follow.h"
#include "test_support.h"

#include <cmath>

using namespace D2RAccess;
using RouteFollow::Event;
namespace {
RouteLegs::Leg Walk(double dx, double dy) { return {{dx, dy}, std::hypot(dx, dy)}; }
RouteLegs::Leg Door() { return {{}, 0, true}; }

void Turns() {
	RouteFollow::Follower follower;
	follower.Start({0, 0}, {Walk(0, -20), Walk(15, 0)});
	Check(follower.Active(), "a route asked for is followed");
	Check(follower.Tick({0, -5}, 0).event == Event::None, "nothing is said along a leg");
	const auto turn = follower.Tick({0, -18}, 250);
	Check(turn.event == Event::Turn && turn.leg == 1, "the next leg is said just before its turn");
	Check(follower.Tick({1, -20}, 500).event == Event::None, "and only once");
	Check(follower.Tick({14, -20}, 750).event == Event::End && !follower.Active(), "the end of the way ends the following");
	Check(follower.Tick({14, -20}, 1000).event == Event::None, "after which nothing more is said");
}

void Pace() {
	RouteFollow::Follower follower;
	follower.Start({0, 0}, {Walk(0, -20), Walk(15, 0)});
	(void)follower.Tick({0, -15}, 0);
	Check(follower.Tick({0, -17.5}, 1000).event == Event::None, "at a walk the turn is not said a run's stride early");
	const auto turn = follower.Tick({0, -19}, 1600);
	Check(turn.event == Event::Turn && turn.leg == 1, "but half a second before it");
}

void Doors() {
	RouteFollow::Follower follower;
	follower.Start({0, 0}, {Walk(0, -10), Door(), Walk(0, -10)});
	const auto turn = follower.Tick({0, -8}, 0);
	Check(turn.event == Event::Turn && turn.leg == 1, "a door ahead is said at its corner, with the leg past it");
	Check(follower.Tick({0, -12}, 250).event == Event::None, "the leg past the door is not said again");
}

// The tester's grave (2026-10-20): "northwest, 3; then southwest, 4" was cut at once by "Now southwest, 4".
void ShortLegs() {
	RouteFollow::Follower follower;
	follower.Start({0, 0}, {Walk(-3, 0), Walk(0, 4), Walk(1, 1), Walk(0, 10)});
	Check(follower.Tick({0, 0}, 0).event == Event::None, "a short first leg is not skipped at the start");
	Check(follower.Tick({0, 0}, 250).event == Event::None, "nor while the player stands");
	Check(follower.Tick({-1, 0}, 500).event == Event::None, "nor before half of it is walked");
	const auto turn = follower.Tick({-2, 0}, 750);
	Check(turn.event == Event::Turn && turn.leg == 1, "its next leg is said past its middle");
	follower.Start({0, 0}, {Walk(0, -20), Walk(2, 0), Walk(0, -10)});
	Check(follower.Tick({0, -17}, 0).event == Event::Turn, "a short leg is said before its turn");
	Check(follower.Tick({0, -18}, 250).event == Event::None, "and the leg after it is not said over it");
	const auto after = follower.Tick({1, -20}, 750);
	Check(after.event == Event::Turn && after.leg == 2, "but once it is half walked");
}

void CutCorners() {
	RouteFollow::Follower follower;
	follower.Start({0, 0}, {Walk(0, -20), Walk(15, 0), Walk(0, -20)});
	const auto step = follower.Tick({15, -25}, 0);
	Check(step.event == Event::Turn && step.leg == 2, "a turn walked past already moves on to the leg the player is on");
}

void OffTheRoute() {
	RouteFollow::Follower follower;
	follower.Start({0, 0}, {Walk(0, -20), Walk(15, 0)});
	Check(follower.Tick({12, -5}, 0).event == Event::None, "a moment away from the route is a dodge");
	Check(follower.Tick({12, -5}, 1000).event == Event::None, "and so is a second");
	Check(follower.Tick({12, -5}, 2001).event == Event::Off && !follower.Active(), "two seconds away leave the route");
	follower.Start({0, 0}, {Walk(0, -20), Walk(15, 0)});
	(void)follower.Tick({12, -5}, 0);
	Check(follower.Tick({0, -6}, 1000).event == Event::None, "coming back to it");
	Check(follower.Tick({12, -5}, 2500).event == Event::None && follower.Active(), "starts the count again");
	follower.Start({0, 0}, {});
	Check(!follower.Active(), "no legs, nothing to follow");
}
}

int main() {
	Turns();
	Pace();
	Doors();
	ShortLegs();
	CutCorners();
	OffTheRoute();
	Passed();
	return 0;
}
