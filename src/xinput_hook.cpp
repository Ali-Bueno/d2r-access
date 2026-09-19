#include "xinput_hook.h"

#include "async_log.h"
#include "keyboard_pad.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>

#include <xinput.h>

#include <array>
#include <atomic>
#include <cstring>
#include <string>

namespace D2RAccess::XInputHook {

namespace {

using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

// The game's own module, established live before the mod touched XInput:
//   [input] input modules at plugin load: xinput9_1_0
//   [input] xinput9_1_0 exports XInputGetState=yes, ordinal 100=no
// It does NOT export the undocumented ordinal 100, so the named function is
// every call the game makes. The mod's own reader loads xinput1_4.dll, a
// different module: patching this one cannot feed the mod its own input.
constexpr const wchar_t* kGameModules[] = {L"xinput9_1_0.dll", L"xinput1_4.dll", L"xinput1_3.dll"};

// XInputGetState's first five instructions on the build this was written for.
// All five are position-independent -- no rip-relative operand, no relative
// branch -- which is what makes copying them into a trampoline safe. The bytes
// are verified before anything is written, so a different build is refused
// rather than corrupted.
//   48 89 5C 24 08   mov  qword ptr [rsp+8], rbx
//   57               push rdi
//   48 83 EC 20      sub  rsp, 0x20
//   48 8B DA         mov  rbx, rdx
//   8B F9            mov  edi, ecx
constexpr uint8_t kPrologue[] = {
	0x48, 0x89, 0x5C, 0x24, 0x08,
	0x57,
	0x48, 0x83, 0xEC, 0x20,
	0x48, 0x8B, 0xDA,
	0x8B, 0xF9,
};
constexpr size_t kStolen = sizeof(kPrologue); // 15

// `jmp qword ptr [rip+0]` followed by the 8-byte destination: absolute, so the
// trampoline can live anywhere, and only 14 bytes -- inside the 15 we relocate.
constexpr uint8_t kAbsoluteJump[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
constexpr size_t  kJumpSize       = sizeof(kAbsoluteJump) + sizeof(void*); // 14

std::array<std::atomic<uint64_t>, XUSER_MAX_COUNT> g_calls {};
std::array<std::atomic<uint64_t>, XUSER_MAX_COUNT> g_connected {};
std::atomic<uint64_t>                              g_otherSlot {}; // a slot index outside 0..3

// Which slot the synthetic pad answers on. Critical rule 3: never merged
// with a physical controller and never reported in place of one, so it takes
// a slot no real device occupies. Measured live, D2R polls all four -- slot 0
// every frame and the empty ones about every five seconds -- so a pad that
// appears on a free slot is found.
constexpr DWORD NoSlot = 0xFFFFFFFF;
// How often the choice is re-checked against the hardware. The game's own
// hotplug rescan is about five seconds; this is well inside it, and each check
// is four calls to the original, not one per frame.
constexpr uint64_t SlotRecheckMs = 1000;
std::atomic<DWORD> g_slot { NoSlot };
uint64_t g_slotCheckedAt = 0;
std::atomic<bool>  g_inject { false };
std::atomic<DWORD> g_packet { 1 };
XINPUT_GAMEPAD     g_lastPad {};

// The layout, as one table so the keys and what they stand for cannot drift
// apart. Controller mode disables D2R's own keyboard scheme the moment a pad
// reports input, so none of these contend with a game binding.
struct KeyBinding { int vk; bool KeyboardPad::Keys::* field; };
constexpr KeyBinding kLayout[] = {
	{'J', &KeyboardPad::Keys::a},          {'N', &KeyboardPad::Keys::b},
	{'U', &KeyboardPad::Keys::x},          {'I', &KeyboardPad::Keys::y},
	{'Y', &KeyboardPad::Keys::leftShoulder}, {'B', &KeyboardPad::Keys::rightShoulder},
	{'T', &KeyboardPad::Keys::leftTrigger},  {'V', &KeyboardPad::Keys::rightTrigger},
	{'W', &KeyboardPad::Keys::moveUp},     {'A', &KeyboardPad::Keys::moveLeft},
	{'S', &KeyboardPad::Keys::moveDown},   {'D', &KeyboardPad::Keys::moveRight},
	{'O', &KeyboardPad::Keys::lookUp},     {'K', &KeyboardPad::Keys::lookLeft},
	{'L', &KeyboardPad::Keys::lookDown},   {VK_OEM_1, &KeyboardPad::Keys::lookRight},   // ;
	{VK_OEM_7, &KeyboardPad::Keys::leftThumb},  // '  L3, long-range pathfinder
	{VK_OEM_2, &KeyboardPad::Keys::rightThumb}, // /  R3, guide to target
	{VK_OEM_PLUS, &KeyboardPad::Keys::start},   // =  Start
	{VK_OEM_MINUS, &KeyboardPad::Keys::back},   // -  Back
	{VK_UP, &KeyboardPad::Keys::dpadUp},   {VK_DOWN, &KeyboardPad::Keys::dpadDown},
	{VK_LEFT, &KeyboardPad::Keys::dpadLeft}, {VK_RIGHT, &KeyboardPad::Keys::dpadRight},
};

// Ctrl+Shift+F12 turns the simulated pad on and off mid-session. It is read
// raw, exactly like the mapping below, because the moment the pad reports
// anything the game enters controller mode and its own input-action system
// stops delivering keys -- which is the same reason the developer console's
// hotkey stops working. A key we read ourselves is the only kind that still
// arrives once that has happened.
constexpr int ToggleKey = VK_F12;
bool g_toggleHeld = false;

// Only while the game has focus: these keys are read raw, so without this the
// player would be driving the character while typing in another window.
bool GameHasFocus() {
	DWORD owner = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &owner);
	return owner == GetCurrentProcessId();
}

KeyboardPad::Keys SampleKeyboard() {
	KeyboardPad::Keys keys;
	if (!GameHasFocus()) return keys;
	for (const KeyBinding& binding : kLayout) {
		if ((GetAsyncKeyState(binding.vk) & 0x8000) != 0) keys.*(binding.field) = true;
	}
	return keys;
}

uint8_t*    g_target     = nullptr;          // the patched function
uint8_t*    g_trampoline = nullptr;          // stolen bytes + jump back
GetStateFn  g_original   = nullptr;
uint8_t     g_saved[kStolen] {};
bool        g_installed  = false;
unsigned long long g_installedAt = 0;
unsigned    g_reports    = 0;

void WriteJump(uint8_t* at, const void* destination) {
	std::memcpy(at, kAbsoluteJump, sizeof(kAbsoluteJump));
	std::memcpy(at + sizeof(kAbsoluteJump), &destination, sizeof(destination));
}

DWORD WINAPI Detour(DWORD slot, XINPUT_STATE* state) {
	if (slot < XUSER_MAX_COUNT) {
		g_calls[slot].fetch_add(1, std::memory_order_relaxed);
	} else {
		g_otherSlot.fetch_add(1, std::memory_order_relaxed);
	}
	// Our slot: answer it whole. Never call the original for it -- the whole
	// point is that no physical device is there. Every other slot is passed
	// through untouched, so a real controller is neither merged with nor
	// spoken over (critical rule 3).
	const DWORD ours = g_slot.load(std::memory_order_relaxed);
	if (g_inject.load(std::memory_order_relaxed) && ours != NoSlot && slot == ours && state != nullptr) {
		const KeyboardPad::Pad pad = KeyboardPad::Build(SampleKeyboard());
		XINPUT_GAMEPAD gamepad {};
		gamepad.wButtons      = pad.buttons;
		gamepad.bLeftTrigger  = pad.leftTrigger;
		gamepad.bRightTrigger = pad.rightTrigger;
		gamepad.sThumbLX      = pad.leftThumbX;
		gamepad.sThumbLY      = pad.leftThumbY;
		gamepad.sThumbRX      = pad.rightThumbX;
		gamepad.sThumbRY      = pad.rightThumbY;
		// dwPacketNumber only moves when the state does: that is how a game
		// tells "nothing happened" from "held since last frame".
		if (std::memcmp(&gamepad, &g_lastPad, sizeof(gamepad)) != 0) {
			g_lastPad = gamepad;
			g_packet.fetch_add(1, std::memory_order_relaxed);
		}
		state->dwPacketNumber = g_packet.load(std::memory_order_relaxed);
		state->Gamepad        = gamepad;
		if (slot < XUSER_MAX_COUNT) g_connected[slot].fetch_add(1, std::memory_order_relaxed);
		return ERROR_SUCCESS;
	}

	const DWORD result = g_original != nullptr ? g_original(slot, state) : ERROR_DEVICE_NOT_CONNECTED;
	if (result == ERROR_SUCCESS && slot < XUSER_MAX_COUNT) {
		g_connected[slot].fetch_add(1, std::memory_order_relaxed);
	}
	return result;
}

// Resolves the module the game already has loaded. GetModuleHandleW never
// loads anything, so this cannot pull in a module the game does not use.
HMODULE GameModule(const wchar_t*& chosen) {
	for (const wchar_t* name : kGameModules) {
		if (const HMODULE module = GetModuleHandleW(name); module != nullptr) {
			chosen = name;
			return module;
		}
	}
	return nullptr;
}

// The lowest slot no physical controller occupies, asked of the ORIGINAL.
//
// The original is the trampoline to the real XInputGetState, so it never sees
// our injection -- this is always the true hardware picture, and it is safe to
// ask it again at any time. An earlier version latched the answer at install
// time for fear of seeing our own pad; that fear was unfounded and the latch
// was a bug. Install runs before the game has finished enumerating
// controllers, so the choice raced the enumeration: lose that race and we take
// slot 0 and answer for the slot the player's own controller is on, which is
// the one thing critical rule 3 forbids outright.
DWORD ChooseFreeSlot() {
	if (g_original == nullptr) return NoSlot;
	for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
		XINPUT_STATE state {};
		if (g_original(slot, &state) != ERROR_SUCCESS) return slot;
	}
	return NoSlot; // four real controllers: leave every one of them alone
}

// Re-checks the choice against the hardware. A controller plugged in later, or
// enumerated after we installed, must get its slot back immediately even if we
// are already answering on it.
void VerifySlot(const D2RL::PluginContext* context) {
	if (g_original == nullptr) return;
	const uint64_t now = GetTickCount64();
	if (g_slotCheckedAt != 0 && now - g_slotCheckedAt < SlotRecheckMs) return;
	g_slotCheckedAt = now;
	const DWORD current = g_slot.load(std::memory_order_relaxed);
	const DWORD chosen = ChooseFreeSlot();
	if (chosen == current) return;
	g_slot.store(chosen, std::memory_order_relaxed);
	// A fresh slot starts from a clean packet, or the game would read our
	// previous state as "held since last frame" on a device it has just met.
	g_lastPad = {};
	g_packet.fetch_add(1, std::memory_order_relaxed);
	if (chosen == NoSlot) {
		AsyncLog::WarnF(context, "[pad] slot %lu now holds a real controller and no slot is free; keyboard input is not injected.",
			static_cast<unsigned long>(current));
	} else {
		AsyncLog::InfoF(context, "[pad] moved the keyboard pad from slot %lu to slot %lu (a real controller appeared).",
			static_cast<unsigned long>(current), static_cast<unsigned long>(chosen));
	}
}

}

