#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

// The mod's switches as the player left them, kept between sessions (the
// user, 2026-09-28: settings did not survive a restart). One "name=value" a
// line: 0 or 1 for a switch, the level of a setting of several values; a
// line that is not one is passed over.
namespace D2RAccess::SettingsStore {
// More digits than any setting has values: a longer number is not one of ours.
inline constexpr size_t MaxValueDigits = 3;
std::string Encode(const std::map<std::string, int>& values);
std::map<std::string, int> Decode(std::string_view text);
// Nothing when the file is missing or unreadable.
std::optional<std::string> Load(const std::filesystem::path& file);
// Written beside the file and moved over it, so a crash leaves the old one whole.
bool Save(const std::filesystem::path& file, const std::string& text);
}
