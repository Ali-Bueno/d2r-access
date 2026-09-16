# Entity scan (right stick)

On-demand directional readout of what the player could see: push the right
thumbstick in a screen direction and the plugin speaks the nearest visible
entity in that quarter of the plane with its distance; push the same direction
again to hear the next nearest, wrapping around. It never scans on its own and
never speaks while moving; every announcement is a response to a push. The
automatic counterpart for friendly NPCs only is `docs/npc-proximity.md`.

## Behaviour

- Directions are the fixed screen axes the left stick and the sonar already
  use (`Radar::Axes`): up = screen up = world (-X,-Y), and so on. Each
  direction owns a 90-degree sector centred on its axis; a target exactly on a
  diagonal belongs to both neighbouring sectors.
- Candidates must be inside `Settings::range` (collision subtiles) and have a
  clear terrain-layer line of sight from the player's cell centre. The ray is
  sampled every half subtile; other actors never block (the sonar's
  `TerrainCollisionFlags` strips them), the target's own collision cell is
  ignored, and unknown cells are transparent. Only positive wall evidence hides
  an entity, so a closed door counts as a wall.
- Nearest first, ties by unit id. `Cycler` keeps the last spoken unit id: a
  second push in the same direction within `cycleTimeoutMs` (5 s) rescans and
  continues after that id, wrapping to the first. A push in another direction,
  a timeout or an empty result restarts from the nearest.
- Wording (`speech_strings.h`, `scan*`): `Akara, 8 metros, 1 de 3`;
  `Akara, 1 metro`; `Akara, aquí mismo` under one metre; `Nada adelante` /
  `atrás` / `a la izquierda` / `a la derecha`. Every push interrupts speech.
- Distance unit: `Settings::metersPerSubtile` (default 1: one collision
  subtile per game yard, the velocity unit). Provisional until calibrated live;
  `access-scan range <subtiles>` adjusts the reach without restarting.

## Controls

- Right thumbstick push (leaves the dead zone; release and push again to
  cycle). Sampled in `input_direction.cpp` (`TakeRightStickPush`).
- Keyboard for tests without a controller: Ctrl+Numpad 8/4/6/2 = up/left/
  right/down, Ctrl+E toggles.
- Console: `access-scan on|off|status|up|left|right|down|range <subtiles>`.
- Log prefix `[scan]`: direction, player position, entity/visible counts, the
  spoken index/id/distance and the text.

## Entity sources

| Kind | Source | Status |
|---|---|---|
| Ground items | `NativeGroundItems::Read` (resident room + neighbours) | done |
| Monsters / NPCs | `NativeEntities::Read`: unit type 1, MonStats name id / cached composed name, alignment + npc flag, dead modes and flags, player minions skipped | built, live check pending |
| World objects (chests, shrines, doors) | `NativeEntities::Read`: unit type 2, objects.txt key name, per-mode Selectable byte, door captions by mode, shrine name via linked row | built, live check pending |
| Exits (level warps, portals, waypoints, gateways) | `NativeEntities::Read`: unit type 5 named after the destination level; objects by `SubClass` / `OpenWarp` / level-named key. `Kind::Exit`, also the portal loop. `docs/level-exits.md` | built, live check pending |

## Code

- `src/entity_radar.{h,cpp}`: `LineOfSight`, `InSector`, `Scan`, `Cycler`,
  `Describe`. Pure; tested by `tests/entity_radar_tests.cpp` (44 checks).
- `src/entity_radar_service.cpp`: lifecycle (player/session/level), input
  actions, console command, world + unit reads on a push, speech and log.
- `src/native_entities.cpp` + `src/entity_layout.h`: guarded room walk for
  monsters and objects; offsets and fingerprints from `docs/entity-units-re.md`.
  Tested by `tests/native_entities_tests.cpp` over constructed memory.
