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
constexpr const wchar_t* BattleNetProcess = L"Battle.net.exe";
constexpr const wchar_t* LogRelative  = L"\\d2rloader\\logs\\d2r-access-launcher.log";
constexpr const wchar_t* TomlRelative = L"\\d2rloader\\config\\d2rloader.toml";
constexpr const char*    BoxTitle     = "D2R Access";
// OSI is D2R's Battle.net product code (Blizzard's uninstaller `--uid=osi`); switch from a Lutris installer.
constexpr const wchar_t* BattleNetLaunch = L"--exec=\"launch OSI\"";

constexpr int ExitOk = 0, ExitFailed = 1;

constexpr const wchar_t* SettingsBackupSuffix = L".before-d2raccess";

// The store the copy came from: Steam copies sit in a Steam library or carry steam_appid.txt.
enum class Client { Steam, BattleNet };

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

Client ClientOf(const std::wstring& gameFolder) {
	return InSteamLibrary(gameFolder) || Sys::FileExists(gameFolder + L"\\" + SteamAppId) ? Client::Steam : Client::BattleNet;
}

BattleNetListing ListingOf(const std::wstring& gameFolder) {
	if (const auto db = Sys::BattleNetProductDb()) return ListingIn(*db, Log::Narrow(gameFolder));
	const bool installed = Sys::IsProcessRunning(BattleNetProcess) || Sys::BattleNetInstalledExe().has_value();
	return installed ? BattleNetListing::Unknown : BattleNetListing::NotListed;
}

// Without steam_appid.txt a D2R.exe started outside Steam quits and asks Steam to start it again, so
// the validation watched a closed game; D2RLoader writes the same file the first time it runs.
void EnsureSteamAppId(const std::wstring& gameFolder) {
	const std::wstring path = gameFolder + L"\\" + SteamAppId;
	if (Sys::FileExists(path)) return;
	const std::size_t name      = gameFolder.find_last_of(L'\\');
	const std::size_t common    = name == std::wstring::npos ? name : gameFolder.find_last_of(L'\\', name - 1);
	if (common == std::wstring::npos) return;
	const std::string folderName = Log::Narrow(gameFolder.substr(name + 1));
	for (const std::wstring& manifest : Sys::FilesMatching(gameFolder.substr(0, common) + L"\\appmanifest_*.acf")) {
		const auto text  = Sys::ReadTextFile(manifest);
		const auto appId = text ? ManifestAppIdFor(*text, folderName) : std::nullopt;
		if (!appId) continue;
		Log::Line(Sys::CreateNewTextFile(path, *appId + "\n")
			? std::format("steam_appid.txt missing: written with {} from {}", *appId, Log::Narrow(manifest))
			: "steam_appid.txt missing and could not be written");
		return;
	}
	Log::Line("steam_appid.txt missing and no Steam manifest names this folder");
}

