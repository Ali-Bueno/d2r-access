#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Pure parts of the launcher's self-update; no Win32 here so tests run without the game.
// Flow and constants: docs/launcher.md, "Self-update".
namespace D2RAccess::Launcher::Update {

inline constexpr std::string_view LatestManifestUrl = "https://github.com/Ali-Bueno/d2r-access/releases/latest/download/release.json";
inline constexpr std::string_view ReleaseDownloadUrl = "https://github.com/Ali-Bueno/d2r-access/releases/download/";
inline constexpr std::string_view UpdateCheckKey = "update-check"; // the plugin's Ctrl+S switch in settings.txt

inline constexpr std::uint64_t CheckTimeoutMs    = 5'000;
inline constexpr std::uint64_t DownloadTimeoutMs = 600'000;
inline constexpr std::uint64_t ExtractTimeoutMs  = 300'000;
inline constexpr std::uint64_t LeftoverWaitMs    = 300;
inline constexpr std::size_t   ManifestMaxBytes  = 1 << 20;
inline constexpr std::size_t   NotesShownChars   = 1'500;
inline constexpr int           LeftoverTries     = 10;

// A release tag as a comparable date: `test-YYYY-MM-DD[-N]` or `vYYYY.MM.DD[-N]`; N is the release number that day.
struct ReleaseTag {
	int year = 0, month = 0, day = 0, number = 1;
	auto operator<=>(const ReleaseTag&) const = default;
};

std::optional<ReleaseTag> ParseReleaseTag(std::string_view tag);

// True only when `latest` is strictly newer than `installed`.
bool IsNewer(const ReleaseTag& latest, const ReleaseTag& installed);

// A top-level string member of a JSON object; nothing when it is missing or the text is not valid enough.
std::optional<std::string> ExtractJsonString(std::string_view json, std::string_view key);

struct Manifest {
	std::string tag, asset, sha256, notes;
};

// A manifest whose tag parses, asset is a plain file name and sha256 is 64 hex digits; notes are optional.
std::optional<Manifest> ParseManifest(std::string_view json);

// `key=value` lines: the switch is on unless its value is 0 (missing means on).
bool SwitchOn(std::string_view settingsText, std::string_view key);

// Files an update never overwrites once they exist: any .toml and the plugin's player files.
bool IsProtectedPath(std::wstring_view relativePath);

// No folders, no `..`: safe to join under a work folder and a URL.
bool IsPlainFileName(std::string_view name);

// The first `maxChars` characters of UTF-8 text, with "..." when cut.
std::string TruncateChars(std::string_view text, std::size_t maxChars);

std::string AssetUrl(std::string_view tag, std::string_view asset);

}
