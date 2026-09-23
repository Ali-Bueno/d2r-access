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

**Poison:** the health orb's own native color branch checks player state id 2.
The mod calls that same verified checker and announces entering and leaving
poison once. An unreadable or dead player does not count as poison ending;
the on-demand vitals report includes poison while it is active. See
`docs/combat-status-evidence.md` for the native call path and its build guard.

**Mercenary:** the hireling panel's native relation identifies its unit, from
which the same current and maximum life stats yield a percentage. It shares
the player's step schedule, takes a new baseline for each unit id, and queues
its alerts after player speech. An unloaded unit is unknown, not a death.
The report key includes current mercenary life when available.

**Breaking**: an item whose durability reaches zero from a positive value is
announced with its equipment slot. An item with no maximum durability never
breaks, an item worn already broken is not a break, and an item that stays
broken is announced once. Repairing and breaking again is announced again.
Breakage speech queues so it cannot erase a critical life alert from the same tick.
