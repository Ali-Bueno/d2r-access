#include "launcher_log.h"
#include "launcher_logic.h"
#include "launcher_speech.h"
#include "launcher_system.h"
#include "launcher_update.h"
#include "speech_strings.h"

#include <windows.h>

#include <format>
#include <string>

// Flow and rationale: docs/launcher.md.
namespace {

using namespace D2RAccess;
using namespace D2RAccess::Launcher;
namespace Sys = D2RAccess::Launcher::System;

constexpr const wchar_t* GameExe      = L"D2R.exe";
constexpr const wchar_t* LoaderExe    = L"D2RLoader.exe";
constexpr const wchar_t* SteamAppId   = L"steam_appid.txt";
constexpr const wchar_t* SteamProcess = L"steam.exe";
constexpr const wchar_t* SteamSilent  = L"-silent";
constexpr const wchar_t* LogRelative  = L"\\d2rloader\\logs\\d2r-access-launcher.log";
constexpr const wchar_t* TomlRelative = L"\\d2rloader\\config\\d2rloader.toml";

constexpr int ExitOk = 0, ExitFailed = 1;

enum class Wait { Validated, Timeout, GameExited };

const char* WaitName(Wait wait) {
	switch (wait) {
	case Wait::Validated: return "date changed";
	case Wait::Timeout: return "timeout";
	default: return "game exited by itself";
	}
}

void ChooseLanguage(const std::wstring& gameFolder) {
	const auto toml = Sys::ReadTextFile(gameFolder + TomlRelative);
	const auto code = toml ? ParseTextLocale(*toml) : std::nullopt;
	const auto language = code ? Strings::FromLocale(*code) : std::nullopt;
	Strings::Select(language.value_or(Strings::Language::enUS));
	Log::Line(std::format("language: {}", code ? *code : "default"));
}

int StartLoader(const std::wstring& gameFolder, const std::wstring& arguments) {
	auto loader = Sys::Start(gameFolder + L"\\" + LoaderExe, arguments, gameFolder);
	if (!loader) {
		Log::Line(std::format("D2RLoader.exe failed to start, error {}", GetLastError()));
		return ExitFailed;
	}
	Log::Line("D2RLoader.exe started");
	Sys::CloseHandleOf(*loader);
	return ExitOk;
}

bool EnsureSteam() {
	if (!Sys::IsProcessRunning(SteamProcess)) {
		const auto steam = Sys::SteamExePath();
		if (!steam) {
			Log::Line("Steam is not running and its path is not in the registry");
			return false;
		}
		auto process = Sys::Start(*steam, SteamSilent, steam->substr(0, steam->find_last_of(L"\\/")));
		if (!process) {
			Log::Line("Steam failed to start");
			return false;
		}
		Sys::CloseHandleOf(*process);
		Log::Line("Steam started");
	}
	const std::uint64_t deadline = Sys::NowMs() + SteamTimeoutMs;
	while (!Sys::SteamUserActive()) {
		if (Sys::NowMs() >= deadline) {
			Log::Line("timed out waiting for a Steam user");
			return false;
		}
		Sleep(static_cast<DWORD>(PollMs));
	}
	Log::Line("Steam user active");
	return true;
}

// Presses Escape on a schedule until Settings.json shows a new date, the game exits or the time runs out.
Wait WaitForNewDate(const Sys::Process& game, const std::wstring& settingsPath, std::optional<std::uint64_t> before) {
	const std::uint64_t deadline = Sys::NowMs() + WaitTimeoutMs;
	std::optional<std::uint64_t> lastPress;
	bool held = false;
	Wait result;
	for (;;) {
		const std::uint64_t now = Sys::NowMs();
		const auto text         = Sys::ReadTextFile(settingsPath);
		if (DateChanged(before, text ? ParseLastOnline(*text) : std::nullopt)) { result = Wait::Validated; break; }
		if (Sys::HasExited(game)) { result = Wait::GameExited; break; }
		if (now >= deadline) { result = Wait::Timeout; break; }
		switch (NextKeyAction(now, lastPress, held)) {
		case KeyAction::Press:
			if (const HWND window = Sys::FindMainWindow(game.id)) {
				Sys::BringToForeground(window);
				// Never press into another program when Windows refused to bring the game forward.
				if (GetForegroundWindow() != window) break;
				Sys::SetEscapeDown(true);
				held      = true;
				lastPress = now;
			}
			break;
		case KeyAction::Release:
			Sys::SetEscapeDown(false);
			held = false;
			break;
		case KeyAction::None: break;
		}
		Sleep(static_cast<DWORD>(PollMs));
	}
	if (held) Sys::SetEscapeDown(false);
	return result;
}

void CloseGame(Sys::Process& game) {
	if (!Sys::HasExited(game)) {
		Sys::PostCloseToWindows(game.id);
		if (!Sys::WaitForExit(game, CloseWaitMs)) {
			Log::Line("the game ignored WM_CLOSE, terminating it");
			Sys::Terminate(game);
			Sys::WaitForExit(game, KillWaitMs);
		}
	}
	Log::Line("the game is closed");
	Sys::CloseHandleOf(game);
}

// The validation did not happen: inside the 30 days the game still starts, past them it does not.
int GiveUp(Verdict verdict, const std::wstring& gameFolder, const std::wstring& arguments) {
	const Strings::Table& text = Strings::Current();
	if (verdict == Verdict::Stale) {
		Speech::Say(text.launcherFailedStartsAnyway);
		return StartLoader(gameFolder, arguments);
	}
	Speech::Say(text.launcherFailedExpired);
	Log::Line("license past the offline limit, the loader is not started");
	return ExitFailed;
}

int Run() {
	const std::wstring gameFolder = Sys::ExeFolder();
	std::wstring arguments        = ArgumentsAfterProgram(Sys::OwnCommandLine());
	const std::wstring originalArguments = arguments;
	const auto unforced           = WithoutLeadingFlag(arguments, ValidateFlag);
	if (unforced) arguments = *unforced;
	Log::Open(gameFolder + LogRelative);
	Speech::Configure(gameFolder);
	Log::Line(std::format("start in {}", Log::Narrow(gameFolder)));
	ChooseLanguage(gameFolder);
	const Strings::Table& text = Strings::Current();

	if (!Sys::FileExists(gameFolder + L"\\" + GameExe) || !Sys::FileExists(gameFolder + L"\\" + LoaderExe)) {
		Log::Line("D2R.exe or D2RLoader.exe not found next to the launcher");
		Speech::Say(text.launcherMissingFiles);
		return ExitFailed;
	}

	Update::CleanLeftover(gameFolder);
	if (Update::Offer(gameFolder, originalArguments)) return ExitOk;

	const std::wstring settingsPath = Sys::SettingsJsonPath();
	const auto settings             = Sys::ReadTextFile(settingsPath);
	const auto before               = settings ? ParseLastOnline(*settings) : std::nullopt;
	const std::uint64_t now         = Sys::NowUnixNanos();
	const Verdict classified        = Classify(before, now);
	const Verdict verdict           = unforced && classified == Verdict::Fresh ? Verdict::Stale : classified;
	Log::Line(before ? std::format("User Last Online is {} days old", AgeDays(*before, now)) : "User Last Online missing");
	if (unforced) Log::Line("validation forced by --validate");
	if (!NeedsValidation(verdict)) {
		Log::Line("license is fresh");
		return StartLoader(gameFolder, arguments);
	}

	Log::Line("validating the license online");
	Speech::Say(text.launcherValidating);
	if (Sys::FileExists(gameFolder + L"\\" + SteamAppId) && !EnsureSteam()) {
		Speech::Say(text.launcherSteamTimeout);
		return ExitFailed;
	}
	auto game = Sys::Start(gameFolder + L"\\" + GameExe, {}, gameFolder);
	if (!game) {
		Log::Line(std::format("D2R.exe failed to start, error {}", GetLastError()));
		return GiveUp(verdict, gameFolder, arguments);
	}
	Log::Line("D2R.exe started");
	const Wait wait = WaitForNewDate(*game, settingsPath, before);
	Log::Line(std::format("wait finished: {}", WaitName(wait)));
	CloseGame(*game);
	if (wait != Wait::Validated) return GiveUp(verdict, gameFolder, arguments);
	Speech::Say(text.launcherValidated);
	return StartLoader(gameFolder, arguments);
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	const int code = Run();
	Log::Line(std::format("exit {}", code));
	Speech::Finish();
	return code;
}
