#include "launcher_system.h"

#include <shlobj.h>
#include <tlhelp32.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <vector>

namespace D2RAccess::Launcher::System {

namespace {

constexpr const wchar_t* SteamKey         = L"Software\\Valve\\Steam";
constexpr const wchar_t* SteamExeValue    = L"SteamExe";
constexpr const wchar_t* SteamActiveKey   = L"Software\\Valve\\Steam\\ActiveProcess";
constexpr const wchar_t* BattleNetUninstallKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Battle.net";
constexpr const wchar_t* InstallLocationValue  = L"InstallLocation";
constexpr const wchar_t* BattleNetExeName      = L"Battle.net.exe";
constexpr const wchar_t* SteamUserValue   = L"ActiveUser";
constexpr const wchar_t* SettingsRelative = L"Diablo II Resurrected\\Settings.json";
constexpr DWORD          LongPathChars    = 32'768; // Windows' longest path, in UTF-16 units

std::optional<DWORD> FindProcessId(const wchar_t* exeName) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return std::nullopt;
	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	std::optional<DWORD> found;
	for (BOOL ok = Process32FirstW(snapshot, &entry); ok && !found; ok = Process32NextW(snapshot, &entry)) {
		if (_wcsicmp(entry.szExeFile, exeName) == 0) found = entry.th32ProcessID;
	}
	CloseHandle(snapshot);
	return found;
}

struct WindowSearch {
	DWORD pid;
	HWND  found;
};

BOOL CALLBACK FindVisibleTopLevel(HWND window, LPARAM param) {
	auto& search = *reinterpret_cast<WindowSearch*>(param);
	DWORD pid    = 0;
	GetWindowThreadProcessId(window, &pid);
	if (pid == search.pid && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
		search.found = window;
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK PostCloseIfOwned(HWND window, LPARAM param) {
	DWORD pid = 0;
	GetWindowThreadProcessId(window, &pid);
	if (pid == static_cast<DWORD>(param)) PostMessageW(window, WM_CLOSE, 0, 0);
	return TRUE;
}

}

std::wstring ExeFolder() {
	std::wstring path(MAX_PATH, L'\0');
	for (;;) {
		const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (size < path.size()) {
			path.resize(size);
			break;
		}
		path.resize(path.size() * 2);
	}
	return path.substr(0, path.find_last_of(L"\\/"));
}

std::wstring OwnCommandLine() { return GetCommandLineW(); }

bool FileExists(const std::wstring& path) {
	const DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::optional<std::string> ReadTextFile(const std::wstring& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) return std::nullopt;
	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::wstring SettingsJsonPath() {
	PWSTR known = nullptr;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_SavedGames, 0, nullptr, &known))) {
		CoTaskMemFree(known);
		return {};
	}
	std::wstring path = std::wstring(known) + L"\\" + SettingsRelative;
	CoTaskMemFree(known);
	return path;
}

std::uint64_t NowUnixNanos() {
	using namespace std::chrono;
	return static_cast<std::uint64_t>(duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
}

std::uint64_t NowMs() { return GetTickCount64(); }

std::optional<Process> Start(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingFolder) {
	std::wstring commandLine = L"\"" + exe + L"\"";
	if (!arguments.empty()) commandLine += L" " + arguments;
	STARTUPINFOW        startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION info{};
	if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, workingFolder.c_str(), &startup, &info)) {
		return std::nullopt;
	}
	CloseHandle(info.hThread);
	return Process{info.hProcess, info.dwProcessId};
}

bool HasExited(const Process& process) { return WaitForSingleObject(process.handle, 0) == WAIT_OBJECT_0; }

bool WaitForExit(const Process& process, std::uint64_t timeoutMs) {
	return WaitForSingleObject(process.handle, static_cast<DWORD>(timeoutMs)) == WAIT_OBJECT_0;
}

void Terminate(const Process& process) { TerminateProcess(process.handle, 1); }

void CloseHandleOf(Process& process) {
	if (process.handle) CloseHandle(process.handle);
	process.handle = nullptr;
}

bool IsProcessRunning(const wchar_t* exeName) { return FindProcessId(exeName).has_value(); }

std::optional<Process> OpenRunning(const wchar_t* exeName) {
	const auto id = FindProcessId(exeName);
	if (!id) return std::nullopt;
	HANDLE handle = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *id);
	if (!handle) return std::nullopt;
	return Process{handle, *id};
}

