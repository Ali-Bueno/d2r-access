#include "process_lifetime.h"
#include "test_support.h"
#include <string>
#include <windows.h>

namespace {
using Action = bool(*)() noexcept;
void Child(const wchar_t* fixture, const wchar_t* mode, DWORD expected) {
	wchar_t executable[32768]{};
	Check(GetModuleFileNameW(nullptr, executable, 32768) != 0, "test executable is located");
	std::wstring command = L"\"" + std::wstring(executable) + L"\" \"" + fixture + L"\" " + mode;
	STARTUPINFOW startup{ sizeof(startup) };
	PROCESS_INFORMATION process{};
	Check(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process), "isolated process-exit test starts");
	const auto result = WaitForSingleObject(process.hProcess, 5000);
	if (result != WAIT_OBJECT_0) TerminateProcess(process.hProcess, 99);
	DWORD code = 0; GetExitCodeProcess(process.hProcess, &code);
	CloseHandle(process.hThread); CloseHandle(process.hProcess);
	Check(result == WAIT_OBJECT_0 && code == expected, "process exits without terminate or a loader-lock join while worker is active");
}
}
int wmain(int argc, wchar_t** argv) {
	Check(argc >= 2, "worker DLL fixture supplied");
	const auto module = LoadLibraryW(argv[1]);
	Check(module != nullptr, "worker DLL loads");
	const auto start = reinterpret_cast<Action>(GetProcAddress(module, "StartWorker"));
	const auto stop = reinterpret_cast<Action>(GetProcAddress(module, "StopWorker"));
	Check(start && stop, "fixture exports normal worker lifecycle");
	Check(start(), "worker starts");
	if (argc > 2) {
		if (std::wstring_view(argv[2]) == L"--exit") ExitProcess(17);
		return 18; // CRT return -> ExitProcess -> DLL_PROCESS_DETACH, worker still joinable
	}
	Check(!start(), "second start cannot overwrite a joinable worker");
	Check(stop() && stop(), "normal stop joins and is idempotent");
	DWORD before = 0, after = 0;
	Check(GetProcessHandleCount(GetCurrentProcess(), &before), "initial handle count is readable");
	for (int i = 0; i < 4; ++i) Check(start() && stop(), "worker can restart and join normally");
	Check(GetProcessHandleCount(GetCurrentProcess(), &after) && before == after, "normal stop releases all worker/event handles");
	Check(FreeLibrary(module), "normal explicit shutdown permits DLL unload");
	Child(argv[1], L"--exit", 17);
	Child(argv[1], L"--return", 18);
	Passed();
}
