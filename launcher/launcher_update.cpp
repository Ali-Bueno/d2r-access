#include "launcher_update.h"

#include "launcher_download.h"
#include "launcher_install.h"
#include "launcher_log.h"
#include "launcher_speech.h"
#include "launcher_system.h"
#include "speech_strings.h"
#include "update_logic.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace D2RAccess::Launcher::Update {

namespace {

namespace Sys = D2RAccess::Launcher::System;

constexpr std::string_view InstalledTag = D2R_ACCESS_RELEASE; // empty in a development build

constexpr std::wstring_view LauncherExe   = L"D2RAccess.exe";
// What this process holds open while it installs: itself, and the library it speaks with.
constexpr std::wstring_view FilesInUse[]  = {LauncherExe, Speech::PrismRelativePath};
constexpr const wchar_t* GameExe          = L"D2R.exe";
constexpr const wchar_t* SettingsRelative = L"\\d2rloader\\plugins\\d2r-access\\settings.txt";
constexpr const wchar_t* WorkFolder       = L"D2RAccessUpdate";
constexpr const wchar_t* TarRelative      = L"\\tar.exe";
constexpr const wchar_t* ExtractedFolder  = L"files";
constexpr const wchar_t* BackupFolder     = L"backup";

std::string Format(const char* pattern, const char* first, const char* second = "") {
	const int size = std::snprintf(nullptr, 0, pattern, first, second);
	std::string text(static_cast<std::size_t>(size > 0 ? size : 0), '\0');
	std::snprintf(text.data(), text.size() + 1, pattern, first, second);
	return text;
}

std::optional<Manifest> LatestManifest() {
	std::string body;
	const bool  ok = Download::Fetch(LatestManifestUrl, CheckTimeoutMs, CheckTimeoutMs, [&](const char* data, unsigned long size) {
		body.append(data, size);
		return body.size() <= ManifestMaxBytes;
	});
	if (!ok) return std::nullopt;
	auto manifest = ParseManifest(body);
	if (!manifest) Log::Line("update check: release.json is not valid");
	return manifest;
}

fs::path TempWorkFolder(const std::string& tag) {
	std::wstring temp(MAX_PATH + 1, L'\0');
	temp.resize(GetTempPathW(static_cast<DWORD>(temp.size()), temp.data()));
	return fs::path(temp) / WorkFolder / Sys::Widen(tag);
}

bool Extract(const fs::path& zip, const fs::path& into) {
	std::wstring system(MAX_PATH, L'\0');
	system.resize(GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size())));
	std::error_code ec;
	fs::create_directories(into, ec);
	const auto code = Sys::RunHidden(system + TarRelative, L"-xf \"" + zip.wstring() + L"\" -C \"" + into.wstring() + L"\"", into.wstring(), ExtractTimeoutMs);
	Log::Line(code ? std::format("tar.exe exit {}", *code) : "tar.exe did not run or timed out");
	return code && *code == 0;
}

// Download, checksum, extract, copy; false on any failure with the game folder untouched.
bool Install(const Manifest& manifest, const std::wstring& gameFolder, const fs::path& work) {
	std::error_code ec;
	fs::remove_all(work, ec);
	fs::create_directories(work, ec);
	const fs::path zip = work / Sys::Widen(manifest.asset);
	{
		std::ofstream file(zip, std::ios::binary);
		if (!file) { Log::Line("update: cannot create the download file"); return false; }
		Log::Line(std::format("update: downloading {}", AssetUrl(manifest.tag, manifest.asset)));
		const bool ok = Download::Fetch(AssetUrl(manifest.tag, manifest.asset), DownloadTimeoutMs, DownloadTimeoutMs, [&](const char* data, unsigned long size) {
			file.write(data, size);
			return static_cast<bool>(file);
		});
		if (!ok) return false;
	}
	const auto hash = Download::Sha256Hex(zip);
	if (!hash || _stricmp(hash->c_str(), manifest.sha256.c_str()) != 0) {
		Log::Line(std::format("update: checksum mismatch, got {}", hash.value_or("nothing")));
		return false;
	}
	Log::Line("update: checksum verified");
	if (!Extract(zip, work / ExtractedFolder)) return false;
	return Install::Replace(work / ExtractedFolder, gameFolder, work / BackupFolder, FilesInUse);
}

}

void CleanLeftover(const std::wstring& gameFolder) {
	for (const auto file : FilesInUse) Install::DeleteLeftover(fs::path(gameFolder) / file);
}

bool Offer(const std::wstring& gameFolder, const std::wstring& arguments) {
	if (InstalledTag.empty()) { Log::Line("update check skipped: development build (no release tag)"); return false; }
	const auto installed = ParseReleaseTag(InstalledTag);
	if (!installed) { Log::Line(std::format("update check skipped: unknown installed tag {}", InstalledTag)); return false; }
	const auto settings = Sys::ReadTextFile(gameFolder + SettingsRelative);
	if (settings && !SwitchOn(*settings, UpdateCheckKey)) { Log::Line("update check skipped: update-check=0 in settings.txt"); return false; }
	if (Sys::IsProcessRunning(GameExe)) { Log::Line("update check skipped: D2R.exe is running"); return false; }

	const auto manifest = LatestManifest();
	if (!manifest) { Log::Line("update check failed, going on"); return false; }
	const auto latest = ParseReleaseTag(manifest->tag);
	Log::Line(std::format("update check: installed {}, latest {}", InstalledTag, manifest->tag));
	if (!latest || !IsNewer(*latest, *installed)) { Log::Line("update check: up to date"); return false; }

	const Strings::Table& text = Strings::Current();
	const std::string installedTag(InstalledTag);
	std::string question = Format(text.launcherUpdateAsk, manifest->tag.c_str(), installedTag.c_str());
	if (!manifest->notes.empty()) question += "\n\n" + TruncateChars(manifest->notes, NotesShownChars);
	if (!Sys::ShowBox(text.launcherUpdateTitle, question, Sys::BoxKind::YesNo)) { Log::Line("update declined"); return false; }

	Log::Line(std::format("update accepted: installing {}", manifest->tag));
	const fs::path work = TempWorkFolder(manifest->tag);
	const bool     done = Install(*manifest, gameFolder, work);
	std::error_code ec;
	fs::remove_all(work, ec);
	if (!done) {
		Log::Line("update failed, the old version stays");
		Sys::ShowBox(text.launcherUpdateTitle, Format(text.launcherUpdateFailed, installedTag.c_str()), Sys::BoxKind::Error);
		return false;
	}
	Log::Line(std::format("updated to {}", manifest->tag));
	Sys::ShowBox(text.launcherUpdateTitle, Format(text.launcherUpdated, manifest->tag.c_str()), Sys::BoxKind::Info);
	auto started = Sys::Start(gameFolder + L"\\" + std::wstring(LauncherExe), arguments, gameFolder);
	if (!started) { Log::Line(std::format("the new launcher failed to start, error {}", GetLastError())); return false; }
	Log::Line("the new launcher started");
	Sys::CloseHandleOf(*started);
	return true;
}

}
