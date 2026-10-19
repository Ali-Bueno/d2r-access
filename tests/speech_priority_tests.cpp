#include "speech_priority.h"
#include "test_support.h"

using namespace D2RAccess::SpeechPriority;

int main() {
	// Each level, for news the mod would and would not cut in with.
	for (const bool urgent : {false, true}) {
		const auto queued = For(Level::Queued, urgent);
		Check(!queued.interrupt && queued.holdMs == 0, "waiting its turn never cuts in");
		const auto mod = For(Level::WhenUrgent, urgent);
		Check(mod.interrupt == urgent && mod.holdMs == 0, "the mod's own choice cuts in only when it is urgent");
		const auto cuts = For(Level::CutsIn, urgent);
		Check(cuts.interrupt && cuts.holdMs == 0, "cutting in always cuts in, and can itself be cut off");
		const auto holds = For(Level::Holds, urgent);
		Check(holds.interrupt && holds.holdMs == HoldMs, "holding cuts in and keeps what follows waiting");
	}

	// The defaults keep what each kind did before the list, but for status endings.
	Check(Defaults[static_cast<size_t>(Kind::StatusBegins)] == Level::Holds, "a status effect's beginning still holds");
	Check(Defaults[static_cast<size_t>(Kind::StatusEnds)] == Level::CutsIn, "its end now cuts in (a tester, 2026-10-19)");
	for (const auto kind : {Kind::LifeAlerts, Kind::BossLife, Kind::EnemyNames, Kind::GameNotices, Kind::QuestAlerts,
			Kind::Places, Kind::NearbyItems})
		Check(Defaults[static_cast<size_t>(kind)] == Level::WhenUrgent, "every other kind keeps the mod's own choice");
	Passed();
	return 0;
}
