#include "input_stack.h"

#include "async_log.h"

#include <windows.h>

#include <xinput.h>

#include <array>
#include <string>

namespace D2RAccess::InputStack {

namespace {

// Every input API a Windows game realistically reads a pad through. XInput
// ships in several versions and a game links exactly one; GameInput and
// Windows.Gaming.Input are the modern alternatives, and DirectInput the legacy
// one. Which of these is present tells us where a synthetic pad must go.
struct Candidate {
	const wchar_t* module;
	const char*    label;
};

constexpr std::array kCandidates {
	Candidate {L"xinput1_4.dll", "xinput1_4"},
	Candidate {L"xinput1_3.dll", "xinput1_3"},
	Candidate {L"xinput9_1_0.dll", "xinput9_1_0"},
	Candidate {L"xinputuap.dll", "xinputuap"},
	Candidate {L"GameInput.dll", "GameInput"},
	Candidate {L"Windows.Gaming.Input.dll", "Windows.Gaming.Input"},
	Candidate {L"dinput8.dll", "dinput8"},
};

// The undocumented ordinal-100 export. Most titles call it rather than the
// named XInputGetState, because it is the one that reports the Guide button --
// so a hook on the named function alone can miss every call the game makes.
constexpr WORD kGetStateExOrdinal = 100;

bool g_captured = false;
std::array<bool, kCandidates.size()> g_present {};

std::string Loaded(const std::array<bool, kCandidates.size()>& flags) {
	std::string list;
	for (size_t at = 0; at < kCandidates.size(); ++at) {
		if (!flags[at]) continue;
		if (!list.empty()) list += ", ";
		list += kCandidates[at].label;
	}
	return list.empty() ? std::string {"none"} : list;
}

// GetModuleHandleW never loads anything, so asking does not itself change the
// answer for the next caller.
std::array<bool, kCandidates.size()> Sample() {
	std::array<bool, kCandidates.size()> flags {};
	for (size_t at = 0; at < kCandidates.size(); ++at) {
		flags[at] = GetModuleHandleW(kCandidates[at].module) != nullptr;
	}
	return flags;
}

// Which XInput entry points the loaded module exposes, and whether a physical
// pad is on any slot.
//
// The pad count is DIAGNOSTIC ONLY. Keyboard support fabricates a controller
// unconditionally and never merges with a physically connected one: merging
// would make owning a gamepad a prerequisite for using the keyboard, which
// defeats the feature outright. It is reported here so that a live test stays
// honest -- with a real pad attached there is no telling whether the game
// answered it or us -- and must never become an input to an injection
// decision, because there is no decision to make.
void ReportXInput(const D2RL::PluginContext* context) {
	for (const Candidate& candidate : kCandidates) {
		const HMODULE module = GetModuleHandleW(candidate.module);
		if (module == nullptr) continue;
		const FARPROC named   = GetProcAddress(module, "XInputGetState");
		const FARPROC ordinal = GetProcAddress(module, MAKEINTRESOURCEA(kGetStateExOrdinal));
		if (named == nullptr && ordinal == nullptr) continue;
		AsyncLog::InfoF(context, "[input] %s exports XInputGetState=%s, ordinal 100=%s",
			candidate.label, named != nullptr ? "yes" : "no", ordinal != nullptr ? "yes" : "no");

		if (named == nullptr) continue;
		const auto getState = reinterpret_cast<DWORD(WINAPI*)(DWORD, XINPUT_STATE*)>(named);
		std::string connected;
		for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
			XINPUT_STATE state {};
			if (getState(slot, &state) != ERROR_SUCCESS) continue;
			if (!connected.empty()) connected += ", ";
			connected += std::to_string(slot);
		}
		AsyncLog::InfoF(context, "[input] %s pads connected: %s", candidate.label,
			connected.empty() ? "none" : connected.c_str());
		return;
	}
}

}

void Capture() {
	if (g_captured) return;
	g_captured = true;
	g_present  = Sample();
}

void Report(const D2RL::PluginContext* context) {
	if (context == nullptr) return;
	if (!g_captured) Capture();
	const std::string atLoad = Loaded(g_present);
	const std::string now    = Loaded(Sample());
	AsyncLog::InfoF(context, "[input] input modules at plugin load: %s", atLoad.c_str());
	if (now != atLoad) {
		AsyncLog::InfoF(context, "[input] input modules now: %s (the difference was loaded after us)", now.c_str());
	}
	ReportXInput(context);
}

}
