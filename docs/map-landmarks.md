# Named places inside towns: initial findings

Investigated 2026-09-15 after the user confirmed hearing Rogue Encampment on
entry and asked about smaller places such as a forge, kitchen or central square.
This is an investigation, not an implemented proximity reader.

## What the installed game exposes

Evidence comes from the installed CASC tables extracted with the existing
`build/casc-extract/casc_extract.exe`, not offsets from another Diablo version.

- `global/excel/levels.txt`: `Act 1 - Town` is level id 1, with `LevelName` key
  `Rogue Encampment`. The entry banner describes this whole level. The inspected
  level records do not subdivide it into kitchen/square/forge districts.
- `global/excel/monstats.txt`: friendly NPC records include Gheed (`*hcIdx` 147),
  Akara (148), Kashya (150), Charsi (154), Warriv1 (155), each with `npc = 1` and a
  `NameStr` localization key. These are identifiers, **not live positions**.
- `global/excel/objects.txt`: `WaypointOutsideAct1` (`*ID` 119) names a waypoint;
  `Bank` (`*ID` 267) names the stash. The localization key `Waypoint` resolves to
  Spanish `Transportador`. Object type existence does not prove its position in
  the player's current map; runtime instances must be checked.
- `global/excel/lvlprest.txt`: the Act I town preset uses multiple layouts, such
  as `Act1/Town/TownN1.ds1`, `TownE1.ds1`, `TownS1.ds1`. Fixed world coordinates
  from one camp layout would be unreliable.
- `global/ui/Layouts/AutoMapHD.json`: `AutoMapPanel` renders map tiles and has
  `unitLabelFont`/`unitLabelOffset`; it is not a tree of district text widgets.
- SDK `lifecycle_events.h` exposes level changes; `inventory.h` exposes an opaque
  player handle. The public SDK headers inspected do not supply a world-position
  or nearby-NPC enumeration service for those handles. Inventory grid x/y values
  are not world coordinates.

## Proposed accessible behavior and next implementation step

Use real named landmarks first: for example, announce "Cerca de Charsi" when
entering the vicinity of the actual Charsi instance, and similarly for Akara,
the stash and the transportador. Announce only a meaningful proximity change,
with separate entry/exit thresholds to avoid repeats near a boundary. An explicit
query should report nearby points and distance/direction once those coordinates
have been calibrated. Dialogue and menus must suppress automatic orientation.

Before enabling this, derive a guarded native world adapter for the current
build: local player position, live NPC/object identities and positions, and
current level. Validate against two town layouts and a known NPC. Reuse the
same adapter for the optional destination/beacon layer described in the user's
`docs/sonar-radar-implementation-spec.md`; terrain sonar remains separate.

No fictitious district names, guessed coordinates, automatic walking, or new
runtime dependency were added. A forge/kitchen/square label would need an
actual named game landmark or an explicitly designed accessibility landmark
mapped to verified geometry. The data reviewed does not establish those labels.
