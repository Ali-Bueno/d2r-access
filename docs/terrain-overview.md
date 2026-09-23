# Terrain overview

> Since 2026-09-21 the place is followed by the share of the level's rooms seen
> (`explorado 22 por ciento`), and the seen/visited memory persists on disk: `docs/exploration.md`.

The accessible equivalent of glancing at the automap: one spoken line that says
where in the level the player stands, how far the terrain reaches along each of
the four screen directions, and where the part they have not walked yet begins.

> Blood Moor, centre. ahead: edge at 49 meters, explored. to the left: edge at
> 42 meters, unexplored from 14 meters. to the right: edge at 42 meters,
> unexplored. behind: Rogue Encampment at 42 meters, unexplored from 14 meters

It never names a level the game has not generated yet and never gives a route:
it describes the shape of the ground, the way looking at the map does.

- The place word is the player's position in the level's bounding box, in
  screen thirds per axis: centre, left edge, back right corner, and so on.
- Each direction reports the distance to the level's outer edge, and the
  distance at which unexplored terrain starts. A stretch that is unexplored
  within two map tiles of the player is simply "unexplored"; one walked all the
  way out is "explored".
- When the tile past the edge belongs to a room of another **already generated**
  level, that level is named instead of "edge" — the same rule the exit review
  follows, so no undiscovered name is ever spoken.
- Distances are the collision subtiles the rest of the mod speaks as metres
  (`EntityRadar::DistancePhrase`), rounded the same way.

## Bindings

| Gesture | Action |
| --- | --- |
| **Ctrl+O** | Read the overview |
| **BACK + B** (hold BACK, press B), no menu open | Read the overview |
| `access-terrain` (or `access-terrain read`) | Read the overview from the console |

On the `bladestorm` branch the controller gesture is BACK+B, not upstream's
right-stick double click, so the beacon's R3 click is never held back for a
double-click window. BACK is the branch's modifier (`input_direction.h`); the B
pressed with it is kept from the game during play (`pad_filter.h`).

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
| Screen directions | `Radar::Axes` — each screen cardinal is a world diagonal (projection `0x334DB0`) |

Explored terrain is tracked by `ExploredRooms::Memory`: every 250 ms
(`5 * Radar::PollMs`) the rooms the game keeps streamed are recorded per level
id, and the set survives leaving and re-entering a level, like the automap. A
new session forgets it. The count is logged only when it grows by a tenth of
the level or the level changes; each query logs one `[terrain]` line.

## Limits

- The walk steps a tile at a time along a screen diagonal, so distances are
  multiples of one tile diagonal (about 7 m) and the edge is reported at the
  first step that leaves the level.
- Room coverage is the terrain: a level whose rooms stop short of its bounding
  box reports the edge where the rooms stop, which is what the player can walk.
- Standing on no room at all (never seen in play) speaks the exit review's
  "unavailable" wording rather than guessing.
- Live validation of the spoken line in an outdoor level is still pending.
