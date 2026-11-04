#pragma once

#include <filesystem>
#include <span>
#include <string_view>

// Copies an extracted release over the game folder; on any failure the folder is as it was.
namespace D2RAccess::Launcher::Install {

// `inUse` holds the game-relative paths of files the launcher has open: renamed to `<name>.old` instead of overwritten.
bool Replace(const std::filesystem::path& extracted, const std::filesystem::path& gameFolder, const std::filesystem::path& backup,
	std::span<const std::wstring_view> inUse);

// Deletes `<file>.old` left by a previous update, retrying while the old launcher exits.
void DeleteLeftover(const std::filesystem::path& file);

}
