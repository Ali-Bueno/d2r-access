# Controller auto-target and melee reach (RE)

Research of 2026-10-22, after the tester's report on the reach ping: it fired from mid-screen
for a Warlock and a Sorceress alike, ignored how many enemies were near, and the pad's
auto-target "locks onto the closest thing and ignores anything behind it". Goal: replace the
mod's sound-design distances ([attack reach](attack-reach.md)) with the game's own rules.

Evidence: D2R 3.3.93787, Ghidra on `re/D2R.rebuilt.exe` (`D2R_live` project); D2MOO
(github.com/ThePhrozenKeep/D2MOO, a 1.10 reconstruction) to recognise shapes; the tables
`game-data/hd/global/excel/controllersettings.json` and `controllerskillsettings.json`,
extracted from CASC `data/hd/global/excel/`. **READ** = seen in code or data; **INFERRED** =
derived, not seen. Nothing here is MEASURED live yet.

## The pad's auto-target

**Loop (READ).** Every frame `0xF1CC0` (from `0xF24C0`, once per local player) clears the
ControllerManager's per-category slots (`0x18B8C0`: id -1, score -1.0), walks every non-missile
unit in the player's room and its neighbours (`0x2EFDE0`), filters each with `0x9B840`, which in
controller mode scores with `0x1919F0`, picks the best (`0x1914B0` -> `0x191130`) and writes it
with `0xF2B40` into the hover array at `0x2A41630` ([target sight](target-sight.md)). It is not
written while the entry's lock byte (+1) is set (`0xF1A60` sets, `0xF15C0` clears) or when
`0xF2670` reports no pointer (`0x13DE60(0x13CE90())`; INFERRED true in pad play, as the same
test gates the movement-stick read at `0x14C62A`).

**Candidates (READ).** Not invisible (state 146, `0x3351B0`), not `0x34F8D0`, passes
`0x34FB00(u, 0x80)` and one relationship test (`0x971E0`, INFERRED hostility; `0x97040`;
`0x96E80`). A monster only if one of the 16 controller skill slots can be used on it
(`0x18B720` -> `0x9E950`). Its sprite box must be on screen (`0x152390`) and pass `0x7A2CA0`.
**No line-of-sight test**: the auto-target picks through walls.

**Direction (READ).** The stick vector at ControllerManager `+0x1954/+0x1958` (per player,
stride 0xB8), rotated into world axes at `0x14C6F4` and snapped by `0x143960` to the whole cell
11-13 subtiles out that best matches it. If either raw axis is exactly 0 the facing is used
(`0x349EF0`). Written in the movement handler `0x14C540`: the left stick (INFERRED).

**Score (`0x18AF30`, READ).** `d` = Euclidean distance in subtiles; `t = d / D`, rejected if
`t > 1`. `a = (1 - n·dir) / (1 - cos(Range/2))`, rejected outside [0, 1]. Curves (`*Calc`):
0 `1-x`, 1 `1-x²`, 2 `(1-x)²`. Blend (`WeightScaleCalc`): 0 `(1-W)·angle + W·dist`; 1
`max(min(2(1-W),1)·angle, min(2W,1)·dist)`. The strictly highest score wins.

Monsters, `primaryTargetingData.MonsterPerClass` (READ; `Range` 180, so ±90°):

| Classes | Base | D | W, blend | Score |
|---|---|---|---|---|
| Amazon, Necromancer, Druid | MiddleGround | 22 | 0.35, 0 | `0.65·cosθ + 0.35·(1-d/22)²` |
| Sorceress | Ranged | 22 | 0.6, 1 | `max(0.8·cosθ, (1-d/22)²)` |
| Paladin, Barbarian, Assassin | Melee | 20 | 0.463, 0 | `0.537·cosθ + 0.463·(1-d/20)²` |
| Warlock | — | ? | ? | no entry; loader defaults (35, linear, 0.5) INFERRED |

`classTargetingData` holds a second per-class set (35 subtiles, 0.5, or zeros); what reads it
was not traced. Other categories (READ): item and room tile 360°, 4, distance only; object
360°, 5.2; NPC 180°, 4; player, pet, ally 180°, 35; corpse never; player corpse 360°, 4.

Consequences (INFERRED from the formula): nothing more than 90° off the stick is a candidate.
A Sorceress aiming straight at a monster scores 0.8, which only a monster within 2.3 subtiles
or nearer the aim beats, so precise aim does pick the one behind. A Paladin's monster at 45°
and 3 subtiles (0.71) beats one dead ahead at 10 (0.65): melee classes are pulled to the near.

**No stickiness in pad mode (READ).** Slots are rebuilt every frame; the keep-the-current-hover
code and `0x144AC0`'s lock (`0xF1A60`) run only outside controller mode. Hence the plate's
trading between a pack's members ([attack reach](attack-reach.md#the-monster-plate)).
Free-target mode (`+0x199C` = 1) takes the unit nearest the reticle (`+0x184C/+0x1850`).

