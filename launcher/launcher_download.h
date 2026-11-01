#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

// WinHTTP download and BCrypt SHA-256 for the launcher's self-update (docs/launcher.md).
namespace D2RAccess::Launcher::Download {

// Receives each chunk of the body; false aborts the download.
using Sink = std::function<bool(const char*, unsigned long)>;

// GET with redirects; every network step times out after `stepMs`, the whole after `totalMs`. Failures are logged.
bool Fetch(std::string_view url, std::uint64_t stepMs, std::uint64_t totalMs, const Sink& sink);

// Lowercase hex SHA-256 of a file; nothing if it cannot be read.
std::optional<std::string> Sha256Hex(const std::filesystem::path& file);

}