std::optional<std::wstring> RunningExePath(const wchar_t* exeName) {
	const auto id = FindProcessId(exeName);
	if (!id) return std::nullopt;
	HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *id);
	if (!handle) return std::nullopt;
	std::wstring path(LongPathChars, L'\0');
	DWORD        size = static_cast<DWORD>(path.size());
	const BOOL   ok   = QueryFullProcessImageNameW(handle, 0, path.data(), &size);
	CloseHandle(handle);
	if (!ok) return std::nullopt;
	path.resize(size);
	return path;
}

std::optional<std::wstring> SteamExePath() {
	wchar_t buffer[MAX_PATH * 2]{};
	DWORD   size = sizeof(buffer) - sizeof(wchar_t);
	if (RegGetValueW(HKEY_CURRENT_USER, SteamKey, SteamExeValue, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) return std::nullopt;
	return std::wstring(buffer);
}

std::optional<std::wstring> BattleNetInstalledExe() {
	wchar_t buffer[MAX_PATH * 2]{};
	DWORD   size = sizeof(buffer) - sizeof(wchar_t);
	// The app is 32-bit, so its Uninstall entry lives in the 32-bit registry view.
	if (RegGetValueW(HKEY_LOCAL_MACHINE, BattleNetUninstallKey, InstallLocationValue, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, nullptr, buffer, &size) != ERROR_SUCCESS) {
		return std::nullopt;
	}
	std::wstring exe = std::wstring(buffer) + L"\\" + BattleNetExeName;
	if (!FileExists(exe)) return std::nullopt;
	return exe;
}

bool SteamUserActive() {
	DWORD user = 0;
	DWORD size = sizeof(user);
	return RegGetValueW(HKEY_CURRENT_USER, SteamActiveKey, SteamUserValue, RRF_RT_REG_DWORD, nullptr, &user, &size) == ERROR_SUCCESS && user != 0;
}

HWND FindMainWindow(DWORD processId) {
	WindowSearch search{processId, nullptr};
	EnumWindows(FindVisibleTopLevel, reinterpret_cast<LPARAM>(&search));
	return search.found;
}

void BringToForeground(HWND window) {
	if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
	// Attaching to the foreground thread lets SetForegroundWindow succeed from a background process.
	const DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
	const DWORD ownThread        = GetCurrentThreadId();
	const bool  attached         = foregroundThread != 0 && foregroundThread != ownThread && AttachThreadInput(ownThread, foregroundThread, TRUE);
	BringWindowToTop(window);
	SetForegroundWindow(window);
	if (attached) AttachThreadInput(ownThread, foregroundThread, FALSE);
}

void PostCloseToWindows(DWORD processId) {
	EnumWindows(PostCloseIfOwned, static_cast<LPARAM>(processId));
}

void SetEscapeDown(bool down) {
	INPUT input{};
	input.type       = INPUT_KEYBOARD;
	input.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(VK_ESCAPE, MAPVK_VK_TO_VSC));
	input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
	SendInput(1, &input, sizeof(input));
}

std::wstring Widen(std::string_view utf8) {
	if (utf8.empty()) return {};
	const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring result(static_cast<std::size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
	return result;
}

bool ShowBox(std::string_view title, std::string_view text, BoxKind kind) {
	const UINT icon = kind == BoxKind::YesNo ? MB_YESNO | MB_ICONQUESTION : kind == BoxKind::Info ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONERROR;
	const int  answer = MessageBoxW(nullptr, Widen(text).c_str(), Widen(title).c_str(), icon | MB_TOPMOST | MB_SETFOREGROUND);
	return answer == IDYES || answer == IDOK;
}

std::optional<DWORD> RunHidden(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingFolder, std::uint64_t timeoutMs) {
	std::wstring commandLine = L"\"" + exe + L"\" " + arguments;
	STARTUPINFOW        startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION info{};
	if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, workingFolder.c_str(), &startup, &info)) {
		return std::nullopt;
	}
	CloseHandle(info.hThread);
	std::optional<DWORD> code;
	if (WaitForSingleObject(info.hProcess, static_cast<DWORD>(timeoutMs)) == WAIT_OBJECT_0) {
		DWORD exitCode = 0;
		if (GetExitCodeProcess(info.hProcess, &exitCode)) code = exitCode;
	} else {
		TerminateProcess(info.hProcess, 1);
	}
	CloseHandle(info.hProcess);
	return code;
}

}
