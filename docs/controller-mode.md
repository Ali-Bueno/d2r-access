# Controller mode, and keyboard play as a simulated pad

## Second attempt: where it stands (2026-10-05)

The user wants the keyboard cursor (`src/keyboard_menu.cpp`) replaced by keys
that drive the game's own controller mode. A first version was built on
2026-09-19 (`bb716be`, `35b7c59`, `28e07db`) and withdrawn the next day
(`85aac6c`). Its code is recoverable with
`git show 85aac6c^:src/<file>` for `xinput_hook.*` (detour, slot choice,
injection), `keyboard_pad.*` (key-to-pad mapping), `key_claim.*` (arrow-key
consumption) and `input_stack.*` (load-time input module report). Game build
then and now: 3.3.93787 (`D2R.exe` unchanged since 2026-09-14).

Settled, MEASURED by that attempt (details below):

- D2R reads pads only through `xinput9_1_0!XInputGetState` (one call site,
  `0x141210ac2`); that module has no ordinal 100, so one detour sees every poll.
- D2R polls all four slots (empty ones about every 5 s), so a pad that appears
  on any slot is found.
- Controller mode is one-way: after one pad input the game's own keyboard
  scheme is gone (only Escape and Enter remain). Keyboard input does **not**
  switch the game back. Loader actions, mod hotkeys and the console still work.

Why it was withdrawn, and what the second attempt must do instead:

| Failure (live) | Cause | Second attempt |
| --- | --- | --- |
| A real controller stopped working at startup | D2R uses one pad and picked the synthetic one, even idle on another slot | Inject only while no physical pad is connected (asked of the trampoline, which never sees our pad); stop the moment one appears |
| Typing a character name failed | Keys were read with `GetAsyncKeyState` and the arrows were consumed as D-pad | Pass every key through while text is edited (`NativeSkillNavigation::TextEditing`, focused `TextInput`), as `keyboard_menu.cpp` already does |
| Movement never reached the game (OPEN) | Unknown: the `[pad] frame` log (`28e07db`) came the day before removal | First live check: the `[pad] frame` line and the per-slot poll census |
| The key scheme | Rejected by the user | New scheme, to be chosen with the user |

Also needed: the mod's own pad reader (`InputDirection`, through `xinput1_4`)
never sees the detour, so the synthetic frame must feed it too, or the
right-stick scanner and other pad gestures stay deaf.

## Record of the first attempt (2026-09-19)

Established in game by the user, 2026-09-19. This is the fact the keyboard
design rests on, so it is recorded rather than re-derived.

## D2R has a controller mode, and it is one-way

- Before any controller input, the **keyboard scheme is live**: `I` inventory,
  `Q` quest log, `A` and `C` both character panel, `T` skill tree, `F1`-`F8`
  the skill hotkeys, `1`-`4` the belt. `G` answers "unavailable in your current
  mode".
- **The moment a single controller button is touched, the keyboard is disabled
  entirely.** Only `Escape` and `Enter` still do anything.
- No way back was found. Controller mode persists for the session.

## What follows from that

**The key-collision problem is void.** Nine of the seventeen keys the pad
mapping wants are bound in D2R's stock keyboard scheme — `W` is Swap Weapons,
`A` is Character Screen, `I`/`B` Inventory, `T` Skill Tree, plus `N`, `V`, `O`
and `S`. That looked like a blocker: the game leaves exactly 17 keys unbound,
the mapping needs 17 (19 once both stick clicks have keys), and `W`, `A` and
`S` are stock-bound, so no choice of keys avoids it.

It does not matter. Simulating a pad puts the game in controller mode, which
disables the keyboard scheme, so **nothing contends for those keys** — the mod
owns the whole keyboard. Do not spend effort picking keys around the stock
bindings, and do not clear bindings in the `.key` file to make room.

**The developer console and the mod hotkeys keep working. An earlier version
of this document claimed they did not; that was wrong.**

Tested by the user, 2026-09-19: the console opens with keyboard control both
enabled and disabled, and `\`, `,` and `.` never stopped working. The claim
that controller mode disables D2RLoader's input-action system was inferred
from one string in `D2RCore.dll` (`"Could not register the D2RLoader Console
input action."`) plus the note above about the game's own bindings, and it was
never measured. A string proves a message exists; it says nothing about when
it fires or what gates it.

