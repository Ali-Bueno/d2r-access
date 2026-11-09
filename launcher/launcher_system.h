#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Win32 glue of the launcher: files, Steam, processes, windows and keys.
namespace D2RAccess::Launcher::System {

struct Process {
	HANDLE handle = nullptr;
	DWORD  id     = 0;
};

std::wstring                ExeFolder();
std::wstring                OwnCommandLine();
bool                        FileExists(const std::wstring& path);
std::optional<std::string>  ReadTextFile(const std::wstring& path);
// Creates the file and its folder; never replaces a file that exists.
bool                        CreateNewTextFile(const std::wstring& path, std::string_view content);
// Replaces the file's text; the first original is kept at `backupPath` and never overwritten.
bool                        ReplaceTextFile(const std::wstring& path, std::string_view content, const std::wstring& backupPath);
std::wstring                SettingsJsonPath();
// Full paths of the files matching a wildcard pattern such as `C:\x\*.acf`.
std::vector<std::wstring>   FilesMatching(const std::wstring& pattern);
std::uint64_t               NowUnixNanos();
std::uint64_t               NowMs();

// Starts an exe with the inherited environment; `arguments` follow the quoted exe path unchanged.
std::optional<Process> Start(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingFolder);
bool                   HasExited(const Process& process);
bool                   WaitForExit(const Process& process, std::uint64_t timeoutMs);
void                   Terminate(const Process& process);
void                   CloseHandleOf(Process& process);

bool IsProcessRunning(const wchar_t* exeName);
// A running process found by exe name, opened to wait for, close or end it.
std::optional<Process>      OpenRunning(const wchar_t* exeName);
// The full exe path of a running process found by exe name.
std::optional<std::wstring> RunningExePath(const wchar_t* exeName);
std::optional<std::wstring> SteamExePath();
bool SteamUserActive();
// Running under Wine or Proton, where the host's Steam is outside the prefix.
bool UnderWine();
// Battle.net.exe in the folder of the app's Uninstall entry, if it is there.
std::optional<std::wstring> BattleNetInstalledExe();
// Battle.net's list of installed games, which names each game's folder; nothing when unreadable.
std::optional<std::string>  BattleNetProductDb();

// The process' top-level visible window, if it has one yet.
HWND FindMainWindow(DWORD processId);
void BringToForeground(HWND window);
void PostCloseToWindows(DWORD processId);
void SetEscapeDown(bool down);

// A topmost Yes/No or OK box above any running window; true for Yes/OK. Text is UTF-8.
enum class BoxKind { YesNo, Info, Error };
bool        ShowBox(std::string_view title, std::string_view text, BoxKind kind);
std::wstring Widen(std::string_view utf8);

// Runs an exe without a window and waits; its exit code, or nothing if it did not start or timed out.
std::optional<DWORD> RunHidden(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingFolder, std::uint64_t timeoutMs);

}
