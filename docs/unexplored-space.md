# Nearest unexplored space (slash)

Press **slash** and the plugin says the way to the closest place on this level
the player has not walked yet, as pushes of the left stick:

> Nearest unexplored space, up 6, right 4, up 3

Walk those runs, press again, get the next ones — the step-by-step sweep of a
dungeon. Asked for on 2026-09-18, after the same key in the author's DevilutionX
fork (`Source/utils/navigation_speech.cpp`, `SpeakNearestUnexploredTileKeyPressed`),
where a breadth-first search stops at the first tile the game's automap has not
flagged as explored and the path to it is spoken as compass runs.

## What "unexplored" means here

D2R has no such flag to borrow. It streams collision rooms around the player, so
a room left behind unloads and reads as *unknown* again, exactly like one never
entered — the world snapshot cannot tell the two apart. The plugin therefore
keeps **its own memory**: every 250 ms it paints a disc of `mark_radius`
subtiles around the player into `Explored::Map`, a sparse map of 32x32 bitmap
chunks (a whole level is a few hundred of them, tens of kilobytes). Unexplored
means *I have not walked there*, which is the sense the player asked for. The
memory is per level and per game: a new level, or a new game on the same level
number, starts empty.

## Behaviour

- The search runs over `Navigation::Build`, the same navigation grid the sonar
  uses, so it obeys exactly the walls that grid knows, within `radius` subtiles
  of the player (the world snapshot itself ends at 152).
- Breadth-first from the player over the **four screen directions** — the ones
  the left stick pushes, which are the world diagonals. Breadth-first order
  makes the answer the nearest one *in steps walked*, not in straight-line
  distance: a place ten steps down the corridor beats one three subtiles away
  through a wall.
- The destination has to sit **`probe_radius` subtiles deep inside unexplored
  ground** (Chebyshev distance to the nearest walked cell, from one sweep over
  the window). The first version aimed at the first cell the memory did not
  cover, which is the rim of the brush the player carries with him: it retreated
  exactly as fast as he approached, so two steps down a "down 9" route answered
  "down 9" again, and ties between equidistant rim cells flipped the direction
  entirely. Reported on 2026-09-18 with the log to prove it. Near the end of a
  level, when no ground is that deep any more, the rim is still used.
- The destination is then **kept between presses**. It is given up only once it
  has been explored — which is what arriving does — or it can no longer be
  reached, and only then is a new one picked. A route that changes its mind
  every few steps cannot be walked.
- Among equally short routes the search prefers the one that carries on in the
  direction it is already going, which both shortens the readout and keeps it
  stable while the player moves along it.
- The path is compressed into runs of identical direction and spoken with the
  movement words the radar already uses (`radarMovementDirections`). At most
  `maxRuns` (6) runs are spoken; the rest come on the next press.
- Nothing unexplored reachable → *"No unexplored space nearby"*. Reachable only
  along the world axes, which no single stick direction walks, → the straight
  bearing plus *"no route, direction only"*. No readable world → *"Route
  unavailable"*.

## Controls and configuration

- **Slash** (`explore-route` action, rebindable in the loader like every other).
- Console: `access-explore` or `access-explore now` speaks the route,
  `access-explore status` logs the memory.
- `radar.toml` `[explore]`: `enabled`, `radius` (80), `mark_radius` (12),
  `probe_radius` (12).
- Log prefix `[explore]`: the startup settings, and per press the player
  position, level, outcome, target, distance, number of runs, whether the
  destination was kept or arrived at, and the spoken text.

## Code

- `src/explored_map.{h,cpp}`: the per-level memory (`Follow`, `Mark`, `Seen`).
- `src/unexplored_route.{h,cpp}`: `Find` and `Describe`. Pure; both are tested
  by `tests/unexplored_route_tests.cpp` (36 checks) over constructed worlds,
  including walking a corridor towards a kept destination and watching the
  instruction count down.
- `src/explore_service.{h,cpp}`: lifecycle, the marking poll, the key, the
  console command and the speech. `src/radar_config.cpp` parses `[explore]`.
