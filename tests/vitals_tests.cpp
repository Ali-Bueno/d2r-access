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

void SimultaneousAlerts() {
	Vitals::Alerts alerts;
	Check(!alerts.Update(100, 100).Any(), "first HUD sample sets both baselines");
	auto alert = alerts.Update(9, 9);
	Check(alert.life == 9 && alert.mana == 9 && alert.interrupt,
		"simultaneous critical losses form one interrupting announcement with life first");
	alert = alerts.Update(9, 10);
	Check(alert.life < 0 && alert.mana == 10 && !alert.interrupt,
		"recovering critical mana does not interrupt a preceding health warning");
	alert = alerts.Update(4, 20);
	Check(alert.life == 4 && alert.mana == 20 && alert.interrupt,
		"critical life loss keeps priority when mana recovers on the same sample");
	alert = alerts.Update(-1, 20);
	Check(!alert.Any(), "a transiently unreadable orb does not change its baseline");
	alert = alerts.Update(3, 9);
	Check(alert.life == 3 && alert.mana == 9 && alert.interrupt,
		"readings resume against the last known baseline");
	alerts.Reset();
	Check(!alerts.Update(2, 2).Any(), "a new game takes fresh critical baselines");
}

void MissingOrbs() {
	Vitals::OrbAvailability availability;
	for (int sample = 1; sample < Vitals::MissingConfirmationSamples; ++sample)
		Check(availability.Update(false, true) == 0,
			"a brief missing life display does not claim the gameplay option is off");
	Check(availability.Update(true, true) == 0, "a good life display clears the transient streak");
	for (int sample = 1; sample < Vitals::MissingConfirmationSamples; ++sample)
		availability.Update(true, false);
	Check(availability.Update(true, false) == 2, "a persistently missing mana display is identified alone");
	for (int sample = 1; sample < Vitals::MissingConfirmationSamples; ++sample)
		availability.Update(false, false);
	Check(availability.Update(false, false) == 3, "both persistently missing displays are identified together");
	Check(availability.Update(true, false) == 2, "a recovered life display is no longer marked missing");
	availability.Reset();
	Check(availability.Update(false, false) == 0, "session reset clears missing-orb confirmation");
}

void Conditions() {
	Vitals::Condition condition;
	Check(!condition.Update(false), "healthy first sample is only a baseline");
	Check(!condition.Update(false), "unchanged condition stays silent");
	Check(condition.Update(true) == true, "entering poison reports once");
	Check(!condition.Update(std::nullopt), "unreadable player state cannot clear poison");
	Check(!condition.Update(true), "a restored poison read does not repeat");
	Check(condition.Update(false) == false, "leaving poison reports once");
	condition.Reset();
	Check(!condition.Update(true), "new game takes its own silent condition baseline");
}

void MercenaryCondition() {
	Vitals::MercenaryMeter meter;
	auto alert = meter.Update(11U, 100, 10);
	Check(alert.life < 0 && alert.level < 0, "first observed mercenary takes life and level baselines");
	alert = meter.Update(11U, 74, 10);
	Check(alert.life == 74 && alert.level < 0 && !alert.interrupt,
		"mercenary life shares player steps and queues above the critical threshold");
	alert = meter.Update(11U, 9, 11);
	Check(alert.life == 9 && alert.level == 11 && alert.interrupt,
		"critical mercenary damage and level gain are one interrupting change");
	alert = meter.Update(std::nullopt, -1, -1);
	Check(alert.life < 0 && alert.level < 0, "an unloaded mercenary is unknown, not dead");
	alert = meter.Update(11U, 9, 11);
	Check(alert.life < 0 && alert.level < 0, "returning unit retains its previous steps and level");
	alert = meter.Update(11U, 10, 12);
	Check(alert.life == 10 && alert.level == 12 && !alert.interrupt,
		"recovery and a level gain queue behind critical downward alerts");
	alert = meter.Update(11U, -1, 13);
	Check(alert.life < 0 && alert.level == 13, "an unavailable life stat does not suppress a verified level gain");
	alert = meter.Update(11U, -1, 12);
	Check(alert.level < 0, "a stale lower level does not cause a false announcement");
	alert = meter.Update(11U, -1, 13);
	Check(alert.level < 0, "returning to the last confirmed level remains silent");
	alert = meter.Update(12U, 4, 9);
	Check(alert.life < 0 && alert.level < 0, "a new hireling starts from its own baselines");
	alert = meter.Update(12U, 0, 9);
	Check(alert.life == 0 && alert.interrupt, "a confirmed death mode reports zero life urgently");
	meter.Reset();
	alert = meter.Update(12U, 100, 12);
	Check(alert.life < 0 && alert.level < 0, "session reset forgets the previous hireling and level");
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

void Equipping() {
	Vitals::Equipping equipping;
	const std::vector<Vitals::Breakage::Item> worn{{1, 28, 28, 1}, {2, 20, 40, 4}};
	Check(equipping.Sweep(worn).empty(), "entering the game says nothing about what is already worn");
	Check(equipping.Sweep(worn).empty(), "unchanged equipment says nothing");
	auto helmet = worn;
	helmet[0] = {7, 12, 12, 1};
	const auto equipped = equipping.Sweep(helmet);
	Check(equipped.size() == 1 && equipped[0].id == 7 && equipped[0].bodyLocation == 1,
		"a new item in a slot is reported once, with its slot");
	Check(equipping.Sweep(helmet).empty(), "and only once");
	auto swapped = helmet;
	swapped[1].bodyLocation = 11;
	Check(equipping.Sweep(swapped).empty(), "a weapon swap moves worn items and equips nothing");
	Check(equipping.Sweep({helmet[0]}).empty(), "taking an item off is not equipping");
	Check(equipping.Sweep(helmet).size() == 1, "putting it back on is");
	equipping.Reset();
	Check(equipping.Sweep(worn).empty(), "a new game starts from a fresh baseline");
}
}

