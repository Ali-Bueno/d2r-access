#include "input_direction.h"

#include <windows.h>

#include <xinput.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

namespace D2RAccess::InputDirection {

namespace {

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

// The stick axes are signed 16-bit values (SHORT), per the XInput API
// contract; the threshold below is derived from that type, not guessed.
constexpr SHORT kThumbstickMaxMagnitude = std::numeric_limits<SHORT>::max();

// Intentionally much larger than the small anti-drift deadzone XInput itself
// recommends (XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE): this one has to signal a
// deliberate directional push, not just that the stick is off-center.
constexpr float kThumbstickDirectionDeadzoneFraction = StickPushFraction;
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
	bool dpadUpOwned    = false; // this press began with BACK held: the tutorial's, not a direction
	bool dpadDownHeld   = false;
	bool stickUpHeld    = false;
	bool stickDownHeld  = false;
	bool aHeld          = false;
	bool bHeld          = false;
	bool xHeld          = false;
	bool yHeld          = false;
	bool rightStickHeld = false;
	bool leftThumbHeld  = false;
	bool lbHeld         = false;
	bool rbHeld         = false;
	bool ltHeld         = false;
	bool rtHeld         = false;
	WORD buttons        = 0;
	bool pushed         = false; // a stick or trigger past its threshold
};

PadEdgeState g_padState[XUSER_MAX_COUNT] {};
// Microsoft's XInput guidance: asking an empty slot is slow, so do it every few
// seconds, not every frame (a tester's machine spent 5-8 ms an update on it).
constexpr uint64_t EmptySlotProbeMs = 2000;
uint64_t g_nextProbe[XUSER_MAX_COUNT] {}; // when an empty slot is asked again; 0 while a pad answers
bool         g_keyLeftHeld  = false;
bool         g_keyRightHeld = false;
bool         g_keyUpHeld    = false;
bool         g_keyDownHeld  = false;
DpadInput g_dpad;
WORD g_previousDpad = 0;
Stick g_leftStick;
bool g_leftStickClick = false;
bool g_backWithA = false, g_backWithB = false, g_backWithX = false, g_backWithY = false, g_backWithLb = false, g_backWithRb = false;
bool g_backWithDpadUp = false;
bool g_padUsed = false;
bool g_backWithLt = false, g_backWithRt = false;
bool g_lbAlone = false, g_rbAlone = false;
bool g_pressedB = false;
bool g_pressedA = false; // A alone, for the pad typing of a text box (pad_typing.h)
StickPush g_leftStickPush = StickPush::None; // only while Y is held
StickPush g_backWithRightStick = StickPush::None; // Up or Down only
StickPush g_backWithRightStickSide = StickPush::None; // Left or Right only
std::atomic<bool> g_routeOnA{false};          // BACK+A and BACK+down trade places
ULONGLONG g_leftStickPosted = 0;              // when the keyboard posted one, 0 when it did not
constexpr ULONGLONG kPostedPushMs = 150;      // several updates of the reader that takes it
bool g_lineStep = false;                      // the pending right-stick push is the keyboard's line step
bool g_lineStepPlain = false;                 // ... made with Shift, for the tooltip as drawn with no key held
ULONGLONG g_lineStepPosted = 0;
TriggerHold g_leftTrigger;

Horizontal g_lastHorizontal       = Horizontal::None;
ULONGLONG  g_lastHorizontalTickMs = 0;
Vertical   g_lastVertical         = Vertical::None;
ULONGLONG  g_lastVerticalTickMs   = 0;
FaceButton g_lastFaceButton       = FaceButton::None;
StickPush  g_rightStickPush       = StickPush::None;
ULONGLONG  g_lastFaceButtonTickMs = 0;

// The last push to be resolved, waiting to be logged. A claimant name means a
// reader took it; null means a later push arrived while this one was still
// pending, which is the only way a push is lost outright.
StickPush   g_resolvedPush     = StickPush::None;
const char* g_resolvedClaimant = nullptr;

void ResolvePush(StickPush push, const char* claimant) {
	if (push == StickPush::None) return;
	g_resolvedPush     = push;
	g_resolvedClaimant = claimant;
}

// A stick or trigger past the threshold at which the mod and the game count it as moved.
bool Pushed(const XINPUT_GAMEPAD& pad) {
	const auto past = [](SHORT axis) { return axis >= kThumbstickDirectionThreshold || axis <= -kThumbstickDirectionThreshold; };
	return past(pad.sThumbLX) || past(pad.sThumbLY) || past(pad.sThumbRX) || past(pad.sThumbRY)
		|| pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD || pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
}

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
	g_leftTrigger = {};
	if (g_xInputGetState != nullptr) {
		const uint64_t now = GetTickCount64();
		for (DWORD padIndex = 0; padIndex < XUSER_MAX_COUNT; ++padIndex) {
			PadEdgeState& edges = g_padState[padIndex];
			if (g_nextProbe[padIndex] && now < g_nextProbe[padIndex]) continue;
			XINPUT_STATE  state {};
			if (g_xInputGetState(padIndex, &state) != ERROR_SUCCESS) {
				// Not connected: clear held flags so a later reconnect starts
				// from a clean edge instead of replaying a stale press.
				edges = {};
				g_nextProbe[padIndex] = now + EmptySlotProbeMs;
				continue;
			}
			g_nextProbe[padIndex] = 0;

			const XINPUT_GAMEPAD& pad = state.Gamepad;
			const bool pushed = Pushed(pad);
			if ((pad.wButtons & ~edges.buttons) != 0 || (pushed && !edges.pushed)) g_padUsed = true;
			edges.buttons = pad.wButtons;
			edges.pushed  = pushed;
			const bool backDown = (pad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
			const bool dpadUpDown = (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
			if (UpdateEdge(edges.dpadUpHeld, dpadUpDown)) {
				edges.dpadUpOwned = backDown;
				if (backDown) g_backWithDpadUp = true;
				else RecordEdge(Vertical::Up);
			}
			dpadButtons |= pad.wButtons & DpadMask & ~(edges.dpadUpOwned ? XINPUT_GAMEPAD_DPAD_UP : 0);
			g_leftTrigger.held = g_leftTrigger.held || pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
			g_leftTrigger.otherButton = g_leftTrigger.otherButton || pad.wButtons != 0
				|| pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
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
				if (edges.yHeld) g_leftStickPush = StickPush::Left;
			}
			if (UpdateEdge(edges.stickRightHeld, pad.sThumbLX >= kThumbstickDirectionThreshold)) {
				RecordEdge(Horizontal::Right);
				if (edges.yHeld) g_leftStickPush = StickPush::Right;
			}
			// XInput's Y axis grows upward.
			if (UpdateEdge(edges.dpadDownHeld, (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0)) {
				RecordEdge(Vertical::Down);
			}
			if (UpdateEdge(edges.stickUpHeld, pad.sThumbLY >= kThumbstickDirectionThreshold)) {
				RecordEdge(Vertical::Up);
				if (edges.yHeld) g_leftStickPush = StickPush::Up;
			}
			if (UpdateEdge(edges.stickDownHeld, pad.sThumbLY <= -kThumbstickDirectionThreshold)) {
				RecordEdge(Vertical::Down);
				if (edges.yHeld) g_leftStickPush = StickPush::Down;
			}
			// BACK is a modifier: a face button pressed while it is held (pad_filter.h
			// keeps that press from the game during play).
			if (UpdateEdge(edges.aHeld, (pad.wButtons & XINPUT_GAMEPAD_A) != 0)) {
				RecordEdge(FaceButton::A);
				if (backDown && g_routeOnA.load()) g_backWithRightStick = StickPush::Down;
				else if (backDown) g_backWithA = true;
				else g_pressedA = true;
			}
			if (UpdateEdge(edges.bHeld, (pad.wButtons & XINPUT_GAMEPAD_B) != 0)) {
				RecordEdge(FaceButton::B);
				g_pressedB = true;
				if (backDown) g_backWithB = true;
			}
			if (UpdateEdge(edges.xHeld, (pad.wButtons & XINPUT_GAMEPAD_X) != 0)) {
				RecordEdge(FaceButton::X);
				if (backDown) g_backWithX = true;
			}
			if (UpdateEdge(edges.yHeld, (pad.wButtons & XINPUT_GAMEPAD_Y) != 0)) {
				RecordEdge(FaceButton::Y);
				if (backDown) g_backWithY = true;
			}
			const bool rightPushed = pad.sThumbRX >= kThumbstickDirectionThreshold || pad.sThumbRX <= -kThumbstickDirectionThreshold
				|| pad.sThumbRY >= kThumbstickDirectionThreshold || pad.sThumbRY <= -kThumbstickDirectionThreshold;
			if (UpdateEdge(edges.lbHeld, (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0)) (backDown ? g_backWithLb : g_lbAlone) = true;
			if (UpdateEdge(edges.rbHeld, (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0)) (backDown ? g_backWithRb : g_rbAlone) = true;
			// The triggers count as pulled past XInput's threshold, as the game's own poll reads them.
			if (UpdateEdge(edges.ltHeld, pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) && backDown) g_backWithLt = true;
			if (UpdateEdge(edges.rtHeld, pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) && backDown) g_backWithRt = true;
			if (UpdateEdge(edges.leftThumbHeld, (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0)) g_leftStickClick = true;
			if (UpdateEdge(edges.rightStickHeld, rightPushed)) {
				// Dominant axis at the moment the dead zone is left; XInput Y grows upward.
				const int absX = std::abs(static_cast<int>(pad.sThumbRX)), absY = std::abs(static_cast<int>(pad.sThumbRY));
				const StickPush push = absY >= absX ? (pad.sThumbRY > 0 ? StickPush::Up : StickPush::Down)
					: (pad.sThumbRX > 0 ? StickPush::Right : StickPush::Left);
				if (backDown && g_routeOnA.load() && push == StickPush::Down) g_backWithA = true;
				else if (backDown) { // the mod's chord, never a scanner step
					(push == StickPush::Left || push == StickPush::Right ? g_backWithRightStickSide : g_backWithRightStick) = push;
				}
				else {
					// A push still pending here is one no reader has wanted, and it
					// is about to be overwritten. Record that before it goes.
					ResolvePush(g_rightStickPush, nullptr);
					g_lineStep = false;
					g_rightStickPush = push;
				}
			}
		}
	}

	// A line step nothing in the menu had lines for is not kept for the level
	// scanner to find when the menu closes.
	if (g_lineStep && GetTickCount64() - g_lineStepPosted > kPostedPushMs) {
		ResolvePush(g_rightStickPush, nullptr);
		g_rightStickPush = StickPush::None;
		g_lineStep = false;
	}
	const bool posted = g_leftStickPosted != 0 && GetTickCount64() - g_leftStickPosted <= kPostedPushMs;
	if (!posted) g_leftStickPosted = 0;
	if (!FaceButtonHeld(FaceButton::Y) && !posted) g_leftStickPush = StickPush::None;
	g_dpad = {0, 0, dpadButtons != 0 && dpadButtons != g_previousDpad, dpadButtons != 0};
	if (dpadButtons == XINPUT_GAMEPAD_DPAD_UP) g_dpad.y = -1;
	else if (dpadButtons == XINPUT_GAMEPAD_DPAD_DOWN) g_dpad.y = 1;
	else if (dpadButtons == XINPUT_GAMEPAD_DPAD_LEFT) g_dpad.x = -1;
	else if (dpadButtons == XINPUT_GAMEPAD_DPAD_RIGHT) g_dpad.x = 1;
	else if (dpadButtons & XINPUT_GAMEPAD_DPAD_UP) g_dpad.diagonalY = -1;
	else if (dpadButtons & XINPUT_GAMEPAD_DPAD_DOWN) g_dpad.diagonalY = 1;
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
bool TakePadUse() { return std::exchange(g_padUsed, false); }
TriggerHold LeftTrigger() { return g_leftTrigger; }
bool TakeLeftStickClick() {
	const bool clicked = g_leftStickClick;
	g_leftStickClick = false;
	return clicked;
}
StickPush TakeBackWithRightStick() {
	const StickPush push = g_backWithRightStick;
	g_backWithRightStick = StickPush::None;
	return push;
}
StickPush TakeBackWithRightStickSide() {
	const StickPush push = g_backWithRightStickSide;
	g_backWithRightStickSide = StickPush::None;
	return push;
}
bool TakeBackWithDpadUp() {
	const bool pressed = g_backWithDpadUp;
	g_backWithDpadUp = false;
	return pressed;
}
bool TakeBackWithLb() {
	const bool pressed = g_backWithLb;
	g_backWithLb = false;
	return pressed;
}
bool TakeBackWithRb() {
	const bool pressed = g_backWithRb;
	g_backWithRb = false;
	return pressed;
}
bool TakeBackWithTrigger(bool right) {
	bool& pressed = right ? g_backWithRt : g_backWithLt;
	return std::exchange(pressed, false);
}
bool RouteOnA() { return g_routeOnA.load(); }
void SetRouteOnA(bool on) { g_routeOnA = on; }
bool TakeShoulder(bool right) {
	bool& pressed = right ? g_rbAlone : g_lbAlone;
	const bool taken = pressed;
	pressed = false;
	return taken;
}
bool TakeBackWith(FaceButton button) {
	bool* pressed = button == FaceButton::A ? &g_backWithA : button == FaceButton::B ? &g_backWithB
		: button == FaceButton::X ? &g_backWithX
		: button == FaceButton::Y ? &g_backWithY : nullptr;
	if (pressed == nullptr) return false;
	const bool result = *pressed;
	*pressed = false;
	return result;
}
bool FaceButtonHeld(FaceButton button) {
	for (const PadEdgeState& pad : g_padState) {
		if ((button == FaceButton::A && pad.aHeld) || (button == FaceButton::B && pad.bHeld)
			|| (button == FaceButton::X && pad.xHeld) || (button == FaceButton::Y && pad.yHeld)) return true;
	}
	return false;
}
StickPush PeekLeftStickPush() { return g_leftStickPush; }
bool TakeLeftStickPushIf(StickPush wanted, const char*) {
	if (wanted == StickPush::None || g_leftStickPush != wanted) return false;
	g_leftStickPush = StickPush::None;
	return true;
}
bool TakeFacePress(FaceButton button) {
	bool* pressed = button == FaceButton::A ? &g_pressedA : button == FaceButton::B ? &g_pressedB : nullptr;
	if (!pressed) return false; // only A and B have readers so far
	const bool result = *pressed;
	*pressed = false;
	return result;
}
StickPush TakeRightStickPush(const char* claimant) {
	const StickPush push = g_rightStickPush;
	ResolvePush(push, claimant);
	g_rightStickPush    = StickPush::None;
	return push;
}
StickPush PeekRightStickPush() { return g_rightStickPush; }
bool TakeRightStickPushIf(StickPush wanted, const char* claimant) {
	if (wanted == StickPush::None || g_rightStickPush != wanted) {
		return false;
	}
	ResolvePush(wanted, claimant);
	g_rightStickPush = StickPush::None;
	return true;
}
void PostRightStickPush(StickPush push) {
	if (push == StickPush::None) return;
	// A push still pending is about to be replaced, as a pad's would be.
	ResolvePush(g_rightStickPush, nullptr);
	g_rightStickPush = push;
	g_lineStep = false;
}
void PostLeftStickPush(StickPush push) {
	g_leftStickPush = push;
	g_leftStickPosted = GetTickCount64();
}
void PostLineStep(StickPush push, bool plain) {
	PostRightStickPush(push);
	g_lineStep = push != StickPush::None;
	g_lineStepPlain = plain;
	g_lineStepPosted = GetTickCount64();
}
bool LineStepPending() { return g_lineStep && g_rightStickPush != StickPush::None; }
bool LineStepPlain() { return g_lineStepPlain; }
void PostBackWith(FaceButton button) {
	if (button == FaceButton::A) g_backWithA = true;
	else if (button == FaceButton::B) g_backWithB = true;
	else if (button == FaceButton::X) g_backWithX = true;
	else if (button == FaceButton::Y) g_backWithY = true;
}
void PostBackWithLb() { g_backWithLb = true; }
void PostBackWithRightStick(StickPush push) {
	if (push == StickPush::Up || push == StickPush::Down) g_backWithRightStick = push;
}
void PostBackWithTrigger(bool right) { (right ? g_backWithRt : g_backWithLt) = true; }
void PostFacePress(FaceButton button) {
	if (button == FaceButton::B) g_pressedB = true;
	if (button == FaceButton::A) g_pressedA = true;
}
PushRecord TakePushRecord() {
	const PushRecord record{g_resolvedPush, g_resolvedClaimant};
	g_resolvedPush     = StickPush::None;
	g_resolvedClaimant = nullptr;
	return record;
}
const char* PushName(StickPush push) {
	switch (push) {
	case StickPush::Up:    return "up";
	case StickPush::Left:  return "left";
	case StickPush::Right: return "right";
	case StickPush::Down:  return "down";
	default:               return "none";
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
		up   = up || (edges.dpadUpHeld && !edges.dpadUpOwned) || edges.stickUpHeld;
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

DpadInput ArrowKeys() {
	const bool modified = (GetAsyncKeyState(VK_CONTROL) & kKeyDownFlag) != 0 || (GetAsyncKeyState(VK_SHIFT) & kKeyDownFlag) != 0;
	const int x = (g_keyRightHeld ? 1 : 0) - (g_keyLeftHeld ? 1 : 0), y = (g_keyDownHeld ? 1 : 0) - (g_keyUpHeld ? 1 : 0);
	if (modified || (x && y) || (!x && !y)) return {};
	return {x, y, false, true};
}

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
	for (auto& probe : g_nextProbe) probe = 0;
	g_padUsed = false;
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
