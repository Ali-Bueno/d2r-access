# Input commands: the shared path, the 280 ms gate and the pad (RE)

Research of 2026-11-04, after the user asked whether NorthDakota's Modern ARPG Character Control
(Nexus mod 1345, three D2RLoader plugins, 1.0.1) could be done for the controller.

Evidence: its DLLs read statically, never run; `re/D2R_live.exe` (3.3.93787) read with Capstone. All
46 byte guards in the plugins match that dump, and the plugin's README claims 3.3.93847 and
3.3.94995, so those sites did not move. The rest of the code below was not compared with the
installed build. Nothing here was measured live. Tags: READ (seen in code), INFERRED.

## The answer

**The left stick never reaches the 280 ms gate (READ).** It calls `MoveToPoint` (`0x101EF0`)
directly, which is also what the mod's keyboard walking calls (`src/native_actions.cpp`). The mouse
reaches the same function through the gate. Held skills on the pad do reach the gate.

**What the user actually wanted was a different thing:** a modern ARPG walks to the enemy when you
attack, and D2R on the pad swings on the spot. That is the docking rule below, not the plugin's
subject. The `pad-approach` switch answers it, for attacks and for talking to NPCs.

## Why a pad attack stands still

`0x1454A0(manager, player, out)` decides on every press (`0x1450BF`) and repeat (`0x14C3CA`) (READ):

- `out` starts at 1, or at 0 for skills 0x165, 0x166 and 0x172 (`0x14550F`): Interaction always
  walks, walls included. `out` is the low byte of the word `0xFE3B0` takes, which sets command flag
  0x20 (`0xFE619`). Flag 0x20 skips the walk: `0x102626` and `0x102A6C`, before `MoveToPoint`.
- `out` goes to 0 only with a target (`0x18DD60`), a skill record with +0x1A set, and a yes from
  `0x190D40`. +0x1A is `skillUsesDocking` (INFERRED): 36 skills in `controllerskillsettings.json`,
  among them Attack, Bash, Smite and Zeal.
- A yes also enters state 2 (`0x14A330`, `+0x199C`), which the stick handler leaves at `0x14C804`:
  pushing the stick should cancel the approach (INFERRED).

`0x190D40` asks `0x33A870`, a line test around `0x363F20` (INFERRED), then
`dist < reach + docking`: `dist` from `0x325140` in half-subtiles, `reach` from `0x348650`, `docking`
at ControllerManager +0x1910. That field is 3 by default (`0x141C24`), loaded at `0x190404`, and
MEASURED 9 on 3.3.94995, the `targetDockingDistance` of `controllersettings.json`.

So only about 4.5 subtiles past reach is walked, while the auto-target picks up to 20-22 subtiles
out ([controller targeting](controller-targeting.md)): a far pick gets a swing on the spot. This
branch needs `0x77E10` true, which all three plugins also test: controller mode (INFERRED).

The field's other readers (READ): `0x18A8D6`, the same test for player candidates; `0x18A950`, see
below; `0x18CDDC` to `0x18CEF0`, a drawing loop (INFERRED).

## Why A does not reach a far NPC

A press on the first skill slot (INFERRED: A; the test is `ebx` 0 at `0x145CD9`, and `0x18D960` reads
slot 0 with `0x117D20(0)`) turns into Interaction (`0x14A6E3`, `0x145CB0`) when
`allowInteractOnDefaultAttack` is set (`0x22BD840`, INFERRED) and `0x18D960` finds a target. With
none, the slot's own skill runs, and in town the character says "Not here". Two limits stand in the
way (READ):

- **The slot.** Each frame the game keeps the best unit per category at ControllerManager +0xC20
  (stride 12: id, -1 when empty; type; score). Categories (`0x18E390`): 0 player, 1 monster, 2
  object, 3 item, 4 room tile, 5 NPC, 6 corpse, 7 pet, 8 ally, 9 player corpse, 10 hostile
  (INFERRED). The score table is at +0xD40 (0x120 bytes: cones +0, distances +0x60, curves +0x90,
  weights +0xC0, one entry per category), so the NPC's `targetAcquisitionDistance` is at +0xDB4.
  MEASURED 4.0 on 3.3.94995. The Interact group is mask 1597: 0, 2, 3, 4, 5, 9, 10.
- **The press.** `0x18A650` drops an Interaction target that fails `0x18AED0(player, unit, docking)`,
  `dist < reach + docking`, about 5 subtiles. No line test. Its only caller is `0x18A96B`.

Raising the first alone was seen live with a pad: Kashya walked to from about 7 subtiles, "Not here"
at 10 and 15.

The game then prefers the NPC to the attack: `0x18D960` drops it only when a monster is in melee
reach or `0x3B7490` says so (INFERRED: a ranged weapon). A far NPC also outscores a stash at hand:
`1-(d/D)²`, so 0.92 at 10 subtiles with D 35, against about 0.68 for an object at 3.

## `pad-approach`

Ctrl+S, Controller group, off: a choice for those who want the pad to play like the mouse, the
user's words of 2026-11-04. On, `src/native_pad_approach.cpp`:

- hooks `0x190D40` to say yes, so an attack walks to any target it has, around walls too;
- hooks `0x18AED0` to say yes for monster units, which is what an NPC is;
- gives the NPC `PadApproach::ByAim`: distance 35 (the table's default; the on-screen test limits it)
  and weight 0.35 at +0xE14, but only while slots 1, 2, 3, 4, 9 and 10 are empty. With anything else
  at hand the game has its own values, so A takes the stash, or fights, exactly as without the switch.

The game's NPC weight is 1.0, distance alone: the nearest in the 180° cone won wherever the stick
pointed (the user: "the nearest one I am looking at"). 0.35 is `MonsterMiddleGround`'s, the game's own
balance for aiming: `0.65·cosθ + 0.35·(1-(d/35)²)`. So 15 subtiles nearer forgives about 25° of aim.
The direction is the stick, or the facing with the stick at rest. The check runs every 100 ms.

Live, 2026-11-04, with the user's pad, `rorgan` in the Rogue Encampment, before the `0x18AED0` hook:
`NPC reach 4.0 -> 35.0`; beside the stash `35.0 -> 4.0` and "Tu alijo personal" targeted, then
raised again and Warriv targeted; dialogues opened with Warriv and Kashya. The target's name is
spoken from farther off, as the hover now holds a far NPC.

With the hook and the weight, the same day, same pad: `NPC reach 4.0 weight 1.00 -> 35.0 weight 0.35`;
Warriv shown with the A prompt at 11.7 subtiles; A on Kashya from 14.5 subtiles walked 1.3 s and talked.

**Not yet seen:** aim choosing between two NPCs; an attack around a wall. To check: the stick toward
the farther of two NPCs, then A (that one); A beside the stash with an NPC in view (the stash); A in a
fight near an NPC, as with Cain or Anya (the monster); the stick while walking (cancels); a monster
behind a wall (walks around it); a ranged skill (unchanged); the switch off.

## The command function

`0xFE3B0(player, action, x, y, word)`, 16 call sites. A jump table at `0xFEB78` (18 entries) picks
the branch, which sets flags in a command struct at `[rsp+0x58]`:

| Action | Flags | Then | Sent by |
|---|---|---|---|
| 0 | 5 | sets the target, `0x1023D0` | mouse, `0x840A9` |
| 1, 8 | 9 | `0x1013B0` | mouse, `0x8BA1A` (1) |
| 2 | 0x11 | `0x1023D0` | mouse, `0x94291`; pad, `0x145457` |
| 3 | 6 | `0x1023D0` | not seen |
| 4, 10 | 0xA | `0x101FA0` | pad held skill, `0x14C4B7` (10) |
| 5 | 0x12 | `0x1023D0` | pad, `0x145457` |
| 6 | none | `MoveToPoint` directly | **stick handler** `0x14C540` at `0x14CD31`, `0x14CE4A`; `0x143931` |
| 7 | 5 | `0x1023D0` | pad skill press `0x14B305`; `0x1932CC` |
| 9 | 6 | as action 3 | pad skill press `0x14B305` |
| 16 | 0x406 | as action 3 | `0x14B305` when `0x77E10` is false |
| 17 | 0x40A | `0x101FA0` path | `0x14C4B7` when `0x77E10` is false |

Actions 6-10 run the pad's target lookup (`0x192B20`). Actions 11-15 were not read (`0x8AC99` sends
11, `0x8BBCE` 12, `0x8AD95` 13, `0x14B305` also 14). The action at `0x83ACC`, `0x83BF2` and `0x8BAA7`
comes in a register and was not read.

Flag meanings, INFERRED from the table: 1 left, 2 right, 4 new press, 8 held repeat, 0x10 standing.

## The gate

`0x100BB0(unit, command)`, called only from `0x1023D0` (`0x102645`, `0x102A77`). At `0x102A77` a
zero return skips `MoveToPoint`. After stretching a short target to a minimum distance, it returns
0 when all of these hold (READ):

- `0x34B9D0(unit)` is 0;
- the command has flag 8;
- the unit's mode (`0x34AB60`) is 2, 3 or 6, walk, run and town walk (names from D2MOO);
- fewer than 0x118 (280) ms passed since the last pass (`cmp ecx, 0x118` at `0x100D48`, tick from
  `0x8B300`, last tick at `0x2A47604`).

So 280 is a literal in milliseconds, not a frame count. Flag-8 commands enter `0x1023D0` from
`0x1013B0` and `0x101FA0`, so pad actions 10 and 17 can reach it. Actual pad play through it was not
traced: it can only bite while the character is already moving under a held skill.

The stick handler has no tick call of its own (`0x8B300` has no call site in `0x14C540`). Its caller
only passes a frame delta (`0x14B7D9`).

## What the plugins do

- **ND-Fluid-Movement:** holds the bytes `81 F9 28 00 00 00 72 0D` beside the RVA `0x100D48`, so it
  rewrites the gate to 40 ms (INFERRED from data, matching its README). This is global, so it also
  touches pad held skills, despite the README's "nothing with controller inputs is affected". It
  compares return addresses against six mouse sites of `0xFE3B0`, so its hook skips the pad.
- **ND-Seamless-Skills:** guards `0x122F30`, `0x14B920` and `0x11F5D0` twice, which reads as hook
  targets (INFERRED). `0x14B920` is called from inside the stick handler (`0x14CE91`) and from
  `0x149C8E`. What the plugin does there was not read; it is the candidate for held-input priority
  on the pad.
- **ND-Unbroken-Casting:** item label hover filtering around `0x1013B0` and `0x101FA0`. No pad
  equivalent: the pad does not click labels.