void Gaits() {
	using Vitals::Gait;
	Vitals::GaitTracker gait;
	Check(!gait.Toggle(), "a toggle before any step has nothing to flip");
	Check(!gait.Observe(Gait::Walk), "the first step is a baseline");
	Check(!gait.Observe(std::nullopt), "standing still says nothing");
	Check(!gait.Observe(Gait::Run) && !gait.Observe(Gait::Run), "a new gait waits until it has held");
	Check(gait.Observe(Gait::Run) == Gait::Run, "starting to run is announced once it holds");
	Check(!gait.Observe(Gait::Run), "and only once");
	bool flicker = false;
	for (int i = 0; i < 10; ++i) flicker |= gait.Observe(i % 2 ? Gait::Run : Gait::Walk).has_value();
	Check(!flicker, "a character flickering between walk and run at stamina's edge says nothing");
	Check(gait.Toggle() == Gait::Walk, "the game's toggle predicts walking at once");
	Check(!gait.Observe(Gait::Walk), "the step that confirms it says nothing more");
	Check(gait.Toggle() == Gait::Run, "toggling again predicts running");
	for (int i = 1; i < Vitals::GaitConfirmSamples; ++i) gait.Observe(Gait::Walk);
	Check(gait.Observe(Gait::Walk) == Gait::Walk, "steps that contradict the prediction correct it");
	gait.Reset();
	Check(!gait.Observe(Gait::Run), "a new game starts from a fresh baseline");
}

void Stamina() {
	Vitals::StaminaMeter stamina;
	Check(stamina.Update(100) < 0, "the first reading is a baseline");
	Check(stamina.Update(60) < 0, "above the first step stays silent");
	Check(stamina.Update(49) == 49, "falling through 50 speaks the real percentage");
	Check(stamina.Update(45) < 0, "within the same step stays silent");
	Check(stamina.Update(55) < 0 && stamina.Update(49) < 0, "a small recovery does not re-arm the step");
	Check(stamina.Update(61) < 0 && stamina.Update(48) == 48, "recovering past the margin re-arms it");
	Check(stamina.Update(24) == 24 && stamina.Update(9) == 9, "each lower step is announced");
	Check(stamina.Update(0) == 0, "running out is announced as zero");
	Check(stamina.Update(-1) < 0 && stamina.Update(0) < 0, "an unreadable sample keeps the state");
	stamina.Reset();
	Check(stamina.Update(5) < 0, "a new game starts from a fresh baseline");
}

void Ammo() {
	Vitals::AmmoMeter ammo;
	using Stack = Vitals::AmmoMeter::Stack;
	Check(!ammo.Update({{1, 1}, {2, 120}}), "the first sight of a stack is a baseline");
	Check(!ammo.Update({{1, 1}, {2, 51}}), "above the first step stays silent");
	const auto fifty = ammo.Update({{1, 1}, {2, 50}});
	Check(fifty && fifty->id == 2 && fifty->quantity == 50, "falling to 50 names the stack and its count");
	Check(!ammo.Update({{1, 1}, {2, 30}}), "within a step stays silent");
	Check(ammo.Update({{1, 1}, {2, 5}})->quantity == 5, "a big drop speaks once, at the count");
	Check(ammo.Update({{1, 1}, {2, 0}})->quantity == 0, "running out is announced");
	Check(!ammo.Update({{1, 1}, {2, 200}}), "a refill is silent");
	Check(ammo.Update({{1, 1}, {2, 20}})->quantity == 20, "and re-arms the steps");
	Check(ammo.Known({{1, 1}, {2, 20}}).size() == 1, "a single sword is not a stack");
	Check(ammo.Known({{2, 1}}).size() == 1, "a stack down to one is still a stack");
	Check(!ammo.Update({{3, 4}}), "a stack equipped low is a baseline, not an alarm");
	ammo.Reset();
	Check(ammo.Known({{2, 1}}).empty(), "a new game forgets the stacks");
}

int main() {
	Steps(); Percentages(); OrbText(); Announcements(); SimultaneousAlerts(); MissingOrbs(); Conditions(); MercenaryCondition(); Breaking(); Equipping();
	Gaits(); Stamina(); Ammo();
	Passed();
}
