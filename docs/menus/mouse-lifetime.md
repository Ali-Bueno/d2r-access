# Menu mouse gesture lifetime

2026-10-20 follow-up: native correction complete/offline-verified and installed;
eight actual observer checks/full 128/128. Revised guard gameplay is pending.
The user confirms Character still opened in the prior 26E052AC build. Its
17 queue checks/126 passing suites proved queue cancellation, not the live cause.
Parent: [keyboard menus](../keyboard-play.md).

Shift+S's intentional HUD action uses the verified native left-button callback,
not this menu-only queue. Its regression/fix and caller/ABI evidence are in
[left-hand selector](left-skill-menu.md); gameplay cancellation below remains.

## Native keyboard producer correction

The new own log records Waypoint closure/parking at 04:37:50.7, successful
monster selection at 04:37:51.922, Character opening 04:37:52.245, Close click
04:37:52.965 and the first fight at 04:37:54.017. There is no preceding mod
attack or queued menu click at the Character open. Physical input provenance
is still absent; do not claim a captured mouse event or a runtime-confirmed cause.

The user's A and C both bind native Character action zero. Static key matching
0x122990 reads the packed native key at binding+4 and passes that record to
BindingDown 0x11FF30. That dispatcher broadcasts input then calls its native
action; action zero is 0x1196F0, which toggles Character UI state 2. Waiting for
a later mod SDK callback cannot retract a native action already dispatched.

src/input/native_keyboard.cpp installs a narrowly scoped pre-dispatch hook,
after NativeSkillHotkeys has verified the original BindingDown bytes. Both
dispatcher/matcher full fingerprints and the hook prefix must match. It copies
only action/key fields in a short read scope, then releases observation before
calling either owner. Action zero may be claimed by KeyboardService's existing
OnAction only when its packed unmodified key matches a copied registered attack
binding, with ordinary/physical Shift input. Typing rejects that claim, so the
original still gets its editor broadcast. C remains native when not bound to
the mod's attack; forced controller events/unrelated actions/unreadable records
forward original arguments/results untouched. Ctrl/Alt clicks retain their
existing route. Both primary/secondary unmodified attack bindings are copied
at registration; binding changes require registration/restart for this fallback
snapshot. Normal SDK callbacks continue to use their actual current bindings.

This fallback reuses atomic hold/press guards, so a later SDK delivery cannot
start another fight/click for the same held A. Existing physical-release polling
retires holds even when skipping native Character dispatch also skips its
release bookkeeping. It neither synthesizes mouse/controller state nor delays
the native thread for workers. Native hook cost is two bounded field reads,
cached intent checks and a counter; UI diagnostics drain at most once a second
through AsyncLog. No scene/path scan, speech or log formatting runs in the hook.
No pointers are handed to a worker. Reset retires the callback before unregister;
loader owns hook removal. Recovery cancels intent but retains binding ownership.

native-keyboard-tests compiles the actual observer with native memory fixtures
and a fake original/claim boundary. It verifies original Character dispatch
never runs for claimed A, while C, typing, forced controller, unrelated native
actions, unreadable pointers and reset retain exact forwarding. This establishes
pre-dispatch ordering offline, not Windows/native gameplay confirmation.

## Evidence and correction

Own latest log (Oct 20, last write 03:54:39) records inventory/vendor/stash
closures around 03:28:18.8 and parking at 962,1042, then Character pane opening
03:28:20.997. Cursor lands on Close, clicks 03:28:21.636; pane closes
03:28:21.711. The later 03:50:37 Character open includes actual stat allocations
and is not identified as the bug. Physical-key/queue provenance is absent;
the exact trigger is unproved.

Verified implementation defects: MenuMouse::Tick dispatched before play/menu
validation; Release retained queued input and Park appended behind it; failed
queued placement still allowed Down at the old pointer. A outside both cached
menu/play contexts could also reach the game's Character binding. Those paths
are corrected. Combat continues through native commands, never this mouse queue.

## Pipeline, state and bounds

The UI pump calls KeyboardService::Tick before KeyboardMenu::Tick. KeyboardService
samples RadarUi play state. KeyboardMenu validates foreground/native readers/play
before mouse dispatch, then passes PanelState::Revision, a monotonic UI-owned
generation incremented on open, known close, movie start/end and reset. Same-name
close/reopen is a new lifetime; an absent-panel close changes nothing.

MenuMouse::Tick cancels BEFORE any step when menu ownership is absent or generation
changes. Reset drops future place/down/up/return steps and releases any mod-owned
left/right button once. KeyboardMenu clears pending actions and forces refresh
on generation change. Release also resets mouse state. Park moves directly after
cancellation, without a delayed click/return. Focus loss retains its reset.
An unrelated tracked panel change may conservatively drop a gesture; press again
after refresh. Transition A/W presses are consumed outside typing, preventing
Character-shortcut fallthrough; a transition press can be discarded safely.

Click requires an engaged, landed entry in a visible owning panel and fresh native
rectangle. Missing target/geometry uses existing localized Unavailable speech.
Queued Place failure cancels its following Down instead of clicking the old HUD.

The UI-owned queue stores copied rectangles/button ids/deadlines, no native
widget pointers. Sixteen steps is the capacity; overflow cancels the burst and
releases owned buttons. Complete gestures reserve capacity, with no producer wait.
Tick performs at most one step, retaining 60 ms move settling and 40 ms button
hold. Normal ClickAt is three steps, four with return. No sleep/drain or new full
UI walk runs per busy frame: context checking is constant time. PanelState keeps
its existing UI pump ownership; PRISM/slow logs retain workers. No new setting,
persistence, sound or spoken string. Existing native/menu traversal costs and
unmeasured runtime response remain explicit limitations.

## Extension and checks

Keep context validation before dispatch; use checked native geometry, complete
gesture reservations and current panel lifetime. Honor Busy for tab/page actions.
Keep native sort/repair/belt routes under their own context guards. Never reuse
menu OS clicks for world combat.

menu-mouse-tests compiles the production queue with isolated Win32 input and
pointer stubs; it sends no desktop/game input. It covers normal timing/release/
return, exit before Down, context loss without a panel event, same-panel reopen
while down, idempotent release, placement failure, focus loss, overload and
reset/absent-close generations. Existing radar-ui/menu navigation checks remain.
Full totals/deployment evidence belong in STATUS/HANDOFF/local verification.

First gameplay: open a shop, move among items, close it, leave town and hold A at
an enemy. Character must stay closed and fighting start. Repeat after inventory/
stash closure and verify regular menu clicks/Shift+A equip. If it recurs, keep
the next own log; the current log does not identify every external input producer.
