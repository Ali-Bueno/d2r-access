# HUD sight gate

D2R names things the player cannot see. Its own targeting reaches through
walls, so the monster health plate and the world target label happily name a
Fallen or a chest in the next room, and the plugin's tooltip reader spoke
whatever they said. The right-stick [entity scan](entity-scan.md) requires a
terrain line of sight, so the same monster was named by one feature and denied
by the other — reported by the player on 2026-09-18: *"the mod says the name of
an object or a monster while I move, when it is behind a wall and not when I
can actually interact with it."*

The gate makes every automatic announcement agree with the scan: a monster,
NPC or object is named only when a ray from the player reaches it.

## Behaviour

- Applies to the two world readouts of `InteractionReader::Tick`, both of which
  only exist while no panel is focused: the `HUDMonsterHealth` plate and the
  `World target` tooltip. Panels, inventory, buttons and dialogue are untouched.
- The label is the game's own text (`"Zombie. Undead"`), not a unit id, so the
  entity behind it is found **by name**: `TargetSight::Names` matches an
  entity's name inside the label on word boundaries, the same rule the ground
  item labels use.
- A label is dropped only on positive evidence: at least one entity within
  `reach` carries that name and **none** of them has a line of sight. A name no
  read unit carries, or one whose bearers are all beyond `reach`, stays
  `Unknown` and is spoken — a changed string table or an unread room degrades
  to today's behaviour rather than to silence.
- Ground items are skipped (their proximity service decides on distance, and
  already owns those labels) and so are level exits, which the scan itself
  exempts from walls as fixed landmarks.
- A dropped label leaves the reader with nothing to say, so the entity
  announces itself as soon as it really comes into sight — it is not swallowed
  once and forgotten.
- Object targets use the same native footprint correction as the scan: their
  own OBJECT collision does not hide them, while real walls and doors still do.
- The friendly NPC announcer ([npc-proximity.md](npc-proximity.md)) now
  requires the same line of sight by default, for the same reason.

## Configuration

`radar.toml` `[target]`, read once at `Register`:

| key | default | meaning |
|---|---|---|
| `line_of_sight` | `true` | `false` speaks every label, as before the gate existed |
| `reach` | `40` | how far a bearer of the name may stand and still be the one meant, in collision subtiles: the game labels only what is on screen, and a legacy 800x600 screen spans about forty |

Log line at startup: `[target] lineOfSight=1 reach=40.0`. Each label dropped is
logged once, as `[interaction] behind a wall, not spoken -> "Zombie. Undead"`.

## Code

- `src/target_sight.{h,cpp}`: `Names`, `Of`, `Settings`. Pure; tested by
  `tests/target_sight_tests.cpp` (22 checks).
- `src/entity_radar_service.cpp`: `LabelSight` — this service already owns the
  world and unit reads the answer needs, and caches one snapshot for 120 ms so
  asking on every UI poll is cheap. `src/radar_config.cpp` parses `[target]`.
- `src/interaction_reader.cpp`: the gate itself, beside the existing
  ground-item deferral.
