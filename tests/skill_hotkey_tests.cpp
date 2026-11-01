#include "test_support.h"
#include "skill_hotkey.h"

using D2RAccess::SkillHotkey::Tracker;

int main() {
	Tracker tracker;
	Check(tracker.Changed(36), "the first skill used is announced");
	Check(!tracker.Changed(36), "the same skill again stays silent");
	Check(!tracker.Changed(36), "and again, as quick cast repeats it");
	Check(tracker.Changed(54), "another skill is announced");
	Check(tracker.Changed(36), "switching back announces it again");
	Check(!tracker.Changed(-1), "no skill says nothing");
	Check(!tracker.Changed(36), "and does not forget the last one");
	tracker.Reset();
	Check(tracker.Changed(36), "a new game announces the same skill again");
	Passed();
}
