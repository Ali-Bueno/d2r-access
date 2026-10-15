# Terrain overview

> Since 2026-09-21 the place is followed by the share of the level's rooms seen
> (`explorado 22 por ciento`), and the seen/visited memory persists on disk: `docs/exploration.md`.

One spoken line based on the generated level layout that says
where in the level the player stands, how far the terrain reaches towards each of
the level's four sides, where the part they have not walked yet begins, and which
places the scanner has already logged here.

This is not restricted to the revealed automap. Generated room geometry and
neighbour data can describe places not yet discovered; the streamed-room history
is not verified native visibility. See [automap discovery research](automap-discovery-research.md).

> Blood Moor, centre, 22 percent explored. northeast: edge at 35 units, unexplored
> from 15 units. southeast: edge at 30 units, unexplored from 10 units. southwest:
> Rogue Encampment at 30 units, unexplored from 10 units. northwest: edge at 35
> units, explored. Waypoint, 40 units, northeast. Exit to the Forgotten Tower,
> 120 units, south

Since 2026-10-15 (a tester: "the scan does not respect Diablo II's actual
directions") the sides are the level's own. A level's tile rectangle lies along
the world axes, which the game draws as a diamond: world -y faces up-right on
screen, +x down-right (`Radar::ScreenOffset`), so its sides face northeast,
southeast, southwest and northwest and its corners north, east, south and west.
The place is the player's third along each world axis, named by the side or
corner it is near (`north corner`, `northwest edge`, `centre`...). Each side's
leg walks tiles along one world axis. The landmarks are `KnownPlaces` for the
level (exits, waypoints, the stash, quest objects the scanner has logged, what
the automap shows), nearest first, with distance and direction as the scanner
says them; compass or clock per `direction-style`.

It never gives a route. Its shape and neighbour descriptions use available native
layout data, which is not proof that the player has discovered that terrain.

- The place word is the player's position in the level's bounding box, in
  thirds per world axis: centre, northeast edge, south corner, and so on.
- Each side reports the distance to the level's outer edge, and the
  distance at which unexplored terrain starts. A stretch that is unexplored
  within two map tiles of the player is simply "unexplored"; one walked all the
  way out is "explored".
- When the tile past the edge belongs to a room of another **already generated**
  level, that level is named instead of "edge". This is a data-availability rule,
  not a discovered-level filter.
- Distances are the collision subtiles the rest of the mod speaks as metres
  (`EntityRadar::DistancePhrase`), rounded the same way.

## Bindings

| Gesture | Action |
| --- | --- |
| **Ctrl+O** | Read the overview |
| **BACK + B** (hold BACK, press B), no menu open | Read the overview |
| `access-terrain` (or `access-terrain read`) | Read the overview from the console |

On the `bladestorm` branch the controller gesture is BACK+B. BACK is the
branch's modifier (`input_direction.h`); the B pressed with it is kept from the
game during play (`pad_filter.h`). Upstream removed its L3+R3 chord on
2026-09-25; here the mod's only stick button is L3, the beacon.

Ctrl+M was taken (room exits toggle), hence Ctrl+O.

## Data

Everything comes from the level's own logical rooms; nothing is generated,
revealed or written.

| Fact | Source |
| --- | --- |
| Level room list, room tile rects | `Level+0x10` → `RoomEx+0x48`, `RoomEx+0x60..0x6C` (`level_exit_layout.h`) |
| Room streamed right now | `RoomEx+0x58`, set/cleared by `0x361290` / `0x361260` |
| Level's own tile rect | `Level+0x24..0x30`, the union the DRLG writes (`0x3267C0`, `0x327650` / `0x3EC820`) |
| Rooms of bordering levels | the near list `RoomEx+0x10/+0x18`, the same test `ReadBorders` uses |
| Level names | the game's own Levels table (`NativeEntities::LevelName`) |
| Sides | `TerrainOverview::SideSteps` — one world axis each; screen placement by `Radar::ScreenOffset` (projection `0x334DB0`) |
| Landmarks | `TerrainService::KnownPlaces` for the level, the scanner's own store (`docs/map-landmarks.md`) |

Explored terrain is tracked by `ExploredRooms::Memory`: every 250 ms
(`5 * Radar::PollMs`) the rooms the game keeps streamed are recorded per level
id, and the set survives leaving and re-entering a level, like the automap. A
new session forgets it. The count is logged only when it grows by a tenth of
the level or the level changes; each query logs one `[terrain]` line.

## Limits

- The walk steps a tile at a time along a world axis, so distances are
  multiples of one tile (5 units) and the edge is reported at the first step
  that leaves the level.
- Room coverage is the terrain: a level whose rooms stop short of its bounding
  box reports the edge where the rooms stop, which is what the player can walk.
- Standing on no room at all (never seen in play) speaks the exit review's
  "unavailable" wording rather than guessing.
- Live validation of the spoken line in an outdoor level is still pending.
