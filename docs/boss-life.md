# Boss life

Requested 2026-10-02: narrate the life of Rakanishu, Blood Raven, Andariel,
Mephisto, Diablo and the like as it falls, the way the player's own is.

## Behaviour

- Same steps as the player's life (`Vitals::Steps`, [vitals](vitals.md)): 75,
  50, every 10 down to 10, then every point. The real percentage is spoken
  ("Andariel, 74 por ciento"), a critical fall interrupts, healing across a
  step is said queued, and death is said once as 0.
- The first reading of a boss is only a baseline, so seeing one says nothing
  until it is hurt. A boss that unloads is forgotten and baselined again.
- Every loaded boss is followed apart; two crossing in one poll form one
  utterance. Polled every 200 ms, only while playing (a pausing menu deals no
  damage; its last baseline is kept).
- `access-boss-life on|off|status`; `radar.toml` `[boss_life] enabled`.
  Log: `[boss-life] <text>` per announcement.

## Which monsters

`EnemyTraits::boss` (`src/native_entities.cpp`):

- monstats.txt `boss` or `primeevil` (`MonStats+0x3C`, bits 0x40/0x80, already
  verified for the important-enemy cue, [entity units](entity-units-re.md)):
  act bosses and quest bosses such as Radament, Blood Raven, the Summoner;
- `MonsterData+0x1A` bit 0x0002, superunique: Rakanishu, Bishibosh, Corpsefire.
  Client unit setup `0x98FA0` stores the SuperUniques row (`MonsterData+0x2A`,
  written by `0x38E810` at `0x99207`) only when that bit is set; it is also the
  classic D2 `MONTYPEFLAG_SUPERUNIQUE`. Champions, random uniques and minions
  are not narrated.

## Where the percentage comes from

The game's own health plate. `HUDMonsterHealth`'s builder `0xCEA50` calls
`0xD0640(unit, 100, 1)` at `0xCFC6F` and stores the result as the bar fill.
`0xD0640` returns 0 for a null, non-monster or dead unit, else clamps
`percent * scale / 100` to `[min, scale]`, where the percent is `0x97490(unit)`:
the hireling relation's value for a pet, otherwise `GetStat(unit, 6) * 100 /
max`, with max forced to `0x8000` for any unit but the local player (`0x97532`).
So the client keeps a monster's life as a fraction of `0x8000`, which is why the
mercenary's raw life read `0x8000` at full ([combat status](combat-status-evidence.md))
and why stat 6 / stat 7 is wrong here.

`NativeStatus::MonsterLife(id)` finds the unit with `0x9A5D0(id, 1)`, checks its
type and id, and calls `0xD0640(unit, 100, 1)`. Guarded by whole-function
fingerprints of `0x9A5D0`, `0xD0640` (304 bytes) and `0x97490` (228 bytes),
matched on 2026-10-02 against both `re/D2R_live.exe` and the D2RLoader 1.3.1-beta
live dump. `[status] Native condition readers: … monster=1` confirms them at startup.

A boss that has just died drops out of `NativeEntities::Read` (dead modes are
skipped); its unit is still loaded, so the service asks `MonsterLife` for every
followed id the snapshot lacks and speaks the 0.

## Code and tests

`src/boss_life.{h,cpp}` (pure tracker), `src/boss_life_service.{h,cpp}`
(lifecycle, reads, speech), `src/native_status.cpp` (`MonsterLife`). CTest
`boss-life`. Live check pending: fight Blood Raven or Rakanishu and listen for
the steps; confirm `monster=1` in the startup log.
