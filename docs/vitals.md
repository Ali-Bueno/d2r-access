# Life, mana and broken equipment

What the mod says about the player's own condition: life and mana as they cross
announcement steps, mercenary life, poison state changes, and a worn item that breaks. Console command
`access-vitals on|off|status|report`; enabled by default.

## Where the numbers come from

**Life and mana: the game's own HUD orbs.** `HUDPanelHD.json` declares
`HealthBall` and `ManaBall` (`AttributeBallWidget`), each with a child
`ValueDisplay` (`TextBoxWidget`) that the game fills with `current/maximum`
while the matching gameplay option is on. `NativeWidgets::TextAt("HUDPanel",
{"HealthBall", "ValueDisplay"})` reads it with the existing native text-box
reader, so no new offset, fingerprint or stat id is involved and the values are
exactly the ones on screen.

The two options are `ShowLifeOrbText` and `ShowManaOrbText` in the Gameplay
panel (`src/generated/panel_widgets.h`, label keys
`OptionsMenuShowLifeOrbText` / `OptionsMenuShowManaOrbText`). If either display
stays unreadable for ten 100 ms samples, the mod names exactly the corresponding
game option. A single unreadable sample does not imply that the option is off.
An explicit `report` reads whichever value is available and identifies any
unavailable display immediately.

**Durability: the loader's inventory service.** `getLocalPlayer` +
`forEachInventoryItem` filtered to `ItemContainer::Equipment` return an
`ItemInfo` per worn item carrying `durability`, `maximumDurability` and
`bodyLocation` (`external/PluginSDK/include/D2RLPlugin/item.h`). `inventory.h`
permits both on the UI thread, which is where the plugin's tick runs.

## Walking, running, stamina and ammunition (2026-10-11)

A tester: running drops defense to zero, so the walk/run state is tactical, and stamina was never
mentioned. Behind the `movement-alerts` switch (Ctrl+S, on by default):

- **Gait.** L3 (the game's toggle) predicts the flip and speaks "Running"/"Walking" at once, with
  interrupt. The player unit's mode then confirms or corrects it: walk 2 and town-walk 6 are
  walking, run 3 running (`GroundItemLayout::Walk/Run/TownWalk`). The first step of a game is a
  silent baseline; the toggle flag itself is not read, so a toggle before any step says nothing,
  and the keyboard's run key is only heard through the next step.
- **Stamina** is `GetStat` 10 over 11 (`NativeStatus::PlayerMovement`); the ratio holds whatever
  the fixed-point scale. Falling through 50, 25 and 10 percent is spoken, and "Out of stamina" at
  zero; a step re-arms once stamina recovers 10 points above it (`Vitals::StaminaMeter`). BACK+X
  adds stamina when it is not full. `[vitals] movement` logs the raw values on each alert.
- **Ammunition** (`ammo-alerts`, on): a stack in hand (seen holding more than one) falling to
  50, 20, 10, 5 or 0 is spoken as "Arrows: 20 left" (`Vitals::AmmoMeter`, quantity from the
  loader's `ItemInfo.quantity`). BACK+X adds every stack in hand.

Live checks pending: the gait mapping in town and with stamina exhausted, and the stamina values.

## What is announced

`src/vitals.cpp` holds all of it, free of game and speech dependencies, and
`tests/vitals_tests.cpp` (CTest suite `vitals`) covers it.

- **Steps** (`Vitals::Steps`): 75, 50, then 40, 30, 20, 10, then 5, 4, 3, 2, 1 —
  25 points while the resource is comfortable, 10 points below half, single
  points at the end. A resource above the first step is one band, so ordinary
  chip damage stays silent.
- Crossing a step speaks the **real percentage**, not the step: falling past 75
  at 74 % says 74, not 75. Percentages are floored, never rounded up into a
  safer number.
- A step is announced **once**, in either direction; recovering across steps is
  announced the same way. The first reading of a character is only a baseline.
- Life and mana crossing steps in the same sample form one utterance, with life
  first. A downward crossing at or below `Vitals::CriticalPercent` (10 %)
  interrupts; recovery and other readings wait their turn. This keeps a
  critical mana reading from erasing a simultaneous critical life reading.
- Leaving the game (no `HUDPanel`) drops the baseline, so the next character
  starts fresh.

Sampling: orbs every 100 ms (life can fall within one blow), durability every
second (it only creeps down).

**Equipped items (2026-09-25):** the same one-second census of worn items
(`Vitals::Equipping`) announces a runtime id that was not worn in the previous
census: "Equipado en <slot>: <base name, quality>", queued. It covers every way
of equipping (by hand, the controller's equip action, swapping one item for
another). A weapon swap only moves ids already worn, and the first census of a
game is the baseline, so neither speaks. The name is the item's table name and
quality (`NativeGroundItems::ClassLabel`, the ground items' reader), not a
generated unique/rare name. Placing an item by hand still says "Colocado en
<slot>" first. Live check pending.

**Poison:** the health orb's own native color branch checks player state id 2.
The mod calls that same verified checker and announces entering and leaving
poison once. An unreadable or dead player does not count as poison ending;
the on-demand vitals report includes poison while it is active. See
`docs/combat-status-evidence.md` for the native call path and its build guard.

**Mercenary:** the hireling panel's native relation identifies its unit. The
panel's own percentage getter supplies its life reading; raw current and
maximum life stats do not form a valid fraction on this client. It shares the
player's step schedule and critical interrupt rule. Simultaneous player and
mercenary changes form one utterance, player first. The panel's level stat
supplies a diff-gated level-up announcement. Each unit id takes fresh life
and level baselines; game lifecycle events reset them even if the HUD panel
persists between sessions. An unloaded unit is unknown, not a death. The report key
includes current mercenary life and level when available. Native evidence is
in `docs/combat-status-evidence.md`.

**Breaking**: an item whose durability reaches zero from a positive value is
announced with its equipment slot. An item with no maximum durability never
breaks, an item worn already broken is not a break, and an item that stays
broken is announced once. Repairing and breaking again is announced again.
Breakage speech queues so it cannot erase a critical life alert from the same tick.
