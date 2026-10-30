#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// How far each kind of news may push past other speech, chosen by the player in
// the settings list (a tester and the user, 2026-10-19). docs/speech-priority.md
namespace D2RAccess::SpeechPriority {

enum class Kind { StatusBegins, StatusEnds, LifeAlerts, BossLife, EnemyNames, GameNotices, QuestAlerts, Places, NearbyItems, Tutorial, Arrivals, Count };
inline constexpr size_t KindCount = static_cast<size_t>(Kind::Count);

// Waits its turn; cuts in only when the mod judges it urgent; always cuts in; cuts in and is not cut off.
enum class Level { Queued, WhenUrgent, CutsIn, Holds, Count };
inline constexpr int LevelCount = static_cast<int>(Level::Count);

// What each kind did before the list existed, except status endings: a tester asked them to cut in (2026-10-19);
// and arrivals, which a door's name under the cursor cut off (a tester, 2026-10-20).
inline constexpr std::array<Level, KindCount> Defaults{
	Level::Holds, Level::CutsIn, Level::WhenUrgent, Level::WhenUrgent, Level::WhenUrgent,
	Level::WhenUrgent, Level::WhenUrgent, Level::WhenUrgent, Level::WhenUrgent, Level::Queued, Level::Holds};

// How long a held phrase keeps later interruptions waiting (a tester, 2026-10-15: a status
// effect's beginning, "with increased priority over other notifications").
inline constexpr uint64_t HoldMs = 1500;

// About a screen reader's default pace, which the tutorial's words are timed by (docs/tutorial.md).
inline constexpr uint64_t MsPerCharacter = 60;
constexpr uint64_t SpokenMs(std::string_view text) { return text.size() * MsPerCharacter; }

struct Delivery { bool interrupt = false; uint64_t holdMs = 0; };

// A pause in speech that news has waited out, and how long a message waits for one at most: in
// a fight speech may never pause (the tutorial's, MEASURED 2026-10-21; docs/tutorial.md).
inline constexpr uint64_t QuietMs = 1500, PatienceMs = 10000;

// Holds a message until other speech has paused `quietMs`, but `patienceMs` at most: then it
// goes in line behind what is said.
struct Pause {
	uint64_t quietMs = QuietMs, patienceMs = PatienceMs, heldSince = 0;
	// True when the message may go now; `waited` gets how long it was held when patience ran out.
	bool Due(uint64_t now, uint64_t lastSpoken, uint64_t* waited = nullptr) {
		if (now - lastSpoken >= quietMs) { heldSince = 0; return true; }
		if (!heldSince) heldSince = now;
		if (now - heldSince < patienceMs) return false;
		if (waited) *waited = now - heldSince;
		heldSince = 0;
		return true;
	}
};

// What `level` makes of a phrase the mod would cut in with when `urgent`.
constexpr Delivery For(Level level, bool urgent) {
	switch (level) {
	case Level::Queued: return {};
	case Level::WhenUrgent: return {urgent, 0};
	case Level::CutsIn: return {true, 0};
	case Level::Holds: return {true, HoldMs};
	default: return {urgent, 0};
	}
}

Level Get(Kind kind);
void Set(Kind kind, Level level);
// Speaks `text` as its kind's level says.
void Say(Kind kind, std::string_view text, bool urgent = false);
// Speaks `text` so that nothing cuts it off while it is said, cutting in unless its kind waits its
// turn: for news said once that the player cannot have again.
void SayWhole(Kind kind, std::string_view text);

}
