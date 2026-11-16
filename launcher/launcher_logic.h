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
inline constexpr std::uint64_t GameAppearTimeoutMs = 90'000; // Battle.net asked to launch (and maybe opening and signing in), until D2R.exe runs

// Written as Settings.json when the game has none: "First Time" 1 is the game's own mark that its
// first-run screens were seen, which only a mouse click passes (docs/launcher.md, "First run").
inline constexpr std::string_view FirstRunSettings = "{\n    \"First Time\": 1\n}\n";
// An existing Settings.json with "First Time" set to 1, changing nothing else; nothing when it already
// is 1 or the text is not a JSON object.
std::optional<std::string> WithFirstTimeMarked(std::string_view settingsJson);

// Steam installs every game in <library>\steamapps\common\<game>; steam_appid.txt only appears there
// once D2RLoader has run (docs/launcher.md, Flow).
bool InSteamLibrary(std::wstring_view gameFolder);
enum class BattleNetListing { Listed, NotListed, Unknown };
// Whether Battle.net's product.db names `gameFolder` (UTF-8); Unknown when the text is not the format
// this reads, so a real copy is never taken for an unlisted one.
BattleNetListing ListingIn(std::string_view productDb, std::string_view gameFolder);
// A copy of neither store: nothing can validate the license, so the loader starts without it
// (docs/launcher.md, Flow).
inline bool NoStoreToValidate(bool inSteamLibrary, BattleNetListing listing) {
	return !inSteamLibrary && listing == BattleNetListing::NotListed;
}
// The "appid" of a Steam appmanifest_*.acf whose "installdir" is `installDir` (any case).
std::optional<std::string> ManifestAppIdFor(std::string_view manifest, std::string_view installDir);

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

// The game writes the new date on its title screen by itself; one tester's license was accepted under
// the loader only once the online main menu had been reached (issue #8, docs/launcher.md, Flow 5).
inline constexpr int           PressesPastTitle   = 2;      // the first leaves the title screen, the second is spare
inline constexpr std::uint64_t MenuSettleMs       = 5'000;  // from the first, for the main menu to come up
inline constexpr std::uint64_t PastTitleTimeoutMs = 30'000; // from the new date, when no press reaches the game

enum class AfterDate { Wait, MenuReached, NoPress };
// Once the date changed at `changedMs`: whether the presses that reached the game since have passed the title.
AfterDate AfterNewDate(std::uint64_t nowMs, std::uint64_t changedMs, int presses, std::optional<std::uint64_t> firstPressMs);

}
