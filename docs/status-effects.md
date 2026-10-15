# Harmful states on the player (2026-10-14)

A tester asked for poison, freezing, chill and every curse to be spoken with high priority as
they begin ("You are poisoned", "You are frozen", "You are afflicted by Amplify Damage"). Only
poison was spoken before, queued.

## Reading

The verified state checker `0x3351B0(unit, state)` (`docs/combat-status-evidence.md`: the health
orb's poison colour calls it with state 2) takes any `states.txt` row. `NativeStatus::PlayerStates`
asks it for each listed state on the live local player, under the same fingerprints; a fault or a
dead or absent player reads nothing, which changes nothing.

## Which states

`game-data/global/excel/states.txt` (232 rows, Reign of the Warlock included; `*ID` = row) has no
debuff column, so the list is explicit (`src/status_effects.h`):

| state | row | named by |
|---|---|---|
| poison, freeze, cold, stunned, slowed | 2, 1, 11, 21, 24 | the mod's own words (no game string names them) |
| amplifydamage, weaken, dimvision, ironmaiden, terror, attract, lifetap, confuse, decrepify, lowerresist | 9, 19, 23, 55, 56, 57, 58, 59, 60, 61 | the game's name of the skill whose `auratargetstate` they are (66, 72, 71, 76, 77, 86, 82, 81, 87, 91) |
| defense_curse, blood_mana | 113, 114 | the mod's own words: skills 309 and 310 have no description |

Left out on purpose: the `curse=1` rows that are buffs or shrines (battlecry 89, shrines
128-137, bloodlust 141, cloaked 156), taunt (27, a monster cannot cast it on the player), and the
Warlock's hex debuffs (199, 201, 217), which only a Warlock casts. Unverified whether monsters
can reach a player with them; add a row once a log shows one.

## Speaking

`StatusEffects::Tracker` takes a silent baseline on entering a game. A beginning interrupts; an
end is said queued after `EndHoldMs` (1 s) without the state, and a state back within that time is
the same one going on, so a chill renewed by each cold hit is one chill. BACK+X adds the states on
the player to the life report. Switch `status-alerts` (Alerts, on). Log: `[vitals] effect began|ended`.
