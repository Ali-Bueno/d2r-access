# Life, mana and worn equipment

What the mod says about the player's own condition: life and mana as they cross
announcement steps, mercenary life, poison state changes, and a worn item running low or breaking. Console command
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

**When the game hides that text: the stats (2026-11-03).** A player of the public release: BACK+X
silent at every start, the options checked, and speech back only after opening them, without
changing anything. The text was the mod's only source, so the alerts and the low-life sound were
silent too. Why the game hid it was not found (no log); `Settings.json` keeps the options as `Show
HP Text` and `Show MP Text`, and the launcher can write a file without them (a suspect, not tested).
An orb whose text cannot be read now falls back to `NativeStatus::PlayerOrbs`: `GetStat` 6 over 7
and 8 over 9, in 1/256ths, whose ratio is the percentage, as with stamina. The hint above now means
both sources failed. The text stays first, so nothing changes where it works. Log, once a game:
`[vitals] orbs: text life=N mana=N, stats life=N mana=N` (-1 for text the game is not showing).
MEASURED 2026-11-04 with both options off in `Settings.json`: no text on the orbs in the capture, Z and X
said 100 per cent. The stats agreed with the screen, 203 of 337 life said as 59 and rising. The log's
first sample of any game has `text life=-1`: the orbs' text is not drawn yet then, options on or off.

**Durability: the loader's inventory service.** `getLocalPlayer` +
`forEachInventoryItem` filtered to `ItemContainer::Equipment` return an
`ItemInfo` per worn item carrying `durability`, `maximumDurability` and
`bodyLocation` (`external/PluginSDK/include/D2RLPlugin/item.h`). `inventory.h`
permits both on the UI thread, which is where the plugin's tick runs.

## Walking, running, stamina and ammunition (2026-10-11)

A tester: running drops defense to zero, so the walk/run state is tactical, and stamina was never
mentioned. Behind the `movement-alerts` switch (Ctrl+S, on by default):

- **Gait.** L3 (the game's toggle) flips it and speaks "Running"/"Walking" at once, with
  interrupt; nothing else speaks it. The player unit's mode (walk 2 and town-walk 6 walking, run 3
  running, `GroundItemLayout::Walk/Run/TownWalk`) only re-syncs the toggle in silence, after three
  samples in a row, never while the wall-slide hook counts refused pushes or stamina is at or under
  10 percent: the game walks by itself against a wall and when tired (`controllerskillsettings`
  `forceWalk*`), which a tester heard as the game toggling (2026-10-12). The toggle flag itself is
  not read, so a toggle before any step says nothing.
- **Stamina** is `GetStat` 10 over 11 (`NativeStatus::PlayerMovement`); the ratio holds whatever
  the fixed-point scale. Falling through 50, 25 and 10 percent is spoken, and "Out of stamina" at
  zero; a step re-arms once stamina recovers 10 points above it (`Vitals::StaminaMeter`). BACK+X
  adds stamina when it is not full. `[vitals] movement` logs the raw values on each alert.
