#include "input_direction.h"

#include <windows.h>

#include <xinput.h>

#include <limits>

namespace D2RAccess::InputDirection {

namespace {

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

// The stick axes are signed 16-bit values (SHORT), per the XInput API
// contract; the threshold below is derived from that type, not guessed.
constexpr SHORT kThumbstickMaxMagnitude = std::numeric_limits<SHORT>::max();

// Intentionally much larger than the small anti-drift deadzone XInput itself
// recommends (XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE): this one has to signal a
// deliberate directional push, not just that the stick is off-center.
constexpr float kThumbstickDirectionDeadzoneFraction = 0.5f;
constexpr SHORT kThumbstickDirectionThreshold         = static_cast<SHORT>(kThumbstickMaxMagnitude * kThumbstickDirectionDeadzoneFraction);

// Per GetAsyncKeyState's documented contract: the high bit of the return
// value is set when the key is currently down.
constexpr SHORT kKeyDownFlag = static_cast<SHORT>(0x8000);

HMODULE           g_library             = nullptr;
XInputGetStateFn  g_xInputGetState      = nullptr;
bool              g_libraryLoadAttempted = false;

struct PadEdgeState {
	bool dpadLeftHeld   = false;
	bool dpadRightHeld  = false;
	bool stickLeftHeld  = false;
	bool stickRightHeld = false;
	bool dpadUpHeld     = false;
	bool dpadDownHeld   = false;
	bool stickUpHeld    = false;
	bool stickDownHeld  = false;
};

PadEdgeState g_padState[XUSER_MAX_COUNT] {};
bool         g_keyLeftHeld  = false;
bool         g_keyRightHeld = false;
bool         g_keyUpHeld    = false;
bool         g_keyDownHeld  = false;

Horizontal g_lastHorizontal       = Horizontal::None;
ULONGLONG  g_lastHorizontalTickMs = 0;
Vertical   g_lastVertical         = Vertical::None;
ULONGLONG  g_lastVerticalTickMs   = 0;

void RecordEdge(Horizontal direction) {
	g_lastHorizontal       = direction;
	g_lastHorizontalTickMs = GetTickCount64();
}

void RecordEdge(Vertical direction) {
	g_lastVertical       = direction;
	g_lastVerticalTickMs = GetTickCount64();
}

// Updates `held` to `currentlyHeld` and returns whether this call is a rising
// edge (was not held, now is), so holding a direction only fires once.
bool UpdateEdge(bool& held, bool currentlyHeld) {
	const bool edge = currentlyHeld && !held;
	held            = currentlyHeld;
	return edge;
}

// Dynamic-loads XInput: xinput1_4.dll (Windows 8+) first, falling back to the
// compatibility DLL that ships on older systems. Never link xinput.lib
// statically, since not every Windows install has the same version.
void EnsureLibraryLoaded() {
	if (g_libraryLoadAttempted) {
		return;
	}
	g_libraryLoadAttempted = true;

	static constexpr const wchar_t* kCandidates[] = { L"xinput1_4.dll", L"xinput9_1_0.dll" };
	for (const wchar_t* name : kCandidates) {
		g_library = LoadLibraryW(name);
		if (g_library == nullptr) {
			continue;
		}
		g_xInputGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(g_library, "XInputGetState"));
		if (g_xInputGetState != nullptr) {
			return;
		}
		FreeLibrary(g_library);
		g_library = nullptr;
	}
}

}

