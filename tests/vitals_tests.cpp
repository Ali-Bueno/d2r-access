#include "test_support.h"
#include "vitals.h"

#include <vector>

using namespace D2RAccess;

namespace {
void Steps() {
	Check(Vitals::Step(100) == 75 && Vitals::Step(99) == 75 && Vitals::Step(75) == 75,
		"a resource above the first step shares one band, so chip damage stays silent");
	Check(Vitals::Step(74) == 50 && Vitals::Step(50) == 50, "the next step is only reached below three quarters");
	Check(Vitals::Step(49) == 40 && Vitals::Step(39) == 30 && Vitals::Step(29) == 20 && Vitals::Step(19) == 10,
		"below half the steps are ten points apart");
	Check(Vitals::Step(9) == 5 && Vitals::Step(4) == 4 && Vitals::Step(3) == 3 && Vitals::Step(2) == 2
		&& Vitals::Step(1) == 1, "the last stretch is announced point by point");
	Check(Vitals::Step(0) == 0 && Vitals::Step(-5) == 0, "an empty resource is its own step");
	Check(Vitals::CriticalPercent == 10, "the critical band that interrupts speech starts where single points do not yet");
}

void Percentages() {
	Check(Vitals::Percent(120, 120) == 100 && Vitals::Percent(60, 120) == 50, "whole percentages come from the two orb numbers");
	Check(Vitals::Percent(119, 120) == 99, "a percentage is floored, never rounded up into a safer number");
	Check(Vitals::Percent(0, 120) == 0 && Vitals::Percent(-3, 120) == 0, "a drained resource reads as zero");
	Check(Vitals::Percent(50, 0) == -1 && Vitals::Percent(50, -1) == -1, "an unusable maximum yields no percentage");
	Check(Vitals::Percent(2000000000, 1) == 100, "an implausible reading cannot exceed a full resource");
}

void OrbText() {
	const auto plain = Vitals::ParseOrb("45/120");
	Check(plain && plain->current == 45 && plain->maximum == 120, "the orb's value display is read as current over maximum");
	const auto grouped = Vitals::ParseOrb("1,234/2,000");
	Check(grouped && grouped->current == 1234 && grouped->maximum == 2000, "digit grouping is not part of the value");
	const auto spaced = Vitals::ParseOrb(" 87 / 300 ");
	Check(spaced && spaced->current == 87 && spaced->maximum == 300, "surrounding spacing is ignored");
	Check(!Vitals::ParseOrb("") && !Vitals::ParseOrb("120") && !Vitals::ParseOrb("/120") && !Vitals::ParseOrb("45/"),
		"text without both numbers is not an orb reading, so the orb text options are known to be off");
}

void Announcements() {
	Vitals::Meter meter;
	Check(meter.Update(100) == -1, "the first reading only takes a baseline");
	Check(meter.Update(88) == -1 && meter.Update(76) == -1, "staying inside a step says nothing");
	Check(meter.Update(74) == 74, "crossing a step announces the real percentage, not the step");
	Check(meter.Update(60) == -1 && meter.Update(51) == -1, "the same step is never repeated");
	Check(meter.Update(49) == 49 && meter.Update(30) == 30 && meter.Update(10) == 10, "each further step announces once");
	Check(meter.Update(4) == 4 && meter.Update(3) == 3 && meter.Update(1) == 1, "the last points are announced one by one");
	Check(meter.Update(0) == 0, "an emptied resource is announced");
	Check(meter.Update(100) == 100, "recovering across steps is announced too");
	meter.Reset();
	Check(meter.Update(20) == -1, "a new character starts from its own baseline");
}

void Breaking() {
	Vitals::Breakage breakage;
	const std::vector<Vitals::Breakage::Item> worn{{1, 28, 28, 1}, {2, 1, 40, 4}, {3, 0, 0, 3}};
	Check(breakage.Sweep(worn).empty(), "the first sweep only learns what is worn");
	Check(breakage.Sweep(worn).empty(), "unchanged durability says nothing");
	std::vector<Vitals::Breakage::Item> damaged = worn;
	damaged[1].durability = 0;
	const auto broke = breakage.Sweep(damaged);
	Check(broke.size() == 1 && broke[0] == 4, "an item reaching zero durability reports its body location");
	Check(breakage.Sweep(damaged).empty(), "an item that stays broken is announced once");
	damaged[1].durability = 40;
	Check(breakage.Sweep(damaged).empty(), "repairing is not a break");
	damaged[1].durability = 0;
	Check(breakage.Sweep(damaged).size() == 1, "breaking again after a repair is announced again");
	const std::vector<Vitals::Breakage::Item> indestructible{{3, 0, 0, 3}};
	Check(breakage.Sweep(indestructible).empty(), "an item without a maximum durability never breaks");
	const std::vector<Vitals::Breakage::Item> replaced{{9, 0, 40, 4}};
	Check(breakage.Sweep(replaced).empty(), "equipping an already broken item is not a break");
	breakage.Reset();
	Check(breakage.Sweep(replaced).empty(), "leaving the game forgets what was worn");
}
}

int main() {
	Steps(); Percentages(); OrbText(); Announcements(); Breaking();
	Passed();
}
