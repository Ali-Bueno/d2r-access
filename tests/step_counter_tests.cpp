#include "step_counter.h"
#include "test_support.h"

using namespace D2RAccess;

int main() {
	StepCounter::Odometer odometer;
	Check(odometer.Walk({100, 100}, 1, 1) == 0, "the first read only sets where the walk starts");
	Check(odometer.Walk({100.6, 100}, 1, 1) == 0, "under a unit walked, no click yet");
	Check(odometer.Walk({101.2, 100}, 1, 1) == 1, "a unit walked is one click");
	Check(odometer.Walk({104.2, 100}, 1, 1) == 3, "three units in one read owe three clicks");
	Check(odometer.Walk({104.2, 104}, 1, 5) == 0 && odometer.Walk({104.2, 105.1}, 1, 5) == 1,
		"every five units, the rest carried over");
	Check(odometer.Walk({160, 160}, 1, 1) == 0, "a teleport or a waypoint is no walk");
	Check(odometer.Walk({161.5, 160}, 1, 1) == 1, "walking on from where the jump landed counts");
	Check(odometer.Walk({162.5, 160}, 2, 1) == 0, "a new game starts the count afresh");
	Check(odometer.Walk({163.5, 160}, 2, 0) == 0 && odometer.Walk({164.5, 160}, 2, 1) == 0,
		"no interval counts nothing and starts again");
	Passed();
}