bool Install(const D2RL::PluginContext* context) {
	if (g_installed) return true;

	const wchar_t* moduleName = nullptr;
	const HMODULE  module     = GameModule(moduleName);
	if (module == nullptr) {
		AsyncLog::Warn(context, "[pad] no XInput module is loaded; nothing to measure.");
		return false;
	}
	auto* target = reinterpret_cast<uint8_t*>(GetProcAddress(module, "XInputGetState"));
	if (target == nullptr) {
		AsyncLog::Warn(context, "[pad] XInputGetState not exported; not hooking.");
		return false;
	}
	if (std::memcmp(target, kPrologue, kStolen) != 0) {
		// Either a different build or something else got there first. Either
		// way the stolen bytes would not be the ones this was reasoned about.
		std::string seen;
		for (size_t at = 0; at < kStolen; ++at) {
			char byte[4] {};
			std::snprintf(byte, sizeof(byte), "%02X", target[at]);
			seen += byte;
		}
		AsyncLog::WarnF(context, "[pad] XInputGetState prologue is not the one this hook was written for (%s); not hooking.", seen.c_str());
		return false;
	}

	g_trampoline = static_cast<uint8_t*>(VirtualAlloc(nullptr, kStolen + kJumpSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (g_trampoline == nullptr) {
		AsyncLog::Warn(context, "[pad] could not allocate a trampoline; not hooking.");
		return false;
	}
	std::memcpy(g_trampoline, target, kStolen);
	WriteJump(g_trampoline + kStolen, target + kStolen);

	DWORD previous = 0;
	if (VirtualProtect(target, kJumpSize, PAGE_EXECUTE_READWRITE, &previous) == 0) {
		VirtualFree(g_trampoline, 0, MEM_RELEASE);
		g_trampoline = nullptr;
		AsyncLog::Warn(context, "[pad] could not unprotect XInputGetState; not hooking.");
		return false;
	}
	std::memcpy(g_saved, target, kStolen);
	WriteJump(target, reinterpret_cast<const void*>(&Detour));
	// The 15th byte is left as it was: the jump is 14, and the instruction it
	// belonged to has been relocated whole into the trampoline.
	VirtualProtect(target, kJumpSize, previous, &previous);
	FlushInstructionCache(GetCurrentProcess(), target, kJumpSize);

	g_target      = target;
	g_original    = reinterpret_cast<GetStateFn>(g_trampoline);
	g_installed   = true;
	g_installedAt = GetTickCount64();
	// A first guess only. Install runs before the game has finished
	// enumerating controllers, so this is re-checked on every tick until the
	// hardware settles -- see VerifySlot.
	g_slot.store(ChooseFreeSlot());
	g_slotCheckedAt = 0;
	const DWORD chosen = g_slot.load();
	if (chosen == NoSlot) {
		AsyncLog::InfoF(context, "[pad] hooked %ls; every slot holds a real controller, so the keyboard pad has nowhere to go.",
			moduleName != nullptr ? moduleName : L"?");
	} else {
		AsyncLog::InfoF(context, "[pad] hooked %ls; keyboard pad starts on slot %lu, re-checked as controllers enumerate.",
			moduleName != nullptr ? moduleName : L"?", static_cast<unsigned long>(chosen));
	}
	return true;
}

void Remove(const D2RL::PluginContext* context) {
	if (!g_installed || g_target == nullptr) return;
	DWORD previous = 0;
	if (VirtualProtect(g_target, kJumpSize, PAGE_EXECUTE_READWRITE, &previous) != 0) {
		std::memcpy(g_target, g_saved, kStolen);
		VirtualProtect(g_target, kJumpSize, previous, &previous);
		FlushInstructionCache(GetCurrentProcess(), g_target, kJumpSize);
	}
	g_installed = false;
	g_original  = nullptr;
	g_target    = nullptr;
	AsyncLog::Info(context, "[pad] XInputGetState restored.");
}

void Report(const D2RL::PluginContext* context) {
	if (context == nullptr) return;
	std::string line;
	for (unsigned slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
		char text[64] {};
		std::snprintf(text, sizeof(text), "%s%u: %llu calls/%llu connected", slot == 0 ? "" : ", ", slot,
			static_cast<unsigned long long>(g_calls[slot].load(std::memory_order_relaxed)),
			static_cast<unsigned long long>(g_connected[slot].load(std::memory_order_relaxed)));
		line += text;
	}
	const unsigned long long other = g_otherSlot.load(std::memory_order_relaxed);
	AsyncLog::InfoF(context, "[pad] polls by slot -- %s%s", line.c_str(),
		other != 0 ? " (plus calls with a slot index outside 0-3)" : "");
}

// The live toggle, edge-detected so holding the combination flips it once.
void PollToggle(const D2RL::PluginContext* context) {
	const bool held = GameHasFocus()
		&& (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0
		&& (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0
		&& (GetAsyncKeyState(ToggleKey) & 0x8000) != 0;
	if (held && !g_toggleHeld) {
		const bool wanted = !g_inject.load(std::memory_order_relaxed);
		SetInjection(context, wanted);
		// Spoken, not just logged: the player who needs this cannot see
		// whether the press took, and the pad reports nothing until a key is
		// held, so there is no other feedback that it worked.
		const auto& strings = Strings::Current();
		ScreenReader::Speak(Injecting() ? strings.keyboardPadEnabled : strings.keyboardPadDisabled, true);
	}
	g_toggleHeld = held;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_installed) return;
	PollToggle(context);
	// Before anything else: a controller that appeared after we chose must get
	// its slot back, whether or not the startup reports are still running.
	VerifySlot(context);
	if (g_reports >= 3) return;
	// Three reports over the first half minute: the game may poll every slot
	// once at startup and then settle on one, and a single sample would not
	// tell those two apart.
	static constexpr unsigned long long kAt[] = {5000, 15000, 30000};
	const unsigned long long elapsed = GetTickCount64() - g_installedAt;
	if (elapsed < kAt[g_reports]) return;
	++g_reports;
	Report(context);
}

void SetInjection(const D2RL::PluginContext* context, bool enabled) {
	if (enabled && g_slot.load() == NoSlot) {
		AsyncLog::Warn(context, "[pad] no free controller slot; keyboard input not injected.");
		return;
	}
	g_inject.store(enabled);
	AsyncLog::InfoF(context, "[pad] keyboard pad %s on slot %lu.", enabled ? "ON" : "off",
		static_cast<unsigned long>(g_slot.load()));
}

bool Injecting() { return g_inject.load(); }

Synthetic SyntheticPad() {
	Synthetic result;
	const DWORD slot = g_slot.load(std::memory_order_relaxed);
	if (!g_inject.load(std::memory_order_relaxed) || slot == NoSlot) return result;
	result.active = true;
	result.slot   = static_cast<unsigned>(slot);
	// Sampled here rather than cached from the detour: the game's poll rate is
	// its own business, and a reader on the UI tick must not inherit it.
	result.pad = KeyboardPad::Build(SampleKeyboard());
	return result;
}

SlotStats Stats(unsigned slot) {
	if (slot >= XUSER_MAX_COUNT) return {};
	return {g_calls[slot].load(std::memory_order_relaxed), g_connected[slot].load(std::memory_order_relaxed)};
}

}
