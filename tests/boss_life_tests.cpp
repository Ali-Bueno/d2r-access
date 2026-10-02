#include "boss_life.h"
#include "test_support.h"

using namespace D2RAccess::BossLife;
namespace {
std::vector<Reading> Andariel(int percent) { return {{7, "Andariel", percent}}; }

void Steps() {
	Tracker tracker;
	Check(tracker.Update(Andariel(100)).empty(), "the first sight of a boss is only a baseline");
	Check(tracker.Update(Andariel(80)).empty(), "chip damage above the first step stays silent");
	auto alerts = tracker.Update(Andariel(74));
	Check(alerts.size() == 1 && alerts[0].name == "Andariel" && alerts[0].percent == 74 && !alerts[0].interrupt,
		"crossing a step speaks the real percentage, queued");
	Check(tracker.Update(Andariel(60)).empty(), "within a step nothing more is said");
	Check(tracker.Update(Andariel(49)).size() == 1 && tracker.Update(Andariel(39)).size() == 1,
		"the finer steps below half are said too");
	alerts = tracker.Update(Andariel(9));
	Check(alerts.size() == 1 && alerts[0].interrupt, "a critical fall interrupts, like the player's own life");
	alerts = tracker.Update(Andariel(0));
	Check(alerts.size() == 1 && alerts[0].percent == 0 && alerts[0].interrupt, "death is said as zero");
	Check(tracker.Update(Andariel(0)).empty(), "and only once");
	Tracker healing;
	(void)healing.Update(Andariel(30));
	alerts = healing.Update(Andariel(52));
	Check(alerts.size() == 1 && alerts[0].percent == 52 && !alerts[0].interrupt, "a boss that heals across steps is said, queued");
}

void Several() {
	Tracker tracker;
	(void)tracker.Update({{1, "Rakanishu", 100}, {2, "Bishibosh", 100}});
	const auto alerts = tracker.Update({{1, "Rakanishu", 70}, {2, "Bishibosh", 45}});
	Check(alerts.size() == 2 && alerts[0].name == "Rakanishu" && alerts[1].name == "Bishibosh",
		"two bosses are followed apart");
	(void)tracker.Update({{2, "Bishibosh", 45}});
	Check(tracker.Followed().size() == 1 && tracker.Followed()[0].id == 2, "a boss missing from the readings is forgotten");
	Check(tracker.Update({{1, "Rakanishu", 40}}).empty(), "a forgotten boss seen again starts a fresh baseline");
	Check(tracker.Followed()[0].percent == 40, "the last percentage is kept with the name");
	Check(tracker.Update({{1, "Rakanishu", -1}}).empty() && tracker.Followed().size() == 1,
		"an unreadable life keeps the baseline without speaking");
	tracker.Reset();
	Check(tracker.Followed().empty(), "a reset forgets every boss");
}
}

int main() {
	Steps(); Several();
	Passed();
}
