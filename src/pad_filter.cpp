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

constexpr uint16_t Back = XINPUT_GAMEPAD_BACK;

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
std::atomic<uint64_t>              g_holdBUntil { 0 };    // GetTickCount64 before which B is held
std::atomic<uint64_t>              g_leftStickUntil { 0 }; // before which a held Y takes the left stick
std::atomic<uint16_t>              g_keptPresses { 0 }; // buttons kept, for the UI tick's log line
std::atomic<bool>                  g_keepBack { true };
std::atomic<bool>                  g_settingsOpen { false };
int                                g_retries = 0;
uint64_t                           g_nextTry = 0;
// Touched only on the thread the game polls from: D2R reads its pads from one
// call site (MEASURED offline 2026-09-20, build 3.3.93787).
std::array<Presses, XUSER_MAX_COUNT> g_press {};
std::array<uint16_t, XUSER_MAX_COUNT> g_keptLast {}; // what the previous frame kept, so each press is logged once
// The first presses the game itself polls outside play, for the UI tick's log.
constexpr uint32_t SeenLogMax = 12;
constexpr uint32_t SeenSlotShift = 16;
constexpr uint32_t SeenSet = 1u << 24; // keeps a pending value non-zero for slot 0
std::array<WORD, XUSER_MAX_COUNT> g_seenLast {};
std::atomic<uint32_t>             g_seenCount { 0 };
std::atomic<uint32_t>             g_seenPending { 0 };

DWORD WINAPI Filtered(DWORD user, XINPUT_STATE* state) {
	const DWORD result = g_original(user, state);
	if (result != ERROR_SUCCESS || state == nullptr || user >= XUSER_MAX_COUNT) return result;
	const WORD before = state->Gamepad.wButtons;
	if (before != 0 && g_seenLast[user] == 0 && !g_claimed.load(std::memory_order_relaxed)
		&& g_seenCount.load(std::memory_order_relaxed) < SeenLogMax)
		g_seenPending.store((user << SeenSlotShift) | before | SeenSet, std::memory_order_relaxed);
	g_seenLast[user] = before;
	const bool holdB = GetTickCount64() < g_holdBUntil.load(std::memory_order_relaxed);
	state->Gamepad.wButtons = Filter(before, g_claimed.load(std::memory_order_relaxed), g_press[user], holdB,
		g_keepBack.load(std::memory_order_relaxed), g_settingsOpen.load(std::memory_order_relaxed));
	if (KeepsLeftStick(before, GetTickCount64() < g_leftStickUntil.load(std::memory_order_relaxed))) {
		state->Gamepad.sThumbLX = 0;
		state->Gamepad.sThumbLY = 0;
	}
	const bool back = (before & Back) != 0, claimed = g_claimed.load(std::memory_order_relaxed);
	if (KeepsTrigger(state->Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD, back, claimed, g_press[user].lt))
		state->Gamepad.bLeftTrigger = 0;
	if (KeepsTrigger(state->Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD, back, claimed, g_press[user].rt))
		state->Gamepad.bRightTrigger = 0;
	const uint16_t kept = static_cast<uint16_t>(before & ~state->Gamepad.wButtons);
	if (const uint16_t fresh = static_cast<uint16_t>(kept & ~g_keptLast[user]))
		g_keptPresses.fetch_or(fresh, std::memory_order_relaxed);
	g_keptLast[user] = kept;
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
			AsyncLog::WarnF(context, "[pad] could not write the game's XInputGetState slot (error %lu); the face buttons with BACK still reach the game.", GetLastError());
			g_original = nullptr;
			g_retries = 0;
			return false;
		}
		AsyncLog::InfoF(context, "[pad] filter on %zu game slot(s) for %ls XInputGetState: the face buttons with BACK kept during play; everything else passes through.",
			g_slots.size(), name);
		g_retries = 0;
		return true;
	}
	if (last) AsyncLog::Warn(context, "[pad] no game slot holds XInputGetState; the face buttons with BACK still reach the game.");
	return false;
}

}

