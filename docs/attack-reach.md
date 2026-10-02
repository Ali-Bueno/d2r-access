# Attack reach and the monster plate

Two answers to one report (2026-10-02): the plate's names were spoken over and
over, for monsters the player could not even hit, and a ranged player wasted
arrows shooting blind. A friend's suggestion completed it: Diablo 1's monsters
announce themselves, and a sound when the target is in reach would do the rest.

- Every enemy now has a presence cue: the three nearest ordinary enemies growl,
  each on its own, as in Diablo 1's diablo-access ([entity cues](entity-cues.md)).
- A **reach ping** says when an enemy in sight comes within reach of the
  player's attacks.
- The **monster plate** names a monster only within that reach, and never
  repeats a name the auto-target keeps trading.

## How far the attacks reach

`AttackReach::Classify` (`src/attack_reach.cpp`) decides melee or ranged from
the strongest evidence first:

1. a bow or crossbow in the active hands (ItemTypes.txt children of `miss`);
2. a bound skill whose skills.txt `Range` is `rng`;
3. a bound skill whose `Range` is `h2h`;
4. a bound skill with `Range` `none` that carries its own damage (`EType`,
   `EMin` or `MinDam` set): Fire Bolt, Bone Spear, Blizzard, Blessed Hammer;
5. otherwise melee (only `both` skills such as Attack, with a melee weapon).

"Bound" means both controller bars of the current weapon set
(`NativeSkillBar::BoundSkills`, the same getter the LT bar reader uses). Ranged
reaches `ranged_reach` (default: the entity cue range, 15 subtiles, so a shot
reaches every enemy the cues play). Melee reaches `melee_reach` (3 subtiles, a
monster at contact) plus the weapon's `rangeadder`. Both are sound-design
defaults, not game constants: the game decides contact from unit sizes this
mod does not read. `attack = "melee"|"ranged"` or the `attack-melee` /
`attack-ranged` commands fix the style when the rule guesses wrong.

Known misreads, each fixable with the fixed mode: a whirlwind barbarian with War
Cry bound reads ranged (War Cry carries damage); a paladin with Zeal and Holy
Bolt reads ranged.

### Game columns

Found by exhaustive correlation of every byte offset against every row of the
extracted tables in `game-data/global/excel/` (2026-10-02). Each offset is the
only one that fits; the in-memory rows share the `.bin` layout (the skill
description index at `+0x23C` and the item code at `+0x80` were already read
live at the same offsets).

| Table (stride) | Column | Offset | Width | Rows confirming |
|---|---|---|---|---|
| skills (0x2EC) | `Range`: none 0, h2h 1, rng 2, both 3, blank 0xFF | +0x34 | u8 | 429/429 |
| skills | `MinDam` | +0x250 | i32 | 429/429 |
| skills | `EType` (ElemTypes.txt row, 0 none) | +0x284 | i32 | 429/429 |
| skills | `EMin` | +0x288 | i32 | 429/429 |
| weapons (0x1C0) | `rangeadder` | +0x115 | u8 | 306/306 |

Missile columns were rejected as an attack test: Charged Bolt, Nova and Blizzard
keep their missile in `srvmissilea`, and Battle Orders has one too.

## The reach ping

`AttackReach::Tracker` pings for the nearest enemy in sight within reach when
none was, then stays quiet until every enemy is beyond reach plus `release`
(1 subtile), so one pacing at the edge does not ping on every step. A menu or a
level change resets it: closing a menu with an enemy still in reach pings again.
The ping is a 96 ms rising two-note sine (1320 then 1760 Hz), far above every
other cue, placed and pitched like them. A pending ping is never replaced by a
presence pulse. Log: `[attack-reach] style=… reach=…` on every change of style,
and `[attack-reach] in reach id=… distance=…` per ping.

## The monster plate

The game's controller auto-target hops between a pack's monsters several times
a second. The 2026-09-29 log shows the plate trading *Trinchante* and *Chamán
trinchante* every 160 ms to 1.1 s for ten seconds, each one interrupting the
last, with the shaman fifteen subtiles away.

`MonsterNames::Gate` (`src/monster_names.cpp`), polled by the interaction
reader for the `HUDMonsterHealth` plate and for world labels naming an enemy:

- With a melee attack, a monster beyond reach is not named. Bosses and elites
  are named from first sight. A label no unit carries, or an unknown reach, drops
  only this rule. A ranged attack drops it too
  (`EntityCueService::NamingReach`). The game's auto-target already picks what a
  shot can hit, and `ranged_reach` is a sound-design default, not the spell's
  range. Live 2026-10-02: a sorceress killing with Glacial Spike from 20–23
  subtiles heard 5 names, 11 held back as out of reach.
- A name back on the plate within `repeatMs` (10 s) of its last showing stays
  silent; the trading itself keeps refreshing that, so a pack's names stay quiet
  for as long as the trading goes on. Only an uninterrupted display, poll after
  poll, counts as the one already admitted.
- An admitted name is spoken at once, without the reader's 80 ms settle. The gate
  marks a name heard when it admits it, and the auto-target hops faster than the
  settle. Live 2026-10-02: a Fallen joined a rat on the plate for under 80 ms, the
  target hopped back, and the Fallen was never named.
- Monster names never interrupt.
- The plate's affix line (`• Invoca trinchantes`) is spoken after a comma. Its
  stray digits were the font command's index character, which `UiText::Clean`
  now drops with the command ([native tooltips](native-tooltips.md)).

The bearer's distance comes from `TargetSight::Find`, the nearest unit in sight
that carries the label's name ([target sight](target-sight.md)).

## Controls

`access-entity-cues reach-on|reach-off|attack-auto|attack-melee|attack-ranged`;
`status` ends with the reach ("Alcance a distancia, 15 pasos, automático").
`radar.toml` `[entity_cues]`: `reach`, `attack`, `melee_reach`, `ranged_reach`.

## Validation

Automated: `attack-reach` (style rules, reach, sight, the ping's hysteresis),
`monster-names` (the 2026-09-29 trading pattern, flicker, edge, clock),
`interaction-reader` (plate gating end to end), `entity-cue-service` (loadout
cadence, ping before growl, commands), `entity-cue-audio` (the ping's band,
rise, placement). Live listening is pending: approach a pack with a bow and with
a melee weapon, and check the plate stays quiet while the target trades.
