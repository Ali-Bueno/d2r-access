#include "pad_filter.h"

#include "async_log.h"

#include <D2RLPlugin/api.h>

#include <windows.h>

#include <xinput.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

namespace D2RAccess::PadFilter {

namespace {

using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

constexpr uint16_t RightThumb = XINPUT_GAMEPAD_RIGHT_THUMB;

// The modules the game may have taken XInputGetState from, in the order to
// prefer them. D2R's is xinput9_1_0 (MEASURED 2026-09-20, "[input] input
// modules at plugin load: xinput9_1_0"). The mod's own reader loads
// xinput1_4, but it keeps its pointer in the mod, not in the game's image, so
// nothing the mod reads is ever filtered.
constexpr const wchar_t* Modules[] = {L"xinput9_1_0.dll", L"xinput1_4.dll", L"xinput1_3.dll"};

// Install runs at load; if the slot is not filled yet, the UI tick looks again
// every two seconds for half a minute before giving up.
constexpr uint64_t RetryMs = 2000;
constexpr int      Retries = 15;

GetStateFn                         g_original = nullptr;
std::vector<void**>                g_slots;               // the game's slots now pointing at Filtered
std::atomic<bool>                  g_claimed { false };
std::atomic<bool>                  g_keptPress { false }; // for the UI tick's log line
int                                g_retries = 0;
uint64_t                           g_nextTry = 0;
// Touched only on the thread the game polls from: D2R reads its pads from one
// call site (MEASURED offline 2026-09-20, build 3.3.93787).
std::array<Press, XUSER_MAX_COUNT> g_press {};

DWORD WINAPI Filtered(DWORD user, XINPUT_STATE* state) {
	const DWORD result = g_original(user, state);
	if (result != ERROR_SUCCESS || state == nullptr || user >= XUSER_MAX_COUNT) return result;
	Press& press = g_press[user];
	const bool wasKept = press.down && press.kept;
	state->Gamepad.wButtons = Filter(state->Gamepad.wButtons, g_claimed.load(std::memory_order_relaxed), press);
	if (!wasKept && press.down && press.kept) g_keptPress.store(true, std::memory_order_relaxed);
	return result;
}

// The game's own pointers to `target`. D2R's image is protected: its header
// lists no XInput import at all, and the protector fills the real import slots
// at run time. MEASURED offline 2026-09-21 in the 3.3.93787 image dump: the pad
// poll at 0x141210AC2 calls a thunk, jmp qword ptr [rip+...], whose slot at RVA
// 0x1CB1258 holds the XInput function. Finding slots by the value they hold
// keeps this free of build-specific addresses. Only data pages are read --
// never code -- and only inside the game's own image.
std::vector<void**> FindSlots(uintptr_t image, const void* target) {
	std::vector<void**> slots;
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return slots;
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + static_cast<uintptr_t>(dos->e_lfanew));
	if (nt->Signature != IMAGE_NT_SIGNATURE) return slots;
	const uintptr_t end = image + nt->OptionalHeader.SizeOfImage;
	constexpr DWORD Data = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY;
	for (uintptr_t at = image; at < end;) {
		MEMORY_BASIC_INFORMATION region {};
		if (VirtualQuery(reinterpret_cast<const void*>(at), &region, sizeof(region)) == 0) break;
		const uintptr_t regionEnd = std::min(end, reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize);
		if (region.State == MEM_COMMIT && (region.Protect & Data) != 0 && (region.Protect & PAGE_GUARD) == 0) {
			for (uintptr_t slot = (at + 7) & ~uintptr_t { 7 }; slot + sizeof(void*) <= regionEnd; slot += sizeof(void*)) {
				if (*reinterpret_cast<void* const*>(slot) == target) slots.push_back(reinterpret_cast<void**>(slot));
			}
		}
		at = regionEnd;
	}
	return slots;
}

bool Write(void** slot, void* value) {
	DWORD previous = 0;
	if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &previous)) return false;
	InterlockedExchangePointer(slot, value);
	DWORD ignored = 0;
	(void)VirtualProtect(slot, sizeof(void*), previous, &ignored);
	return true;
}

bool TryInstall(const D2RL::PluginContext* context, bool last) {
	for (const wchar_t* name : Modules) {
		// Never LoadLibrary: a module the mod pulled in is not one the game calls.
		const HMODULE module = GetModuleHandleW(name);
		const FARPROC target = module != nullptr ? GetProcAddress(module, "XInputGetState") : nullptr;
		if (target == nullptr) continue;
		auto slots = FindSlots(context->exeBase, reinterpret_cast<const void*>(target));
		if (slots.empty()) continue;
		g_original = reinterpret_cast<GetStateFn>(target);
		for (void** slot : slots) {
			if (Write(slot, reinterpret_cast<void*>(&Filtered))) g_slots.push_back(slot);
		}
		if (g_slots.empty()) {
			AsyncLog::WarnF(context, "[pad] could not write the game's XInputGetState slot (error %lu); R3 still reaches the game.", GetLastError());
			g_original = nullptr;
			g_retries = 0;
			return false;
		}
		AsyncLog::InfoF(context, "[pad] R3 filter on %zu game slot(s) for %ls XInputGetState; everything else passes through.",
			g_slots.size(), name);
		g_retries = 0;
		return true;
	}
	if (last) AsyncLog::Warn(context, "[pad] no game slot holds XInputGetState; R3 still reaches the game.");
	return false;
}

}

uint16_t Filter(uint16_t buttons, bool claimed, Press& press) {
	const bool down = (buttons & RightThumb) != 0;
	if (down && !press.down) press.kept = claimed;
	press.down = down;
	return down && press.kept ? static_cast<uint16_t>(buttons & ~RightThumb) : buttons;
}

bool Install(const D2RL::PluginContext* context) {
	if (!g_slots.empty()) return true;
	if (context == nullptr || context->exeBase == 0) return false;
	g_retries = Retries;
	g_nextTry = GetTickCount64() + RetryMs;
	return TryInstall(context, false);
}

void Remove() {
	for (void** slot : g_slots) (void)Write(slot, reinterpret_cast<void*>(g_original));
	g_slots.clear();
	g_retries = 0;
}

void ClaimRightThumb(const D2RL::PluginContext* context, bool claimed) {
	g_claimed.store(claimed, std::memory_order_relaxed);
	if (g_retries > 0 && context != nullptr && GetTickCount64() >= g_nextTry) {
		g_nextTry = GetTickCount64() + RetryMs;
		(void)TryInstall(context, --g_retries == 0);
	}
	if (g_keptPress.exchange(false, std::memory_order_relaxed)) {
		AsyncLog::Info(context, "[pad] R3 kept from the game: it is guidance's during play");
	}
}

}
