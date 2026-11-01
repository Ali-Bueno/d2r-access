#pragma once

#include <filesystem>
#include <string_view>

// Copies an extracted release over the game folder; on any failure the folder is as it was.
namespace D2RAccess::Launcher::Install {

// `runningExe` is the file name of the running launcher: renamed to `<name>.old` instead of overwritten.
bool Replace(const std::filesystem::path& extracted, const std::filesystem::path& gameFolder, const std::filesystem::path& backup,
	std::wstring_view runningExe);

// Deletes `<exe>.old` left by a previous update, retrying while the old launcher exits.
void DeleteLeftover(const std::filesystem::path& exe);

}
