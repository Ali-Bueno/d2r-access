# Friendly NPC announcer

While the player explores, the plugin names the nearest **friendly** NPC
(Akara, Kashya, Warriv, Charsi, Cain…) as soon as it is within range, with its
distance and screen direction, so the left stick can walk to it without
sweeping the right stick. Enemies, objects and ground items are never
announced: the right-stick scanner (`docs/entity-scan.md`) remains the
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

## The "!" over an NPC (2026-10-13)

A tester asked for parity with the yellow "!" a sighted player sees over a town
NPC with a quest or reward. It is overlay 72 (`npcalert`, Overlay.txt) in the
unit's overlay list: graphics data `Unit+0x78` (0x9A600), list head `+0x440`,
a node's id at `+4`, the next node at `+0xB0` (the game's own test 0x152650).
Packet 0x8A's handler (0x10F190, wrapper 0x12E150) adds it, skipping the NPC
being talked to; packet 0x27's path (0x10B2B0) removes it from every NPC, so
talking to anyone clears every mark until the server sends it again. The
server sends 0x8A only within 15 subtiles (town AI 0x4A8930, D2MOO
`AiTactics.cpp`): a far NPC carries no mark yet. Static RE of the live dump;
fingerprints in `entity_layout.h` (`NpcAlertChecks`).

`NativeEntities` reads it for NPC-flagged monsters (`Entity::npcAlert`). A
small bell (`EntityCueAudio::Sound::NpcAlert`) rings, panned, when a mark
appears (`EntityCues::AlertTracker`), and after the NPC's name from this
announcer and from the scanner. Switch `npc-alerts` (Ctrl+S, on).
Live check: a fresh Act 1 game, walk to Akara and Warriv: `[npc] exclamation
mark id=...`; after talking to anyone the bell rings again only for NPCs that
still have something to say.

### The "!" appearing while the NPC is near (2026-10-13)

Tester: the bell did not reliably ring when the mark appeared after the name was said. The announcer rings
only at the name; the mark appearing later is the entity cue service's `AlertTracker`, which required the
NPC cue switch (`entity-cues-npcs`) on as well as `npc-alerts` and consumed a mark while it was off. The bell
(`RingAlert`, `PlayNpcAlert`) now depends on `npc-alerts` and the master cue switch only, and the tracker
state advances either way. The NPC announcer's poll asks the same tracker (`RaisedAlerts`), so a
mark rings within its poll even while the cue service is quiet, and still once. Live check: `[npc] exclamation
mark id=...` when the mark appears while standing near.