- **Ammunition** (`ammo-alerts`, on): a stack in hand (seen holding more than one) falling to
  50, 20, 10, 5 or 0 is spoken as "Arrows: 20 left" (`Vitals::AmmoMeter`, quantity from the
  loader's `ItemInfo.quantity`). BACK+X adds every stack in hand.

Live checks pending: the gait mapping in town and with stamina exhausted, and the stamina values.

## Mana alerts (2026-10-12)

A tester: casting made mana spammy and it drowned out life. `mana-alerts` in the Ctrl+S list has
three values (`Vitals::ManaAlerts`): every change (as life, the default), low mana only (a fall to
20 percent or under, `ManaLowPercent`, never a recovery) and off. BACK+X still reads mana.

## What is announced

`src/vitals.cpp` holds all of it, free of game and speech dependencies, and
`tests/vitals_tests.cpp` (CTest suite `vitals`) covers it.

- **Steps** (`Vitals::Steps`): 75, 50, then 40, 30, 20, 10, then 5, 4, 3, 2, 1 —
  25 points while the resource is comfortable, 10 points below half, single
  points at the end. A resource above the first step is one band, so ordinary
  chip damage stays silent.
- **Life 0 says nothing** (`Vitals::Alerts::Update`): it is death, and the game's
  death screen ("You have died...", interrupting) says it. "Life 0 percent", interrupting
  60 ms later, cut that screen off (a tester's log, 2026-10-25). The meters still take the
  0, so the respawn's full life is measured from it.
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

**Durability** (a tester, 2026-11-03: the game's paper doll at the top right is seen only). The
mod follows the doll's own rule, read in `DurabilityWidget`'s update (`0x14E1A90`, slot rule at
`0x14E1BC4`): `damage = 1 - durability / maximum`, the doll appearing at `mediumDamageFraction`
0.8 (`requiredDamageLevel` "medium" in `HUDWarningsHD.json`), so a fifth of the maximum left or
less is low (`Vitals::Wear::Low`), and zero is broken. Skipped as the game skips them: no maximum
(indestructible), stacks (ammunition has its own warning; a stack down to one reads as one item,
since `ItemInfo` has no stackable flag), and the weapon set not in hand (body locations 11, 12).
Which of the two levels the doll paints yellow or red was not found in the draw code.

An item getting worse while worn is spoken with its slot, "Low durability: Head", "Broken
equipment: Head", once per level; an item that stays low is silent, a repair re-arms it, an item
put on already worn says nothing. `durability-alerts` in Ctrl+S switches it; the speech priority
entry **Durability alerts** defaults to "cannot be cut off" (the tester's request; breaking used
to queue). The sweep runs before the orbs in the tick, so a critical life alert of the same tick
queues behind it instead of being erased. BACK+X always adds what the doll shows now, right after
the experience, with the alerts off too; the keyboard's Z and X read life and mana alone, so it has
no such line. Log: `[vitals] worn item low|broken at body location N`.

## Experience (2026-10-13)

A tester: the bar between the orbs is always in a sighted player's view. The
life report (BACK+X, Mod+X) ends with "Experience N percent", how far into the
level as the bar fills (`experience-report`, Ctrl+S, on). Static RE of the HUD
bar's updater 0x14CC8C0: experience is stat 13, the thresholds come from
experience.txt through 0x300920(expansion 0x34A0E0, class 0x349860, level - 1
and level), below the cap 0x300C70. At the cap nothing is said.

The controller's mercenary panel draws only a bar (its CurrentXP/NextLevelXP
boxes are commented out in `Controller/HirelingInventoryPanelHD.json`), so the
panel's experience line is the same percent: the hireling panel 0x14F3400 reads
stat 13 and the next threshold stat 30, and a level starts at
ExpPerLvl * L^2 * (L + 1), so ExpPerLvl = next / ((L + 1)^2 * (L + 2))
(`Vitals::MercenaryLevelStart`). Live check: `[vitals] experience level=1
total=... start=0 next=500` for a new character; the percent matches the bar.

### "Mercenary says:" (2026-10-13)

The mercenary's level-up line (`merclevelup`, "I feel much stronger now") was named
only in the notices, but the tester's log has it three times as `[interaction] World
target -> "I feel much stronger now"`: the game draws it as the world target's tooltip.
`GameTextReader::NameSpeaker` now names it on both paths (`mercenary-bark`, on); the
log line to check is the same `World target` one, now starting with "Mercenary says:".

## Low life heartbeat (2026-10-15)

A tester died twice before the spoken life percentage caught up. At or under `low-life-below`
(5 to 70 percent of life, 25 by default: a Sorceress and a Barbarian need different lines) the
Diablo IV low-life sound plays once and its heartbeat loops (`sounds/*/vitals/`, the author's
Last Epoch mod's copies), until life climbs `Vitals::LowLife::ReleasePoints` (5) over the line, so
a regeneration tick at the line does not start it again. No life (dead) or an unreadable orb is
quiet, and so is a pause or a dialogue. `LowLifeAudio` is its own XAudio2 voice with its own
volume (`low-life-volume`, Volumes), under the mod-sounds master switch. Switch `low-life-sound`
(Alerts, on). Logs: `[vitals] low life`, `[low-life] heartbeat started|stopped`.

2026-10-17: twice as fast (the tester: drowned in heavy combat). The loop's ten beats, 1.16 s
apart (52 a minute), were each cut to their first 0.58 s, lub-dub included, with a 40 ms fade:
103 a minute, each beat still its own. A faster playback rate was rejected (an octave higher).

The life report's experience now names the level it leads to: "Experience 34 percent to level 17".
