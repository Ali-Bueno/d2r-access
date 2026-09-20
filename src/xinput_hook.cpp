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

// Slot 0, always, for the whole session. Never chosen, never moved.
//
// This replaces picking the lowest FREE slot, which could not work. MEASURED
// 2026-09-20: with the keyboard pad answering on slot 1 the game polled it
// every frame -- 240 to 509 calls per fifteen-second window against slot 0's
// 1488 to 1644 -- and still drove the character from slot 0 alone. Every
// keyboard control the MOD consumes worked; every one that had to reach the
// GAME was dead. Reading a slot is not being driven by it.
//
// Free-slot selection also had no way back. A controller present at startup
// put us on slot 1, and rule 3 rightly forbids taking slot 0 from it later, so
// the choice was permanent -- and D2R does not re-enumerate slots on its own,
// so nothing we did afterwards could move us. Answering slot 0 from the very
// first poll means the slot is occupied for the whole session and its
// occupancy NEVER changes; only its contents do, and contents are re-read
// every frame. There is no connect or disconnect event for the game to miss.
//
// Critical rule 3 is still satisfied, by its own written fallback rather than
// by hiding on an empty slot: a physical controller's state is passed through
// untouched whenever it is being used, and the keyboard is reported only while
// that controller is at rest. Nothing is merged, and nothing a player is
// actually doing is ever discarded.
constexpr DWORD OurSlot = 0;

// Which device owns slot 0 right now. Last input wins: whichever was used most
// recently holds it, and a source that is currently non-neutral keeps it.
enum class Source { Keyboard, Controller };
std::atomic<Source> g_source { Source::Keyboard };