**A skill press (READ).** `0x144AC0` -> `0x18DD60` -> `0x18DDE0` takes the best per-category
slot within the skill's `targetGroup` (`controllerskillsettings.json`; group 0 = monster plus
category 10). The skills.txt `Range` column plays no part. `maxTargetDistance` (only Miasma
Chains, 20) and `maxTargetAngleOffset` (Leap, Leap Attack, 10°) were not traced. So the hover
entry is the best of every category, while an attack takes the best monster slot.

## Melee reach

`0x348650(attacker, target, bonus, int* outReach)` = D2MOO `UNITS_IsInMeleeRange` (READ): in
range when `dist <= 0`, or `dist <= MeleeRange + 1 + bonus` and the line has no collision of
mask 0x804 (`0x3502A0`). Tentacles (monster ids 258, 261): `dist < MeleeRange + 8`, no test.

**Distance** `0x325140` -> `0x325200` = D2MOO `D2Common_10399` (READ), in half-subtiles.
`dx`, `dy` integer subtiles; sizes are SizeX only, cached at unit `+0x180` by `0x3481E0`
(player 2, monster MonStats2 `SizeX`, object objects.txt `SizeX`, item 1). If `dx, dy < 8` and
both sizes < 4: `T[dy][dx]`, negative -> 0; -1 if either size is 3 (floor 0); +1 if either size
is 1 or less. Otherwise `s = s1/2 + s2/2`, `x = max(dx-s, 0)`, `y = max(dy-s, 0)`, distance
`2·max(x,y) + min(x,y)`. `T`, rows `dy` 0-7 (READ in D2MOO, byte-identical in `D2R.exe`; the
live pointer `0x1CFDA00` to it INFERRED):

```
-1 -1 -1  0  2  4  6  8 | -1 -1  0  1  2  4  6  8 | -1  0  0  2  3  5  7  8 |  0  1  2  2  4  5  7  8
 2  2  3  4  5  6  7  9 |  4  4  5  5  6  7  8  9 |  6  6  7  7  7  8 10 10 |  8  8  8  8  9  9 10 11
```

**MeleeRange** `0x34A620` (READ): player = the weapon's `rangeadder` (item record `+0x115`, via
`0x371F10`), **+2 for class 7, the Warlock**; monster = MonStats2 `MeleeRng` (`+0xE`), 255 ->
2 or 0 by weapon class. **Bonus** (client `0xFBDB0`, READ): `0x34A7D0`, 1 when the target moves
toward the attacker (INFERRED meaning), else 0; out of range, the client walks closer
(`0xFBEF0`). D2MOO's server damage check uses bonus 1 (player) / 3 (monster); not traced in D2R.

Example: no `rangeadder` against a size-2 monster strikes at `T <= 1`: (3,0), (3,1), (2,2)
yes; (3,3), (4,0) no. One `rangeadder` point is about half a subtile.

## Ranged reach

D2MOO's skill-on-unit handler (`PlrMsg.cpp` ~1012-1030, not traced in D2R) checks distance
only for melee; anything else is cast at once. So what a ranged pad attack can hit is what the
auto-target can pick: within `D` (20-22 subtiles for monsters) and on screen. Missiles fly
farther (D2MOO `Missiles.cpp`: `Range + lvl·LevRange` frames at `Vel·3/64` subtiles per frame;
values from blizzhackers/d2data, not D2R's own table): Fire Bolt 47, arrows, Magic Arrow and
Bone Spear 45, Lightning 35, Glacial Spike 30, Ice Bolt 28, Frozen Orb 14 subtiles.

## Visible extent

`0x152390` tests the sprite box against a pixel viewport whose size is at `0x22A0D68` (width)
and `0x22A0D6C` (height), widened by aspect ratio. With the projection `16(x-y), 8(x+y)`
(INFERRED): `|x-y| <= W/32`, `|x+y| <= H/16`, plus the sprite. `CombatTarget::Screen`'s 33/37
is a 1066x600 viewport.

## Open (live reads settle them)

1. The hover array in pad play: watch the `0x2A41630` entry while sweeping the stick.
2. The per-category slot layout in ControllerManager, to read the monster slot an attack takes.
3. `.rdata` floats: 1/65536 `0x1CBD0C8`; 1.0/0.5 `0x1CBA904`/`0x1CBA900`; cone `0x1CC4128`,
   `0x1CCC67C`; stick rotation `0x1CCC680`. Viewport `0x22A0D68`/`0x22A0D6C`.
4. The Warlock's monster entry; category 10; `0x34F8D0`, `0x7A2CA0`; `classTargetingData`.
5. D2R's own `missiles.txt` and `monstats2.txt` (not in `game-data/`).
