# Assassin charges (2026-11-02)

A tester (bfbeast) asked for the Assassin's martial-arts charges to be spoken. This page
covers the mechanics, how the mod reads them and what it says.

## Mechanics

From [Arreat Summit, Martial Arts](https://classic.battle.net/diablo2exp/skills/assassin-martialarts.shtml)
and the game data (`game-data/global/excel/skills.txt`, `states.txt`):

- **Charge-up skills** (`progressive=1`): Tiger Strike 254, Fists of Fire 259, Cobra Strike 265,
  Claws of Thunder 269, Blades of Ice 274, Royal Strike 280 (Phoenix Strike in classic). Each hit
  adds a charge, up to 3. Different skills' charges coexist on the character.
- **Release** (`finishing=1`): Attack 0, Left Hand Swing 5, Dragon Talon 255, Dragon Claw 260,
  Dragon Tail 270, Dragon Flight 275. Dragon Talon releases charges on its first kick only.
- `prgstack=1` (Fists of Fire, Claws of Thunder, Blades of Ice): releasing fires every level reached.
  Without it (Tiger, Cobra, Royal), releasing applies only the current level.
- **Expiry:** charges last "until triggered or until the time limit runs out" (Arreat Summit).
  `auralencalc` is 375 frames (15 s) for each. Unverified whether a new hit renews the timer.
- **What a sighted player gets:** an overlay per level (`states.txt` `pgsvoverlay`
  `progressive_*_1`) and a sound per level (`sounds.txt` `assassin_chargeup_<skill>_1..3`, rows
  2282-2299).

## Reading

| skill | state (states.txt) | count stat (ItemStatCost) |
|---|---|---|
| Tiger Strike | `progressive_damage` 122 | 318 |
| Cobra Strike | `progressive_steal` 123 | 319 |
| Royal Strike | `progressive_other` 124 | 320 |
| Fists of Fire | `progressive_fire` 125 | 321 |
| Blades of Ice | `progressive_cold` 126 | 322 |
| Claws of Thunder | `progressive_lightning` 127 | 323 |

`progressive_tohit` is 325. The stats have `Send Bits=3`, so the client receives them.
`game-data/global/excel/ItemStatCost.txt` was pulled from the installed game with
`build/casc-extract/casc_extract.exe extract "<game dir>" <out> "excel\\itemstatcost.txt"`.
`tools/gen_skill_states.py` writes each charge-up skill's stat into `src/generated/skill_states.h`
as `charges`.

## What the mod does

`VitalsService` reads the stats every 100 ms with the orbs, through `NativeStatus::PlayerStats`
(`GetStat` `0x2F5020`, as stamina does). `ChargeTracker` (`src/charge_tracker.h`) compares each read
with the one before:

- **More charges** cut in as "<skill> <count>", e.g. "Tiger Strike 2". The interruption keeps a
  quick string of blows from piling up, and it still queues behind held status news.
- **Charges falling to none** within `ReleaseWindowMs` (500 ms) of a swing are silent: the
  player released them. A swing is player mode A1, A2, TH, KK, S1-S4 or SQ.
- **Charges falling to none at any other time** ran out, and "<skill> charges lost" waits its turn.

BACK+X says "<skill> charges: <n>" in place of "<skill> active". The switch is `charge-alerts`
(Alerts, on). Its log lines are `[vitals] charges -> "<gained>" "<lost>"`, and once per game
`[vitals] charge state <row> on, stat <row> reads <n>`.

**Unverified live:**
- whether `GetStat` on the player counts the stat held in the state's own stat list, and whether
  the value is the charge count (1-3). The second log line answers both.
- whether a hit at 3 charges renews the 15 s timer.

If the stat reads 0 while the state is on, fall back to the state alone. It shows whether charges
are present, but not how many.
