# Weapon swap

A weapon swap (R3 on the controller, Y on the keyboard, the inventory's I/II tabs) says the
weapons now in hand: "Long Bow, Quiver: 250 left", or "no weapons" for an empty set. A stack in
hand (arrows, bolts, throwing weapons) says how many it holds. The set number ("Weapon set 2, …")
leads with the `weapon-set-number` switch in Ctrl+S, on by default since 2026-10-12 (a tester
asked for it; the user had left it out on 2026-09-27). It interrupts, because it answers the button
just pressed. `[weapon_swap] enabled` in the
settings file sets the default; `access-weapon-swap on|off` toggles it for the run.

Code: `src/native_weapon_set.*` (the flag), `src/weapon_swap.*` (when to speak, the phrase,
tested in `tests/weapon_swap_tests.cpp`), `src/weapon_swap_service.*` (100 ms sampling),
`src/equipment_census.*` (the worn-item census shared with `vitals_service.cpp`).

## When it speaks

- The active set is a flag, sampled every 100 ms with the hands (body locations 4 and 5).
- The first reading in a game is the baseline: a game joined on set II says nothing. Out of a
  game (no local player) the tracker resets.
- A flag change whose hands already differ from the previous sample is spoken at once (a swap
  done on this client moves the items and flips the flag in one call).
- Otherwise it waits for the hands to change, or `ItemMoveGraceMs` (500 ms) — the server may
  send the items and the flag in separate replies, and two empty sets have nothing to move.
- Names are the item table name and quality (`EquipmentCensus::Name`), the same as the
  "Equipado en" announcements, not generated unique or rare names.

## Native evidence (D2R 3.3.93787, RVAs from image base `0x140000000`)

Ghidra decompiles of `re/D2R.rebuilt.exe`; every function below is fingerprinted in
`native_weapon_set.cpp`.

- **The flag is not on the player unit.** It is a bool in a client key/value registry:
  `0x8461C0()` returns `*(.data 0x2E401A0)` (read directly from the `mov rax,[rip+disp32]` at
  `+4`, because the accessor asserts when the global is empty). `0x11C860(registry, key)` looks
  the key up (murmur finalizer hash) and returns `entry->[+0x10]->[+0x110]` as a bool, 0 when
  absent. 0 = set I, nonzero = set II.
- **Key** `0xF2D7CF8E9CC08212`, the `movabs rdx` immediate at `0x1195C0 + 0x5D`; the plugin reads
  it from there. `0x1195C0` is the skill bar's set offset (16 on set II), but it returns 0 without
  separate skill bindings or under the `0x8D3B0` mode gate, so it is not the set.
- **Readers:** the inventory panel update `0x22BA70` colours its I/II tabs by this flag
  (`0x22C4CE`); `0x2C9940` and `0x2CA0B0` shift inventory positions by it.
- **Writers:** `0x15B920`, the local swap, removes the items at 4, 5, 11, 12 and re-places them
  (4<->11, 5<->12), then toggles the flag. `0x12E1E0` toggles it without a static caller (the
  server reply, inferred). `0x1F0120` saves it to `0x2A96B4C` when the trade window opens, and
  `0x1EDC50` (trade completed, `docs/trade.md`) and `0x1F33B0` restore it from there; a restore
  that changes the set is announced like any swap.
- **Items move.** `0x1E33A0` (the swap entry the key, controller and UI handlers call) reads
  body locations 4, 5, 11, 12 through `INVENTORY_GetItemFromBodyLoc` (`0x3886D0`); on a local
  game it calls `0x15B920`, otherwise it sends the four GUIDs to the server, whose handler
  `0x4BEA50` makes the same moves (`0x4BF30F`..`0x4BF3D4`). Runtime ids do not change, so after
  any swap 4/5 hold the weapons in hand and `Vitals::Equipping` stays silent.

## Live check pending

Swap with two filled sets, with set II empty, and with both empty; the log shows
`[weapon-swap] set=… right=… left=…` and `[equip] worn` with `11:`/`12:` entries.
