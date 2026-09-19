# Friendly NPC announcer

While the player explores, the plugin names the nearest **friendly** NPC
(Akara, Kashya, Warriv, Charsi, Cain…) as soon as it is within range, with its
distance and screen direction, so the left stick can walk to it without
sweeping the right stick. Enemies, objects and ground items are never
announced: the right-stick entity scan (`docs/entity-scan.md`) remains the
tool for those. Requested on 2026-09-16 because story NPCs got lost while the
player was busy killing and looting.

## Behaviour

- Polls every `Settings::intervalMs` (500 ms) from the UI tick, only while the
  local player is ready and `RadarUi` reports `Playing` (no panel, dialogue or
  pause), and never while the game text reader owns speech. It reuses the same
  unit reader as the scan (`NativeEntities::Read`, kind `Npc`: friendly
  alignment or npc flag; hirelings/minions are already skipped there).
- Every friendly NPC inside `range` is remembered until it leaves
  `range + releaseMargin` (3 subtiles of hysteresis) or vanishes. The nearest
  remembered NPC is spoken **once**, the first time it is the nearest; the
  others speak when they become nearest. Leaving and re-entering range speaks
  again. Menus and dialogues do not reset the memory; a new level/game does.
- Wording (`NpcProximity::Describe`, shares the scan's distance phrase and the
  four `scanDirections`): `Kashya, 12 metros, a la derecha`; under one metre
  `Charsi, aquí mismo`. Spoken with `interrupt = false`.
- Direction = the screen axis closest to the target (same four sectors as the
  scan; a diagonal tie goes to the earlier direction in `Radar::Direction`).
- `line_of_sight = true` by default: the NPC must have the scan's terrain-layer
  line of sight before it counts as seen. Set it to `false` to hear NPCs walls
  hide (the sonar then handles the wall). Changed 2026-09-18 on player feedback:
  a name heard through a wall reads as "he is over there", and the player walks
  into the wall. [The HUD sight gate](target-sight.md) is the same rule for the
  labels the game itself puts on screen.

## Controls and configuration

- Ctrl+N toggles for the current run (`npc-toggle` action, rebindable).
- Console `access-npc on|off|status|range <subtiles>`.
- `radar.toml` `[npc]`: `enabled`, `range` (subtiles, default the scan's 15),
  `line_of_sight`. Parsed by `RadarConfig::ParseNpc`; absent keys keep the
  compiled defaults in `NpcProximity::Settings`, invalid values log
  `[npc] Invalid [npc] settings` and keep the defaults.
- Log prefix `[npc]`: startup settings, every announcement with player and NPC
  positions, distance and direction.

## Code

- `src/npc_proximity.{h,cpp}`: `DirectionOf`, `Tracker::Update`, `Describe`.
  Pure; `tests/npc_proximity_tests.cpp` (34 checks).
- `src/entity_radar_service.cpp`: `Proximity()` poll, `access-npc`, Ctrl+N,
  config read at `Register`. `EntityRadar::DistancePhrase` is the shared
  distance wording. Config checks live in `tests/native_world_tests.cpp`.

## Live check (controller)

1. Start in the Rogue Encampment with NVDA. Walk around the camp with the left
   stick; expect one announcement per NPC as each becomes the nearest in range,
   e.g. `Kashya, 9 metros, adelante`, then silence while walking towards her.
2. Talk to her (A) and leave the dialogue (B): nothing repeats.
3. Walk away past about eighteen metres and come back: she is announced again.
4. `[npc]` lines in `d2rloader/logs/d2r-access.log` show each announcement; the
   spoken distance is the scan's provisional metre (calibration pending there).