What the game's controller mode does disable is **its own** keyboard scheme --
`I`, `Q`, `A`, `C`, `T`, `F1`-`F8`, `1`-`4` -- which is the user's direct
observation recorded at the top of this file. Mod hotkeys are registered by
the loader and are mod-side; they are unaffected.

**Enter controller mode deliberately, not at startup.** It is one-way and it
costs the game's own keyboard scheme, so the pad is off by default and the
player asks for it with Ctrl+Shift+F12, which is announced when it happens.
The toggle is read raw with `GetAsyncKeyState` rather than registered as an
input action, so that it cannot depend on anything the pad itself changes.

**Everything the keyboard used to reach must now be reachable as a pad input.**
The keyboard scheme is gone, so inventory, quest log, character panel and skill
tree are only reachable the way a controller reaches them. The mapping has to
cover the pad's full button set, not just the ones gameplay uses.

## The mapping (rejected by the user, 2026-09-20)

| key | pad input | |
|---|---|---|
| `W` `A` `S` `D` | left stick | move; opposite keys cancel, diagonals keep the stick's circular magnitude |
| `O` `K` `L` `;` | right stick | the entity scan cluster |
| arrow keys | D-pad | menu navigation, the game's own focus |
| `J` `N` `U` `I` | A B X Y | |
| `Y` `B` | LB RB | |
| `T` `V` | LT RT | reported as fully pulled; a key has no analog range |
| `'` | **L3** | long-range pathfinder toggle |
| `/` | **R3** | guide to target — context sensitive, routes to the selected destination with obstacle avoidance |
| `=` `-` | Start, Back | |

`Escape` and `Enter` keep working natively and need no mapping.

### The right stick is modal

With the long-range pathfinder **off**, the right stick is the entity scan.
With it **on** (L3), the same stick drives the catalogue instead:

| direction | |
|---|---|
| left / right | previous / next item within the category |
| up / down | previous / next category |

**One axis at a time.** A push that is not clearly horizontal or vertical must
cycle nothing: pick the dominant axis and require it to beat the other by a
margin, so a slightly diagonal push cannot change the category and the item in
the same gesture. This matters more here than for the scan, because the two
axes do different jobs and an accidental category change loses the player's
place in the list.

This replaces the `[` `]` and `,` `.` cycling keys: with the keyboard owning a
simulated stick there is no need for four more keys, and no room on the pad
for them either.

**The mapping now covers all sixteen pad inputs.** `=` is Start and `-` is
Back: in controller mode D2R's own keyboard scheme is gone, so a pad input with
no key is a game function the player cannot reach at all. `=` duplicates what
`Escape` already does natively; `-` is the only way to Back.