// The game's first-run screens only pass with a click, so the validation would wait for nothing
// (docs/launcher.md, "First run"); "First Time" 1 is the game's own mark that they were seen.
void MarkFirstRunSeen(const std::wstring& settingsPath) {
	if (settingsPath.empty()) return;
	const auto existing = Sys::ReadTextFile(settingsPath);
	if (!existing) {
		Log::Line(Sys::CreateNewTextFile(settingsPath, FirstRunSettings)
			? "Settings.json missing: written with First Time 1, so the game skips its first-run screens"
			: "Settings.json missing and could not be written");
		return;
	}
	const auto marked = WithFirstTimeMarked(*existing);
	if (!marked) return;
	Log::Line(Sys::ReplaceTextFile(settingsPath, *marked, settingsPath + SettingsBackupSuffix)
		? "Settings.json without First Time 1: marked, the original kept as Settings.json.before-d2raccess"
		: "Settings.json without First Time 1 could not be changed");
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
		Speech::Say(Strings::Current().launcherOpeningSteam);
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

// Runs Battle.net.exe with `arguments`: a closed app opens with them, a running one takes them over.
bool RunBattleNet(const std::wstring& arguments) {
	auto exe           = Sys::RunningExePath(BattleNetProcess);
	const bool opening = !exe;
	if (opening) exe = Sys::BattleNetInstalledExe();
	if (!exe) {
		Log::Line("Battle.net is not running and not installed (no Uninstall entry)");
		return false;
	}
	auto process = Sys::Start(*exe, arguments, exe->substr(0, exe->find_last_of(L"\\/")));
	if (!process) {
		Log::Line(std::format("Battle.net.exe failed to start, error {}", GetLastError()));
		return false;
	}
	Sys::CloseHandleOf(*process);
	if (opening) Speech::Say(Strings::Current().launcherOpeningBattleNet);
	Log::Line(arguments.empty() ? "Battle.net started" : "asked Battle.net to launch the game");
	return true;
}

// The copy's store app open, opened if closed. With `launchGame` Battle.net is also asked to launch
// the game, since a direct D2R.exe start on a Battle.net copy only opens the app.
bool EnsureClient(Client client, bool launchGame) {
	if (client == Client::Steam) return EnsureSteam();
	if (launchGame) return RunBattleNet(std::wstring(BattleNetLaunch));
	if (Sys::IsProcessRunning(BattleNetProcess)) {
		Log::Line("Battle.net is running");
		return true;
	}
	return RunBattleNet({});
}

// The unmodded game, online: started here on Steam, picked up by name once Battle.net launched it.
std::optional<Sys::Process> StartGame(Client client, const std::wstring& gameFolder) {
	if (client == Client::Steam) {
		auto game = Sys::Start(gameFolder + L"\\" + GameExe, {}, gameFolder);
		Log::Line(game ? "D2R.exe started" : std::format("D2R.exe failed to start, error {}", GetLastError()));
		return game;
	}
	const std::uint64_t deadline = Sys::NowMs() + GameAppearTimeoutMs;
	for (;;) {
		if (auto game = Sys::OpenRunning(GameExe)) {
			Log::Line("D2R.exe started by Battle.net");
			return game;
		}
		if (Sys::NowMs() >= deadline) {
			Log::Line("Battle.net did not start D2R.exe in time");
			return std::nullopt;
		}
		Sleep(static_cast<DWORD>(PollMs));
	}
}

// Presses Escape on a schedule until Settings.json shows a new date, the game exits or the time runs out.
Wait WaitForNewDate(const Sys::Process& game, const std::wstring& settingsPath, std::optional<std::uint64_t> before) {
	const std::uint64_t deadline = Sys::NowMs() + WaitTimeoutMs;
	std::optional<std::uint64_t> lastPress;
	bool held      = false;
	bool announced = false;
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
				if (!announced) {
					// Said once the game has the focus, so the screen reader's window title does not cut it.
					Speech::SayOverFocus(Strings::Current().launcherValidating);
					announced = true;
				}
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
		Speech::SayOverFocus(text.launcherFailedStartsAnyway);
		return StartLoader(gameFolder, arguments);
	}
	Speech::SayOverFocus(text.launcherFailedExpired);
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
	Speech::SayOverFocus(text.launcherStarted); // started from Run or a menu, the focus has just moved

	if (!Sys::FileExists(gameFolder + L"\\" + GameExe) || !Sys::FileExists(gameFolder + L"\\" + LoaderExe)) {
		Log::Line("D2R.exe or D2RLoader.exe not found next to the launcher");
		Speech::Say(text.launcherMissingFiles);
		return ExitFailed;
	}

	Update::CleanLeftover(gameFolder);
	if (Update::Offer(gameFolder, originalArguments)) return ExitOk;

	const Client client = ClientOf(gameFolder);
	const std::wstring settingsPath = Sys::SettingsJsonPath();
	MarkFirstRunSeen(settingsPath);
	const auto settings             = Sys::ReadTextFile(settingsPath);
	const auto before               = settings ? ParseLastOnline(*settings) : std::nullopt;
	const std::uint64_t now         = Sys::NowUnixNanos();
	const Verdict classified        = Classify(before, now);
	const Verdict verdict           = unforced && classified == Verdict::Fresh ? Verdict::Stale : classified;
	Log::Line(before ? std::format("User Last Online is {} days old", AgeDays(*before, now)) : "User Last Online missing");
	if (unforced) Log::Line("validation forced by --validate");
	if (NoStoreToValidate(InSteamLibrary(gameFolder), ListingOf(gameFolder))) {
		Log::Line("copy of neither store: nothing can validate the license, starting the loader");
		Speech::Say(text.launcherStartingGame);
		return StartLoader(gameFolder, arguments);
	}
	const bool validate = NeedsValidation(verdict);
	Log::Line(validate ? "validating the license online" : "license is fresh");
	if (!EnsureClient(client, validate)) {
		Sys::ShowBox(BoxTitle, client == Client::Steam ? text.launcherNeedsSteam : text.launcherNeedsBattleNet, Sys::BoxKind::Error);
		return ExitFailed;
	}
	if (!validate) {
		Speech::Say(text.launcherStartingGame);
		return StartLoader(gameFolder, arguments);
	}

	if (client == Client::Steam) EnsureSteamAppId(gameFolder);
	auto game = StartGame(client, gameFolder);
	if (!game) {
		if (client == Client::BattleNet) Sys::ShowBox(BoxTitle, text.launcherBattleNetNoGame, Sys::BoxKind::Error);
		return GiveUp(verdict, gameFolder, arguments);
	}
	const Wait wait = WaitForNewDate(*game, settingsPath, before);
	Log::Line(std::format("wait finished: {}", WaitName(wait)));
	CloseGame(*game);
	if (wait != Wait::Validated) return GiveUp(verdict, gameFolder, arguments);
	Speech::SayOverFocus(text.launcherValidated); // the game's window just closed
	Speech::Say(text.launcherStartingGame);
	return StartLoader(gameFolder, arguments);
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	const int code = Run();
	Log::Line(std::format("exit {}", code));
	Speech::Finish();
	return code;
}