// XInput's own resting thresholds. A real stick never reads exactly zero, so
// "is the player using this controller" has to mean "outside the deadzone",
// not "not zero" -- otherwise a drifting stick would hold slot 0 forever and
// the keyboard would never get a turn.
bool InUse(const XINPUT_GAMEPAD& pad) noexcept {
	if (pad.wButtons != 0) return true;
	if (pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return true;
	if (pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return true;
	const auto outside = [](int16_t x, int16_t y, int dead) {
		const int dx = x, dy = y;
		return dx * dx + dy * dy > dead * dead;
	};
	if (outside(pad.sThumbLX, pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)) return true;
	if (outside(pad.sThumbRX, pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)) return true;
	return false;
}

std::atomic<bool>  g_inject { false };
std::atomic<DWORD> g_packet { 1 };
XINPUT_GAMEPAD     g_lastPad {};

// The last frame the game was actually handed, kept so the UI tick can log it.
// MEASURED offline (build 3.3.93787): D2R imports exactly XInputGetState and
// XInputSetState from XINPUT9_1_0.dll and calls each from ONE site --
// 0x141210ac2 and 0x14120ffde. The poll site does
//     lea rdx,[rdi+4] / mov ecx,esi / call XInputGetState
//     test eax,eax / sete al / mov [rdi],al
// so the game keeps a per-slot record whose first byte is "connected" and
// whose XINPUT_STATE sits at +4. That is as far as reading the game settles
// it: our frame reaches that record. Whether the rest of D2R acts on it is
// not readable from there, and until this log existed our own half was not
// established either -- nothing recorded what we were reporting when the
// player pressed W.
std::atomic<uint32_t> g_frameButtons { 0 };
std::atomic<int32_t>  g_frameLeftX { 0 }, g_frameLeftY { 0 };
std::atomic<int32_t>  g_frameRightX { 0 }, g_frameRightY { 0 };
std::atomic<uint32_t> g_framePacket { 0 };
std::atomic<uint64_t> g_frameSerial { 0 }; // bumped whenever the frame changes
// Which source the arbitration handed the game, so a log can tell a keyboard
// frame from a passed-through controller one without guessing.
std::atomic<bool>     g_frameFromPad { false };

// The layout, as one table so the keys and what they stand for cannot drift
// apart.
//
// Whether these contend with D2R's own bindings depends on whether the game is
// actually being DRIVEN by the pad, and both halves are now measured:
//   - On slot 1, where the game polled us but drove the character from slot 0,
//     W reached D2R's weapon-swap bind and moved the player's weapon and shield
//     onto weapon-swap set II. That read as the equipment vanishing and cost
//     three sessions.
//   - On slot 0, with the game genuinely in controller mode, W moves the
//     character and does NOT swap weapons (MEASURED 2026-09-20, user).
// So controller mode does suppress the game's own keyboard bindings, but only
// once a pad is really driving it. NOT established: that this covers menu
// navigation as well as gameplay binds -- the arrow keys still appear to reach
// the inventory, which is why the D-pad over-moves there.
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
	// Slot 0 is arbitrated; slots 1-3 are passed through untouched, so a second
	// real controller is never merged with, spoken over, or reported absent.
	if (g_inject.load(std::memory_order_relaxed) && slot == OurSlot && state != nullptr) {
		// Ask the hardware first. g_original is the trampoline to the real
		// XInputGetState, so this is the true picture and never sees our own
		// injection.
		XINPUT_STATE real {};
		const bool padPresent = g_original != nullptr && g_original(OurSlot, &real) == ERROR_SUCCESS;
		const KeyboardPad::Pad pad = KeyboardPad::Build(SampleKeyboard());
		const bool controllerActive = padPresent && InUse(real.Gamepad);
		const bool keyboardActive   = !KeyboardPad::Idle(pad);
		// Last input wins. With both at rest the previous owner keeps the slot,
		// which costs nothing: both would report a neutral frame anyway.
		if (controllerActive)    g_source.store(Source::Controller, std::memory_order_relaxed);
		else if (keyboardActive) g_source.store(Source::Keyboard, std::memory_order_relaxed);
		const bool controllerOwns = padPresent && g_source.load(std::memory_order_relaxed) == Source::Controller;

		XINPUT_GAMEPAD gamepad {};
		if (controllerOwns) {
			// The physical pad's own state, byte for byte. Not rebuilt, not
			// filtered, not combined with anything of ours.
			gamepad = real.Gamepad;
		} else {
			gamepad.wButtons      = pad.buttons;
			gamepad.bLeftTrigger  = pad.leftTrigger;
			gamepad.bRightTrigger = pad.rightTrigger;
			gamepad.sThumbLX      = pad.leftThumbX;
			gamepad.sThumbLY      = pad.leftThumbY;
			gamepad.sThumbRX      = pad.rightThumbX;
			gamepad.sThumbRY      = pad.rightThumbY;
		}
		// One monotonic packet number for the slot, bumped whenever what the
		// game is handed changes -- whichever source produced it. The hardware
		// has its own numbering and ours has another; handing the game first
		// one and then the other would make the count jump about, and
		// dwPacketNumber is exactly how a game tells "nothing happened" from
		// "held since last frame".
		if (std::memcmp(&gamepad, &g_lastPad, sizeof(gamepad)) != 0) {
			g_lastPad = gamepad;
			g_packet.fetch_add(1, std::memory_order_relaxed);
		}
		state->dwPacketNumber = g_packet.load(std::memory_order_relaxed);
		state->Gamepad        = gamepad;
		g_frameButtons.store(gamepad.wButtons, std::memory_order_relaxed);
		g_frameLeftX.store(gamepad.sThumbLX, std::memory_order_relaxed);
		g_frameLeftY.store(gamepad.sThumbLY, std::memory_order_relaxed);
		g_frameRightX.store(gamepad.sThumbRX, std::memory_order_relaxed);
		g_frameRightY.store(gamepad.sThumbRY, std::memory_order_relaxed);
		g_framePacket.store(state->dwPacketNumber, std::memory_order_relaxed);
		g_frameSerial.store(g_packet.load(std::memory_order_relaxed), std::memory_order_relaxed);
		g_frameFromPad.store(controllerOwns, std::memory_order_relaxed);
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
	// The source starts as the keyboard: with no controller connected that is
	// correct, and with one connected the first thing the player actually does
	// hands the slot over on that same frame.
	g_source.store(Source::Keyboard);
	AsyncLog::InfoF(context, "[pad] hooked %ls; the keyboard pad answers slot %lu for the session, handing it to a physical controller whenever one is in use.",
		moduleName != nullptr ? moduleName : L"?", static_cast<unsigned long>(OurSlot));
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

// One line per CHANGE of the injected frame, written on the UI tick and never
// from the detour, which runs on the game's poll. Silent while nothing is
// held, so it cannot flood: a frame only changes when a key goes down or up.
void LogFrame(const D2RL::PluginContext* context) {
	static uint64_t lastSerial = 0;
	if (!g_inject.load(std::memory_order_relaxed)) { lastSerial = 0; return; }
	const uint64_t serial = g_frameSerial.load(std::memory_order_relaxed);
	if (serial == lastSerial) return;
	lastSerial = serial;
	AsyncLog::InfoF(context, "[pad] frame slot=%lu src=%s packet=%u buttons=0x%04X left=%d,%d right=%d,%d",
		static_cast<unsigned long>(OurSlot),
		g_frameFromPad.load(std::memory_order_relaxed) ? "controller" : "keyboard",
		g_framePacket.load(std::memory_order_relaxed),
		g_frameButtons.load(std::memory_order_relaxed),
		g_frameLeftX.load(std::memory_order_relaxed), g_frameLeftY.load(std::memory_order_relaxed),
		g_frameRightX.load(std::memory_order_relaxed), g_frameRightY.load(std::memory_order_relaxed));
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_installed) return;
	PollToggle(context);
	LogFrame(context);
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
	g_inject.store(enabled);
	AsyncLog::InfoF(context, "[pad] keyboard pad %s on slot %lu.", enabled ? "ON" : "off",
		static_cast<unsigned long>(OurSlot));
	if (!enabled) return;
	// Turning it on restarts the poll census, so the counts that follow describe
	// the injecting period and nothing before it. This is the measurement that
	// says whether the game actually took the device: a slot it has accepted is
	// polled every frame, an empty one only on its ~5 s hotplug rescan. Do not
	// reason about whether D2R is reading our pad without these numbers.
	for (auto& count : g_calls) count.store(0, std::memory_order_relaxed);
	for (auto& count : g_connected) count.store(0, std::memory_order_relaxed);
	g_otherSlot.store(0, std::memory_order_relaxed);
	g_installedAt = GetTickCount64();
	g_reports = 0;
}

bool Injecting() { return g_inject.load(); }

Synthetic SyntheticPad() {
	Synthetic result;
	if (!g_inject.load(std::memory_order_relaxed)) return result;
	result.active = true;
	result.slot   = static_cast<unsigned>(OurSlot);
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
