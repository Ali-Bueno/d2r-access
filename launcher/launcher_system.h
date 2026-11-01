#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
std::wstring                SettingsJsonPath();
std::uint64_t               NowUnixNanos();
std::uint64_t               NowMs();

// Starts an exe with the inherited environment; `arguments` follow the quoted exe path unchanged.
std::optional<Process> Start(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingFolder);
bool                   HasExited(const Process& process);
bool                   WaitForExit(const Process& process, std::uint64_t timeoutMs);
void                   Terminate(const Process& process);
void                   CloseHandleOf(Process& process);

bool IsProcessRunning(const wchar_t* exeName);
std::optional<std::wstring> SteamExePath();
bool SteamUserActive();

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
