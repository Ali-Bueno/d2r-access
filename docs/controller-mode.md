# Controller mode, and why keyboard play goes through it

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

**Inject from startup.** Controller mode is entered by the first synthetic
frame and never left, so the game should be in it from the moment the plugin
loads rather than switching mid-session — a player must not be left between
modes with neither input working.

**Everything the keyboard used to reach must now be reachable as a pad input.**
The keyboard scheme is gone, so inventory, quest log, character panel and skill
tree are only reachable the way a controller reaches them. The mapping has to
cover the pad's full button set, not just the ones gameplay uses.

## The mapping

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

## Still to measure

Whether D2R reads a pad on a slot above 0. The user has a physical controller
on slot 0, and critical rule 3 forbids both merging with it and speaking over
it, so the synthetic pad has to take a free slot. If the game only ever reads
slot 0, the documented fallback is last-input-wins on slot 0 — pass the real
pad through untouched whenever it is connected *and* being used, and report
keyboard state only while it is at rest.

This is a measurement, not a guess: hook `XInputGetState` as a pass-through
that only counts calls per slot, and read the counts.

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