namespace {
uint16_t Keep(uint16_t buttons, uint16_t button, bool claimed, Press& press) {
	const bool down = (buttons & button) != 0;
	if (down && !press.down) press.kept = claimed;
	press.down = down;
	return down && press.kept ? static_cast<uint16_t>(buttons & ~button) : buttons;
}
}

uint16_t Filter(uint16_t buttons, bool claimed, Presses& presses, bool holdB, bool keepBack, bool shoulders) {
	const bool back = (buttons & Back) != 0;
	buttons = Keep(buttons, Back, claimed && keepBack, presses.back);
	buttons = Keep(buttons, XINPUT_GAMEPAD_A, claimed && back, presses.a);
	buttons = Keep(buttons, XINPUT_GAMEPAD_B, (claimed && back) || holdB, presses.b);
	buttons = Keep(buttons, XINPUT_GAMEPAD_X, claimed && back, presses.x);
	buttons = Keep(buttons, XINPUT_GAMEPAD_Y, claimed && back, presses.y);
	buttons = Keep(buttons, XINPUT_GAMEPAD_LEFT_SHOULDER, (claimed && back) || shoulders, presses.lb);
	buttons = Keep(buttons, XINPUT_GAMEPAD_RIGHT_SHOULDER, (claimed && back) || shoulders, presses.rb);
	return buttons;
}

void HoldB(uint64_t now) {
	g_holdBUntil.store(now + HoldBMs, std::memory_order_relaxed);
}

void ArmLeftStickWithY(uint64_t now) {
	g_leftStickUntil.store(now + HoldBMs, std::memory_order_relaxed);
}

bool KeepsLeftStick(uint16_t buttons, bool armed) {
	return armed && (buttons & XINPUT_GAMEPAD_Y) != 0;
}

bool KeepsTrigger(bool pulled, bool back, bool claimed, Press& press) {
	if (pulled && !press.down) press.kept = claimed && back;
	press.down = pulled;
	return pulled && press.kept;
}

void ReleaseB() {
	g_holdBUntil.store(0, std::memory_order_relaxed);
}

bool KeepBack() { return g_keepBack.load(std::memory_order_relaxed); }
void SetSettingsOpen(bool open) { g_settingsOpen.store(open, std::memory_order_relaxed); }
void SetKeepBack(bool keep) { g_keepBack.store(keep, std::memory_order_relaxed); }

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

void Claim(const D2RL::PluginContext* context, bool claimed) {
	g_claimed.store(claimed, std::memory_order_relaxed);
	if (g_retries > 0 && context != nullptr && GetTickCount64() >= g_nextTry) {
		g_nextTry = GetTickCount64() + RetryMs;
		(void)TryInstall(context, --g_retries == 0);
	}
	if (const uint32_t seen = g_seenPending.exchange(0, std::memory_order_relaxed)) {
		const uint32_t count = g_seenCount.fetch_add(1, std::memory_order_relaxed) + 1;
		AsyncLog::InfoF(context, "[pad] the game polled slot %u with buttons 0x%04x outside play (%u of %u logged)",
			(seen >> SeenSlotShift) & 0xFF, seen & 0xFFFF, count, SeenLogMax);
	}
	if (const uint16_t kept = g_keptPresses.exchange(0, std::memory_order_relaxed)) {
		AsyncLog::InfoF(context, "[pad] kept from the game:%s%s%s%s%s%s%s", (kept & Back) ? " BACK (the automap in play)" : "",
			(kept & XINPUT_GAMEPAD_A) ? " A (with BACK)" : "", (kept & XINPUT_GAMEPAD_B) ? " B (with BACK, or closing the line reader)" : "",
			(kept & XINPUT_GAMEPAD_X) ? " X (with BACK)" : "", (kept & XINPUT_GAMEPAD_Y) ? " Y (with BACK)" : "",
			(kept & XINPUT_GAMEPAD_LEFT_SHOULDER) ? " LB (with BACK)" : "", (kept & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? " RB (with BACK)" : "");
	}
}

}
