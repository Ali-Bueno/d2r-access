#include "input_direction.h"

#include "xinput_hook.h"

#include <windows.h>

#include <xinput.h>

#include <cstdlib>
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
	bool aHeld          = false;
	bool bHeld          = false;
	bool xHeld          = false;
	bool yHeld          = false;
	bool rightStickHeld = false;
	bool leftThumbHeld  = false;
	bool rightThumbHeld = false;
};

PadEdgeState g_padState[XUSER_MAX_COUNT] {};
bool         g_keyLeftHeld  = false;
bool         g_keyRightHeld = false;
bool         g_keyUpHeld    = false;
bool         g_keyDownHeld  = false;
DpadInput g_dpad;
WORD g_previousDpad = 0;
Stick g_leftStick;
bool g_leftStickClick = false;
bool g_rightStickClick = false;

Horizontal g_lastHorizontal       = Horizontal::None;
ULONGLONG  g_lastHorizontalTickMs = 0;
Vertical   g_lastVertical         = Vertical::None;
ULONGLONG  g_lastVerticalTickMs   = 0;
FaceButton g_lastFaceButton       = FaceButton::None;
StickPush  g_rightStickPush       = StickPush::None;
ULONGLONG  g_lastFaceButtonTickMs = 0;

void RecordEdge(Horizontal direction) {
	g_lastHorizontal       = direction;
	g_lastHorizontalTickMs = GetTickCount64();
}

void RecordEdge(Vertical direction) {
	g_lastVertical       = direction;
	g_lastVerticalTickMs = GetTickCount64();
}

void RecordEdge(FaceButton button) {
	g_lastFaceButton       = button;
	g_lastFaceButtonTickMs = GetTickCount64();
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
	WORD dpadButtons = 0;
	constexpr WORD DpadMask = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN
		| XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;

	g_leftStick = {};
	// Keyboard play reports a pad of its own on a free slot, but into the
	// module the GAME reads -- not this one (xinput_hook.h). Taking it here is
	// what lets the scan, the pathfinder and guidance answer the keyboard at
	// all. It is a source beside the real pads, never mixed into one: it holds
	// a slot no physical controller occupies, so nothing is merged or masked.
	const XInputHook::Synthetic keyboard = XInputHook::SyntheticPad();
	if (g_xInputGetState != nullptr || keyboard.active) {
		for (DWORD padIndex = 0; padIndex < XUSER_MAX_COUNT; ++padIndex) {
			PadEdgeState& edges = g_padState[padIndex];
			XINPUT_STATE  state {};
			if (keyboard.active && padIndex == keyboard.slot) {
				// The synthetic pad IS the device on this slot, exactly as the
				// game sees it; asking the real XInput would report nothing.
				state.Gamepad.wButtons      = keyboard.pad.buttons;
				state.Gamepad.bLeftTrigger  = keyboard.pad.leftTrigger;
				state.Gamepad.bRightTrigger = keyboard.pad.rightTrigger;
				state.Gamepad.sThumbLX      = keyboard.pad.leftThumbX;
				state.Gamepad.sThumbLY      = keyboard.pad.leftThumbY;
				state.Gamepad.sThumbRX      = keyboard.pad.rightThumbX;
				state.Gamepad.sThumbRY      = keyboard.pad.rightThumbY;
			} else if (g_xInputGetState == nullptr || g_xInputGetState(padIndex, &state) != ERROR_SUCCESS) {
				// Not connected: clear held flags so a later reconnect starts
				// from a clean edge instead of replaying a stale press.
				edges = {};
				continue;
			}

			const XINPUT_GAMEPAD& pad = state.Gamepad;
			dpadButtons |= pad.wButtons & DpadMask;
			if (!g_leftStick.connected) {
				// XInput's Y axis grows upward; screen y grows downward.
				g_leftStick = {static_cast<float>(pad.sThumbLX) / kThumbstickMaxMagnitude,
					-static_cast<float>(pad.sThumbLY) / kThumbstickMaxMagnitude, true};
			}
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
			if (UpdateEdge(edges.aHeld, (pad.wButtons & XINPUT_GAMEPAD_A) != 0)) {
				RecordEdge(FaceButton::A);
			}
			if (UpdateEdge(edges.bHeld, (pad.wButtons & XINPUT_GAMEPAD_B) != 0)) {
				RecordEdge(FaceButton::B);
			}
			if (UpdateEdge(edges.xHeld, (pad.wButtons & XINPUT_GAMEPAD_X) != 0)) {
				RecordEdge(FaceButton::X);
			}
			if (UpdateEdge(edges.yHeld, (pad.wButtons & XINPUT_GAMEPAD_Y) != 0)) {
				RecordEdge(FaceButton::Y);
			}
			const bool rightPushed = pad.sThumbRX >= kThumbstickDirectionThreshold || pad.sThumbRX <= -kThumbstickDirectionThreshold
				|| pad.sThumbRY >= kThumbstickDirectionThreshold || pad.sThumbRY <= -kThumbstickDirectionThreshold;
			if (UpdateEdge(edges.leftThumbHeld, (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0)) {
				g_leftStickClick = true;
			}
			if (UpdateEdge(edges.rightThumbHeld, (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0)) {
				g_rightStickClick = true;
			}
			if (UpdateEdge(edges.rightStickHeld, rightPushed)) {
				// Dominant axis at the moment the dead zone is left; XInput Y grows upward.
				const int absX = std::abs(static_cast<int>(pad.sThumbRX)), absY = std::abs(static_cast<int>(pad.sThumbRY));
				g_rightStickPush = absY >= absX ? (pad.sThumbRY > 0 ? StickPush::Up : StickPush::Down)
					: (pad.sThumbRX > 0 ? StickPush::Right : StickPush::Left);
			}
		}
	}

	g_dpad = {0, 0, dpadButtons != 0 && dpadButtons != g_previousDpad, dpadButtons != 0};
	if (dpadButtons == XINPUT_GAMEPAD_DPAD_UP) g_dpad.y = -1;
	else if (dpadButtons == XINPUT_GAMEPAD_DPAD_DOWN) g_dpad.y = 1;
	else if (dpadButtons == XINPUT_GAMEPAD_DPAD_LEFT) g_dpad.x = -1;
	else if (dpadButtons == XINPUT_GAMEPAD_DPAD_RIGHT) g_dpad.x = 1;
	g_previousDpad = dpadButtons;
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

Stick LeftStick() { return g_leftStick; }
bool TakeLeftStickClick() {
	const bool clicked = g_leftStickClick;
	g_leftStickClick = false;
	return clicked;
}
bool TakeRightStickClick() {
	const bool clicked = g_rightStickClick;
	g_rightStickClick = false;
	return clicked;
}
StickPush TakeRightStickPush() {
	const StickPush push = g_rightStickPush;
	g_rightStickPush    = StickPush::None;
	return push;
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

FaceButton LastFaceButton(unsigned maxAgeMs) {
	return g_lastFaceButton != FaceButton::None && WithinAge(g_lastFaceButtonTickMs, maxAgeMs) ? g_lastFaceButton : FaceButton::None;
}

Vertical LastVertical(unsigned maxAgeMs) {
	if (const Vertical held = HeldVertical(); held != Vertical::None) {
		return held;
	}
	return g_lastVertical != Vertical::None && WithinAge(g_lastVerticalTickMs, maxAgeMs) ? g_lastVertical : Vertical::None;
}

DpadInput Dpad() { return g_dpad; }

void Shutdown() {
	g_dpad = {};
	g_previousDpad = 0;
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
	g_lastFaceButton       = FaceButton::None;
	g_lastFaceButtonTickMs = 0;
}

}
