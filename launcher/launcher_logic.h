#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Pure decisions of the D2RAccess launcher; no Win32 here so tests run without the game.
// Why the launcher exists: docs/launcher.md.
namespace D2RAccess::Launcher {

inline constexpr int ValidateAfterDays = 20; // older than this, the license is refreshed online first
inline constexpr int OfflineLimitDays  = 30; // Blizzard blocks offline play from here

inline constexpr std::uint64_t NanosPerSecond = 1'000'000'000ULL;
inline constexpr std::uint64_t SecondsPerDay  = 86'400ULL;
inline constexpr std::uint64_t NanosPerDay    = NanosPerSecond * SecondsPerDay;

inline constexpr std::uint64_t EscapeHoldMs   = 600;    // hold-to-skip videos needs at least 500
inline constexpr std::uint64_t EscapePeriodMs = 1'500;  // between the starts of two presses
inline constexpr std::uint64_t PollMs         = 250;    // Settings.json poll and key scheduler tick
inline constexpr std::uint64_t WaitTimeoutMs  = 120'000;
inline constexpr std::uint64_t CloseWaitMs    = 10'000; // after WM_CLOSE, before TerminateProcess
inline constexpr std::uint64_t KillWaitMs     = 5'000;
inline constexpr std::uint64_t SteamTimeoutMs = 60'000;

enum class Verdict { Fresh, Stale, Expired };

// "User Last Online" of Settings.json: nanoseconds since the Unix epoch.
std::optional<std::uint64_t> ParseLastOnline(std::string_view settingsJson);

// Whole days since the date; zero for a date in the future.
std::uint64_t AgeDays(std::uint64_t lastOnlineNanos, std::uint64_t nowNanos);

// A missing date counts as expired: nothing proves the license is current.
Verdict Classify(std::optional<std::uint64_t> lastOnlineNanos, std::uint64_t nowNanos);

inline bool NeedsValidation(Verdict verdict) { return verdict != Verdict::Fresh; }

// True when the new reading holds a date different from the one read before the game started.
bool DateChanged(std::optional<std::uint64_t> before, std::optional<std::uint64_t> after);

// `text_locale` under [d2rloader] in d2rloader.toml; empty or absent gives nothing.
std::optional<std::string> ParseTextLocale(std::string_view toml);

// The command line after the program name, unchanged (what Steam's %command% appends).
std::wstring ArgumentsAfterProgram(const std::wstring& commandLine);

// Forces the online validation whatever the date's age; never forwarded to the loader.
inline constexpr std::wstring_view ValidateFlag = L"--validate";
// `arguments` without a leading `flag` token; nothing when they do not start with it.
std::optional<std::wstring> WithoutLeadingFlag(std::wstring_view arguments, std::wstring_view flag);

enum class KeyAction { None, Press, Release };

// The Escape press-and-hold schedule while waiting for the date to change.
KeyAction NextKeyAction(std::uint64_t nowMs, std::optional<std::uint64_t> lastPressMs, bool held);

}
