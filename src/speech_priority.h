#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// How far each kind of news may push past other speech, chosen by the player in
// the settings list (a tester and the user, 2026-10-19). docs/speech-priority.md
namespace D2RAccess::SpeechPriority {

enum class Kind { StatusBegins, StatusEnds, LifeAlerts, BossLife, EnemyNames, GameNotices, QuestAlerts, Places, NearbyItems, Tutorial, Count };
inline constexpr size_t KindCount = static_cast<size_t>(Kind::Count);

// Waits its turn; cuts in only when the mod judges it urgent; always cuts in; cuts in and is not cut off.
enum class Level { Queued, WhenUrgent, CutsIn, Holds, Count };
inline constexpr int LevelCount = static_cast<int>(Level::Count);

// What each kind did before the list existed, except status endings: a tester asked them to cut in (2026-10-19).
inline constexpr std::array<Level, KindCount> Defaults{
	Level::Holds, Level::CutsIn, Level::WhenUrgent, Level::WhenUrgent, Level::WhenUrgent,
	Level::WhenUrgent, Level::WhenUrgent, Level::WhenUrgent, Level::WhenUrgent, Level::Queued};

// How long a held phrase keeps later interruptions waiting (a tester, 2026-10-15: a status
// effect's beginning, "with increased priority over other notifications").
inline constexpr uint64_t HoldMs = 1500;

struct Delivery { bool interrupt = false; uint64_t holdMs = 0; };

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

}