void Poll() {
	EnsureLibraryLoaded();

	if (g_xInputGetState != nullptr) {
		for (DWORD padIndex = 0; padIndex < XUSER_MAX_COUNT; ++padIndex) {
			PadEdgeState& edges = g_padState[padIndex];
			XINPUT_STATE  state {};
			if (g_xInputGetState(padIndex, &state) != ERROR_SUCCESS) {
				// Not connected: clear held flags so a later reconnect starts
				// from a clean edge instead of replaying a stale press.
				edges = {};
				continue;
			}

			const XINPUT_GAMEPAD& pad = state.Gamepad;
			if (UpdateEdge(edges.dpadLeftHeld, (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0)) {
				RecordEdge(Horizontal::Left);
			}
			if (UpdateEdge(edges.dpadRightHeld, (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0)) {
				RecordEdge(Horizontal::Right);
			}
			if (UpdateEdge(edges.stickLeftHeld, pad.sThumbLX <= -kThumbstickDirectionThreshold)) {
				RecordEdge(Horizontal::Left);
			}
			if (UpdateEdge(edges.stickRightHeld, pad.sThumbLX >= kThumbstickDirectionThreshold)) {
				RecordEdge(Horizontal::Right);
			}
			// XInput's Y axis grows upward.
			if (UpdateEdge(edges.dpadUpHeld, (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0)) {
				RecordEdge(Vertical::Up);
			}
			if (UpdateEdge(edges.dpadDownHeld, (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0)) {
				RecordEdge(Vertical::Down);
			}
			if (UpdateEdge(edges.stickUpHeld, pad.sThumbLY >= kThumbstickDirectionThreshold)) {
				RecordEdge(Vertical::Up);
			}
			if (UpdateEdge(edges.stickDownHeld, pad.sThumbLY <= -kThumbstickDirectionThreshold)) {
				RecordEdge(Vertical::Down);
			}
		}
	}

	const bool keyLeft  = (GetAsyncKeyState(VK_LEFT) & kKeyDownFlag) != 0;
	const bool keyRight = (GetAsyncKeyState(VK_RIGHT) & kKeyDownFlag) != 0;
	const bool keyUp    = (GetAsyncKeyState(VK_UP) & kKeyDownFlag) != 0;
	const bool keyDown  = (GetAsyncKeyState(VK_DOWN) & kKeyDownFlag) != 0;
	if (UpdateEdge(g_keyLeftHeld, keyLeft)) {
		RecordEdge(Horizontal::Left);
	}
	if (UpdateEdge(g_keyRightHeld, keyRight)) {
		RecordEdge(Horizontal::Right);
	}
	if (UpdateEdge(g_keyUpHeld, keyUp)) {
		RecordEdge(Vertical::Up);
	}
	if (UpdateEdge(g_keyDownHeld, keyDown)) {
		RecordEdge(Vertical::Down);
	}
}

// Direction currently held by any source (None when nothing is held): key
// repeat keeps firing value messages long after the press edge.
Horizontal HeldHorizontal() {
	bool left = g_keyLeftHeld, right = g_keyRightHeld;
	for (const PadEdgeState& edges : g_padState) {
		left  = left || edges.dpadLeftHeld || edges.stickLeftHeld;
		right = right || edges.dpadRightHeld || edges.stickRightHeld;
	}
	if (left == right) {
		return Horizontal::None;
	}
	return left ? Horizontal::Left : Horizontal::Right;
}

Vertical HeldVertical() {
	bool up = g_keyUpHeld, down = g_keyDownHeld;
	for (const PadEdgeState& edges : g_padState) {
		up   = up || edges.dpadUpHeld || edges.stickUpHeld;
		down = down || edges.dpadDownHeld || edges.stickDownHeld;
	}
	if (up == down) {
		return Vertical::None;
	}
	return up ? Vertical::Up : Vertical::Down;
}

bool WithinAge(ULONGLONG tickMs, unsigned maxAgeMs) {
	return GetTickCount64() - tickMs <= maxAgeMs;
}

Horizontal LastHorizontal(unsigned maxAgeMs) {
	if (const Horizontal held = HeldHorizontal(); held != Horizontal::None) {
		return held;
	}
	return g_lastHorizontal != Horizontal::None && WithinAge(g_lastHorizontalTickMs, maxAgeMs) ? g_lastHorizontal : Horizontal::None;
}

Vertical LastVertical(unsigned maxAgeMs) {
	if (const Vertical held = HeldVertical(); held != Vertical::None) {
		return held;
	}
	return g_lastVertical != Vertical::None && WithinAge(g_lastVerticalTickMs, maxAgeMs) ? g_lastVertical : Vertical::None;
}

void Shutdown() {
	if (g_library != nullptr) {
		FreeLibrary(g_library);
		g_library = nullptr;
	}
	g_xInputGetState       = nullptr;
	g_libraryLoadAttempted = false; // allow reloading if Poll runs again

	for (PadEdgeState& edges : g_padState) {
		edges = {};
	}
	g_keyLeftHeld  = false;
	g_keyRightHeld = false;
	g_keyUpHeld    = false;
	g_keyDownHeld  = false;

	g_lastHorizontal       = Horizontal::None;
	g_lastHorizontalTickMs = 0;
	g_lastVertical         = Vertical::None;
	g_lastVerticalTickMs   = 0;
}

}
