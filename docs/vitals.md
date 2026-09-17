# Life, mana and broken equipment

What the mod says about the player's own condition: life and mana as they cross
announcement steps, and a worn item that breaks. Console command
`access-vitals on|off|status`; enabled by default.

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
`OptionsMenuShowLifeOrbText` / `OptionsMenuShowManaOrbText`). **With both off
the game writes no number anywhere**, so there is nothing to read; the mod then
says once, in the player's language and using the game's own label text, that
those options need turning on.

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
- At or below `Vitals::CriticalPercent` (10 %) the announcement **interrupts**;
  above it, it waits its turn.
- Leaving the game (no `HUDPanel`) drops the baseline, so the next character
  starts fresh.

Sampling: orbs every 100 ms (life can fall within one blow), durability every
second (it only creeps down).

**Breaking**: an item whose durability reaches zero from a positive value is
announced with its equipment slot. An item with no maximum durability never
breaks, an item worn already broken is not a break, and an item that stays
broken is announced once. Repairing and breaking again is announced again.
