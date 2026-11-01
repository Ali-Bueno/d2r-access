#pragma once

// Value controls (sliders, dropdowns) fire a UI update event when the player
// changes them, but the event carries no value, only that the control
// changed. To speak "increased"/"decreased" the plugin infers the direction
// from whichever input source must have just driven the change: the most
// recent left/right press across the gamepad D-pad, the gamepad left
// thumbstick, and the keyboard arrow keys.
namespace D2RAccess::InputDirection {

enum class Horizontal {
	None,
	Left,
	Right,
};

enum class Vertical {
	None,
	Up,
	Down,
};

// Samples all input sources once; call from the UI poll (see
// ui_thread_pump.h). Records a press EDGE per source (not-held -> held), so
// holding a direction down does not keep re-triggering it. Cheap, does not
// allocate, never throws.
void Poll();

// Cardinal D-pad input only; independent of sticks/keyboard and sampled once
// per UI tick. Diagonals remain held but do not choose an arbitrary direction;
// `diagonalY` keeps a diagonal's up (-1) or down (1) for a list, whose only axis that is.
struct DpadInput { int x = 0, y = 0; bool pressed = false, held = false; int diagonalY = 0; };
DpadInput Dpad();

// The arrow keys, as sampled by the last Poll(): one direction held, with no Ctrl or
// Shift (those step through lines). Only `held` is meaningful.
DpadInput ArrowKeys();

// The most recent left/right press edge, if it happened within the last
// `maxAgeMs` milliseconds (measured via GetTickCount64); Horizontal::None
// otherwise, or if no edge has been recorded yet.
Horizontal LastHorizontal(unsigned maxAgeMs);

// Same for up/down: a list that scrolls under a still focus indicator only
// reveals which way it moved through the press that caused it.
Vertical LastVertical(unsigned maxAgeMs);

// Gamepad face buttons. Some screens open a modal from a face button without
// any UI message saying which action it was (character select: X deletes);
// the press that preceded the modal tells.
enum class FaceButton {
	None,
	A,
	B,
	X,
	Y,
};
FaceButton LastFaceButton(unsigned maxAgeMs);

// Whether that face button was pressed since the previous Take() for it, then
// cleared: for a reader that acts on the press itself (the skill line reader
// closes on B). A caller should take it once before it starts listening, so an
// old press is not mistaken for a new one. A (alone, without BACK) and B are recorded.
bool TakeFacePress(FaceButton button);

// Whether that face button is down on any pad, as of the last Poll(): for a
// mode that lasts while it is held (compare, on Y).
bool FaceButtonHeld(FaceButton button);

// Right thumbstick push edges for the level scanner: a push is recorded once
// when the stick leaves its dead zone (same threshold as the left stick), in
// the dominant axis at that moment. Releasing below the threshold re-arms it,
// so cycling means push, release, push. Take() returns the latest push since
// the previous Take() and clears it.
enum class StickPush {
	None,
	Up,
	Left,
	Right,
	Down,
};
// Both takers name the reader doing the taking, so the log can say who got
// the push. Four readers ask for it in a fixed order on one tick and only one
// can win, so a push claimed by a reader that then has nothing to say is spent
// in silence -- which is what "sometimes it needs more than one push" looks
// like from the player's side. Nothing recorded that until PushRecord below.
StickPush TakeRightStickPush(const char* claimant);

// Claims the pending push only when it is the one asked for, leaving any
// other direction for the next consumer. The menu readers share the stick
// with the level scanner, and must not swallow the pushes that belong to it.
bool TakeRightStickPushIf(StickPush wanted, const char* claimant);

// Left thumbstick push edges, recorded only while Y is held: the reader for a
// held Y (compare in the inventory, skills in the hire list) lives on the left
// stick because Y and the right stick are both under the right thumb (the
// user, 2026-09-22). A pending push is dropped when Y is released, so no push
// made while walking is ever read later. Same claim rules as the right stick.
StickPush PeekLeftStickPush();
bool TakeLeftStickPushIf(StickPush wanted, const char* claimant);

// The pending push WITHOUT claiming it, so a reader whose gate is expensive
// can ask "is there anything for me?" before paying for the gate, and still
// leave the push for the next reader when the gate says no. A pending push
// survives ticks until it is claimed or a later push replaces it, so a reader
// that declines it costs the player nothing but the next tick.
StickPush PeekRightStickPush();

// What became of a push, written when it is resolved and not before: claimed
// by a reader, or replaced by a later push while still pending. A push still
// waiting for the reader that wants it therefore costs nothing per frame.
struct PushRecord {
	StickPush   push     = StickPush::None;  // None when nothing was resolved
	const char* claimant = nullptr;          // null when a later push replaced it
};
PushRecord TakePushRecord();

// The push's name for the log. English only: this is diagnosis, not speech.
const char* PushName(StickPush push);

// BACK (Select/View) held as a modifier: a face button pressed while it is
// down (the user, 2026-09-21): BACK+X life and mana, BACK+Y the room layout,
// BACK+A the exit finder, BACK+B the terrain overview. BACK alone does nothing
// for the mod and still reaches the game, which does nothing with it in play.
// The face button's press is kept from the game during play by pad_filter.h,
// so no attack or skill fires with it. Take() returns whether one happened
// since the previous Take() and clears it, so a caller must take it every
// tick, in a menu too.
bool TakeBackWith(FaceButton button);

// BACK+LB: follows or releases the scanned target (beacon). L3 and R3 are the
// game's (run/walk, weapon swap); pad_filter.h keeps this LB press from the
// game during play, as it does the face buttons. Take() returns whether a
// press happened since the previous Take() and clears it.
bool TakeBackWithLb();

// A right-stick push up or down made with BACK held, which no scanner or menu reader ever
// sees: BACK+down describes the route to the scanned target (beacon_service.cpp).
// Same Take() rules as BACK+LB.
StickPush TakeBackWithRightStick();
// The same for a push left or right: the tutorial's previous and next message.
StickPush TakeBackWithRightStickSide();
// BACK+D-pad up: the tutorial's first message. The press is no ordinary D-pad
// or vertical edge. Same Take() rules as BACK+LB.
bool TakeBackWithDpadUp();

// BACK+LT (`right` false): the main skill bar; BACK+RT: the next of the mercenary,
// summons and party members (a tester, 2026-10-14). pad_filter.h keeps that
// trigger from the game during play. Same Take() rules as BACK+LB.
bool TakeBackWithTrigger(bool right);

// BACK+A and BACK+down trade places (a tester, 2026-10-14, switch `pad-route-on-a`):
// BACK+A then gives the route; the other of the two is free since the passage finder
// went (2026-10-22), so nothing takes it.
bool RouteOnA();
void SetRouteOnA(bool on);

// BACK+RB: opens and closes the mod's settings list, the pad's Ctrl+S
// (keyboard_service.cpp). pad_filter.h keeps this RB press from the game
// during play, as it does LB's. Same Take() rules as BACK+LB.
bool TakeBackWithRb();

// LB or RB pressed without BACK: lower or raise a setting while the settings
// list is open, when pad_filter.h keeps them from the game. Same Take() rules.
bool TakeShoulder(bool right);

// L3, the game's own run/walk toggle, which still reaches it: read only to
// announce the new gait. Take() returns whether a press happened since the
// previous Take() and clears it.
bool TakeLeftStickClick();

// Left trigger (LT/L2), the game's skill-bar toggle, as sampled by the last
// Poll(): held past XInput's trigger threshold, which the game's own poll
// uses too. `otherButton` is set while any digital button or the right
// trigger is also down, i.e. while LT is part of a combo. Sticks do not count.
struct TriggerHold { bool held = false, otherButton = false; };
TriggerHold LeftTrigger();

// Left thumbstick of the first connected pad as sampled by the last Poll(),
// in screen convention (x right, y down), each axis in [-1, 1]. Diagnostics
// only: the radar logs it next to each cue so a log can tell where the player
// was steering when the cue fired. Zero and not connected without a pad.
struct Stick { float x = 0, y = 0; bool connected = false; };
Stick LeftStick();
// Whether a pad was used -- a button pressed, a stick or trigger pushed past
// its threshold -- since the previous Take(), then cleared.
bool TakePadUse();
// The share of a stick's travel that counts as a deliberate push, not drift.
inline constexpr float StickPushFraction = 0.5f;

// The keyboard's twins of the gestures above. A key posts the gesture its
// reader already listens for, so every reader serves pad and keyboard alike and
// none has to learn a second input. The game takes no part in this: nothing
// posted here is input to it, and no pad is imitated. Post after Poll() in the
// same update; a left-stick push no reader claims is dropped by the next Poll,
// as one made without Y held is.
void PostRightStickPush(StickPush push);
// A left-stick push is dropped by the next Poll unless Y is held, and its
// reader looks only every third update or so: a posted one is kept for that long.
void PostLeftStickPush(StickPush push); // as if made with Y held
// Ctrl or Shift with an arrow key in a menu: a step through the lines of what
// the cursor is on, up and down a line, left and right to the first and the last.
// It is a right-stick push to every reader that walks lines already (the
// character panel, Advanced Stats), and LineStepPending tells the reader of
// items and skills that the keyboard made it, where the stick's four
// directions are four different readouts. Plain is Shift's: the tooltip as the
// game draws it with Shift held, not its own Ctrl view of an item's ranges.
void PostLineStep(StickPush push, bool plain = false);
bool LineStepPending();
bool LineStepPlain();
void PostBackWith(FaceButton button);
void PostBackWithLb();
// BACK + the right stick up or down, and BACK + a trigger, for the keyboard.
void PostBackWithRightStick(StickPush push);
void PostBackWithTrigger(bool right);
void PostFacePress(FaceButton button);  // A and B have readers

// Releases the dynamically loaded XInput library. Safe to call even if Poll
// was never called.
void Shutdown();

}
