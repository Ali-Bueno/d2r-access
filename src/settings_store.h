#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

// The mod's switches as the player left them, kept between sessions (the
// user, 2026-09-28: settings did not survive a restart). One "name=1" or
// "name=0" a line; a line that is not one is passed over.
namespace D2RAccess::SettingsStore {
std::string Encode(const std::map<std::string, bool>& values);
std::map<std::string, bool> Decode(std::string_view text);
// Nothing when the file is missing or unreadable.
std::optional<std::string> Load(const std::filesystem::path& file);
// Written beside the file and moved over it, so a crash leaves the old one whole.
bool Save(const std::filesystem::path& file, const std::string& text);
}
