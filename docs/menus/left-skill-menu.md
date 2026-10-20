# Left-hand skill selector: Shift+S

2026-10-20: implementation complete, offline-verified and installed. Nine
adapter checks and full 129/129 pass; deployment/code/data verification is in
STATUS and workspace re/left-skill-2026-10-20/verification.md. Gameplay
confirmation pending. Parent: [keyboard menus](../keyboard-play.md).

## Diagnosis and input path

The user's own latest log records four left-HUD requests at 09:02:51.820,
09:02:53.135, 09:02:54.477 and 09:02:55.648 without opening SkillSelect.
Later openings at 09:02:56.127 and 09:02:57.633 explicitly announce the right
hand. The log proves request delivery and a resolved HUD button; its old
"is clicked" diagnostic precedes queue dispatch and does not prove a click.

KeyboardService::OnAction accepts the registered keyboard-left-skill binding
(default Shift+S), excluding active text editing. Its copied atomic command is
consumed by KeyboardService::Tick on the existing UI pump, after lifecycle
readiness is checked. KeyboardMenu::LeftSkillList finds the current visible
HUDPanel/LeftSkillButton and requires fresh visible, on-screen native geometry
and verified mouse input mode. Previously it queued MenuMouse::ClickAt; the
same pump's KeyboardMenu::Tick cancels menu-only gestures during gameplay.
That cancellation is required to prevent old shop/menu clicks reaching the HUD.

LeftSkillList now calls NativeSkillHand::ActivateLeft on that fresh widget.
It invokes the actual native button callback immediately on the UI owner;
there is no OS click, pointer movement or deferred widget pointer. It retains
the game's behavior: open the left selector, switch the right selector to left,
or close an already-open left selector. S continues through the game's original
right-hand action. Arrows, A selection and function-key binding in the opened
list retain the existing menu cursor and native assignment routes.

## Native contract and ownership

Target D2R 3.3.93787. NativeSkillHand initialization already verifies the complete
SkillSelectButtonWidget::OnClick at RVA 0x2389F0, 546 bytes, normalized FNV-1a
B9B54A9E9333B2D6; native chosen-side reader 0x238240, 143 bytes,
B7370C1DF57D057C; and its registry dependencies. The image's skill-button
vtable at 0x1CE4210 has that callback at slot +0xC0. Its x64 callback ABI takes
the widget as the sole used argument and has no used result. Widget side at
+0xB88 is 1 for left HUD, 2 for right, 0 for skill-list icons and 3 for binding.
ActivateLeft requires the exact verified callback pointer and side 1, rejecting
other types/sides or unreadable objects. Short NativeMemory validation finishes
before a separate SEH-contained native call. The existing whole-pump permission
read scope remains; no live object is published to workers.

The callback owns native controller-mode checks, chosen-side registry changes
and UI state 3 (SkillSelect). The mod does not write those fields. Current
geometry/foreground/mode are checked by the caller; native faults or failed
verification use existing localized Unavailable speech. NativeSkillHand resets
its callback before initialization and only installs it after all fingerprints
pass. No new hooks, worker, retained widget state, setting, persistence or
user-facing string is added. Existing localized hand announcements remain.

One request performs the existing bounded HUD search/geometry read, three
additional pointer/side reads and one native callback. There is no queue wait,
sleep, retry or substantial added scan. Native/UI/speech aggregate cost and
audible latency have not been measured in game. Generic queue cancellation,
capacity, focus loss and Character interception remain unchanged; see
[menu lifetime](mouse-lifetime.md). Release/reset continues through that owner.

## Verification and extension

native-skill-hand tests compile the production adapter with a mod-owned widget
and fake native callback. Nine checks cover immediate/repeated dispatch, exact
widget identity, left versus right/list/binding sides, mismatched virtual slot,
unreadable/null target, unavailable service and missing callback. They execute
no game code or desktop input; native open/switch/toggle behavior is established
statically and remains a gameplay check. Existing menu-mouse tests must keep
passing to prove the stale-gesture protection remains intact.

Source-bearing native exports stay outside Git under workspace
re/actions/widgets; task log, hashes and verification under
re/left-skill-2026-10-20. Keep any new HUD action explicitly typed and freshly
validated; never relax MenuMouse's gameplay cancellation to support it. Native
layout changes require re-establishing ABI, slot/side fields and whole-function
fingerprints together.

First gameplay check: during normal play, press Shift+S and hear the left-hand
skill selector; choose a skill with arrows and A. Press S, then Shift+S, and
verify the selector changes to left. Press Shift+S again to close it. Verify
ordinary shop tab cycling and menu closure followed by A still work.
