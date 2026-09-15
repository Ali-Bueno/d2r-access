#pragma once

#include <optional>
#include <string>
#include <string_view>

// Reads the game's own options file so the plugin can react to settings the
// UI update events don't carry a value for (see input_direction.h for the
// companion piece that infers direction on those same value controls).
//
// File location, as written by the game itself whenever the player applies
// settings:
//   %USERPROFILE%\Saved Games\Diablo II Resurrected\Settings.json
// resolved via SHGetKnownFolderPath(FOLDERID_SavedGames), which honors a
// user-redirected Saved Games folder; falls back to %USERPROFILE%\Saved Games
// only if that shell call itself fails.
//
// File format: a flat JSON object of string keys to scalar values, e.g.
//   {"VSync": 1, "NPC Speech": 2, "Master Volume": 100, "Window Mode": 1,
//    "Text to Speech": 1, "Screen Resolution (Windowed)": "",
//    "Aux Battle.net": "216.148.246.34"}
// Only scalars are read; nested objects/arrays (none expected in this file)
// are skipped. A string value that parses fully as a number counts as one
// (e.g. a numeric string); other strings (like the IP above) are skipped.
namespace D2RAccess::SettingsFile {

// The game's save folder (<Saved Games>\Diablo II Resurrected), which also
// holds the character saves; empty when it cannot be resolved.
std::wstring SavedGamesDirectory();

// Re-reads the file if its last-write time changed since the previous read
// attempt. Returns whether values are currently loaded (true if a prior read
// already succeeded, even when nothing changed this call).
bool Reload();

// Value for a key, matched by NormalizeKey() against the file's keys
// (normalized the same way when the file is parsed). std::nullopt when
// nothing is loaded or the key is absent/non-numeric.
std::optional<double> Number(std::string_view normalizedKey);

// Raw string value for a key (e.g. "Screen Resolution (Windowed)" holds
// "WxH" once the player applied one); std::nullopt when nothing is loaded,
// the key is absent, or its value is not a string.
std::optional<std::string> Text(std::string_view normalizedKey);

// Lowercases and keeps only [a-z0-9], so "NPC Speech", "npcspeech" and
// "NPC-Speech" all address the same stored value.
std::string NormalizeKey(std::string_view key);

// Convenience for UI update-command names such as "SetNPCSpeech" or
// "SetTextToSpeech": strips a leading "Set", normalizes what remains, and
// looks it up (e.g. "SetMasterVolume" -> "mastervolume"). The few commands
// whose key in the file is named differently are mapped explicitly.
std::optional<double> NumberForCommand(std::string_view updateCommand);

}