Still free, and staying that way: `X`, `[`, `]`. `\`, `,` and `.` are taken —
the map survey registers them as unmodified SDK actions. Every other action the
mod registers carries `Control`, so none of them contend with a pad key.

## Does D2R read a slot above 0? Measured: yes

Critical rule 3 forbids both merging with a physical controller and speaking
over it, so the synthetic pad has to take a free slot — which only works if
the game looks at one. The pass-through counter answered it: over thirty
seconds D2R made **1650 calls on slot 0 and six on each empty slot**, a hotplug
rescan about every five seconds. A pad that appears on a free slot is found.

So the free-slot design holds and the last-input-wins fallback that was drafted
for the other outcome is not needed. It is recorded here only so nobody
re-derives it: it would have passed the real pad through whenever it was
connected *and* being used, reporting keyboard state only while it was at rest.

## OPEN: movement does not reach the game

Reported by the user, 2026-09-19, with keyboard control enabled: **movement
does not work, while the rest of the mod commands do.** The mod-side features
respond to the pad -- the long-range pathfinder toggles on L3 and the
catalogue cycles on the right stick, both visible in that session's log -- so
`InputDirection` is reading the synthetic frame. What is not established is
whether D2R itself is reading it at all.

Do not theorise about this without the poll census. Enabling injection now
restarts it, so the log reports per-slot counts at +5 s, +15 s and +30 s from
the toggle:

- a slot the game has **accepted** is polled every frame, so thousands of calls
  in thirty seconds;
- a slot it has **not** is polled only on the ~5 s hotplug rescan, so single
  digits.

That one number separates "the game never took the device" from "the game took
it but ignores the left stick", and those need completely different fixes.

## Both pads connect, and the game still uses one

Measured, 2026-09-19. With the synthetic pad on slot 1 and a real controller
on slot 0, the poll counts are identical and both answer connected:

```
[pad] polls by slot -- 0: 611 calls/611 connected, 1: 611 calls/611 connected
```

So nothing was merged and nothing was spoken over -- slot 0 passed through
untouched, exactly as designed -- and the player's controller still stopped
working. D2R picks one pad and ignores the other, and it picked ours. Being
present is enough; reporting an idle state on our slot does not hand control
back.

That kills the "both at once" design. Neither half of critical rule 3 is
violated by the code any more, but the outcome the rule exists to prevent
happens anyway, inside the game. Until D2R's own pad-selection is understood
the honest answer is a toggle, not parity:

- `[keyboard] enabled` stays **false** by default: controller play is what
  works out of the box.
- **Ctrl+Shift+F12** turns the simulated pad on and off at any time.

Why that combination is read raw rather than registered as an input action:
entering controller mode is what disables the action system, so an action
bound to it would work exactly once and then never again -- it would be able
to turn the pad on and never off.

**This is the first thing to revisit once the decompile has names.** D2R's pad
selection is a function we could read, and if it picks by recency or by slot
there may be a way to be present without displacing a real device.

## Choosing the slot is a race, and losing it is the worst outcome

`Install` runs at plugin load, **before the game has finished enumerating
controllers**. The first version asked XInput once at that moment and latched
the answer, on the reasoning that asking again later would see our own pad. It
would not: the original is the trampoline to the real `XInputGetState`, which
never sees our injection, so it is always the true hardware picture and is safe
to ask at any time.

The latch was a live bug. Lose the race and slot 0 reads as empty, we take it,
and from then on the detour answers for the slot the player's physical
controller is on -- never calling the original for it, so that controller's
input is discarded entirely. That is precisely what critical rule 3 forbids,
and it is what happened: keyboard play worked perfectly and the pad stopped
existing.

How close the race is, from one launch: `[input] xinput9_1_0 pads connected: 0`
at 12:15:04.948, and 208 ms later `ChooseFreeSlot` found slot 0 occupied and
picked slot 1. Two hundred milliseconds decided whether the player kept their
controller.

The choice is now re-checked once a second against the original, and the detour
refuses a slot that is no longer ours between checks. A controller plugged in
mid-session, or enumerated after we installed, takes its slot back immediately.

## The injection point, already settled

From the live log, before the mod touched XInput itself:

```
[input] input modules at plugin load: xinput9_1_0
[input] xinput9_1_0 exports XInputGetState=yes, ordinal 100=no
[input] xinput9_1_0 pads connected: 0
```

The game reads the pad through **`xinput9_1_0.dll`**, and that module does
**not** export the undocumented ordinal 100, so hooking the named
`XInputGetState` alone reaches every call the game makes.

## The title and the pad (2026-10-13) {#the-title-and-the-pad}

Tester: a pad press does not pass "Press Any Key to Begin" in the modded game. Static reading found no
cause in our pad path: the XInput filter (`pad_filter.cpp`) changes nothing outside play (`claimed` is
false), and the mod's own poll uses xinput1_4 while the game reads xinput9_1_0. The fallback Space
(`modal_reader.cpp`) now waits 400 ms, not 1000, so a press the game takes is never followed by it, and
logs `[modal] title: pad press seen`, `... the game took the pad press in N ms`. New: `[pad] the game
polled slot N with buttons 0x... outside play`: if the title ignores the pad and that line is missing, the
game never saw the button (slot or device problem); if present, the game saw it and ignored it.
The version announcement is queued behind the title's own (`ModalReader::SetTitleAnnouncement`), not
spoken at load where the game's window read cut it off.
