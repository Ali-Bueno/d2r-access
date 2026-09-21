# Target beacon (2026-09-16)

Follow-up: the scan also offers distant fixed level exits from existing logical
room presets, with stable identity across unit unloading. See [distant exits](distant-exits.md)
for coverage. Older nearby-only limitations below describe the initial scan;
complete unexplored routing remains unavailable.

Lets a blind player walk to a specific thing instead of only sensing walls
around them. The directional entity scan (`docs/entity-scan.md`) names
whatever the right stick points at; a right-stick click, or **Ctrl+B**, locks
the last thing it named. From then on a ping repeats, faster the closer the
route gets, panned along the next leg of a route proved on the game's own
collision — so a dungeon corridor is followed turn by turn instead of the
ping just pointing straight through a wall. Clicking again (or `Ctrl+B`,
`access-beacon clear`) cancels the lock. Arriving announces the target and
releases it on its own.

## Controls and console

- Right-stick click (`InputDirection::TakeRightStickClick`) or **Ctrl+B**:
  lock the last thing the entity scan or the exit review announced, or release
  the current lock if one is active. The click is reported on **release**, and
  only when the press stayed under 350 ms: holding the stick down past that is
  the exit review's "next way out" instead (`docs/rooms.md`), and it swallows
  its own release so the two gestures never both fire.
- Console `access-beacon lock|clear|status` (`lock` also accepts `toggle`,
  `clear` also accepts `off`); `status` speaks whether something is locked
  and, if so, its name.

## The model

- The entity scan hands the beacon service an *offer* (`BeaconService::Offer`)
  every time it announces something: entity, level and timestamp. A lock only
  takes the most recent offer, and only while it is still the same level and
  within `OfferWindowMs` of being spoken — never something older than what
  the player just heard.
- Once locked, the target is re-located every tick by matching its unit id
  (and kind, for non-items) against a fresh native read (`Refresh`); ground
  items are matched the same way against the ground-item snapshot. If the
  target's room is not currently loaded the match fails and its **last known
  position is kept** — that is still where it was, so the ping keeps leading
  there.
- A level change (`ActChanged`/`LevelChanged`, or leaving/joining a game)
  bumps the service's lifecycle revision and the beacon releases on the next
  tick (`reason=lifecycle`); it also releases on arrival or a manual cancel.
- It stays completely silent, but keeps its lock, while dialogue or a pausing
  menu is open or the game window is not focused (`Playing()` gate); it
  resumes on its own once play resumes.

## Point targets

A way out offered by the exit review (`docs/rooms.md`) can be a plain opening
between two streamed rooms, which the game has no unit for. `OfferPoint` locks
that place itself: identity is the coordinate identity the fixed level exits
already use, the per-tick `Refresh` skips it because a passage does not move,
and arrival is the same `arrivalDistance` as everything else. Nothing else
about the lock changes.

## Route search

`Beacon::FindRoute` (`src/beacon.cpp`) runs per tick, not per frame:

- A square box is cut around the player, sized to the straight-line distance
  to the target plus `lookAhead`, clamped between a 16-cell minimum
  neighbourhood and `searchRadius`; the world's rooms are blitted into that
  box once (`Field`/`Copy`) instead of re-scanning the room list per cell.
- Only the **terrain layer** is read (`TerrainCollisionFlags(raw) &
  walkingMask`), the same layer the sonar radar uses: a monster or another
  player standing in a doorway does not make the beacon reroute around them.
  The player's body footprint is then dilated over that field, following the
  native footprint patterns: 1/3/5 are a five-cell cross, 2/4 a 3x3 square,
  0 a single point.
- A breadth-first search from the player's cell finds the target cell. If it
  cannot be reached (unknown/blocked cell, or an unloaded room with only a
  last-known position), the search instead keeps the reachable cell nearest
  to the target as a sub-goal — walking toward an unseen destination.
- The spoken heading aims at the point `lookAhead` cells along the found
  route, not the very next cell, so the ping turns a corner early instead of
  hugging it.
- The route is recomputed at most every `routeIntervalMs`, or sooner if the
  player or the target has moved more than `reroute` cells since the route
  was last computed.

## How you get there

Locking a target ends the announcement with one word, from `Beacon::Reach`:

| Word (es) | Word (en) | When |
|---|---|---|
| recto | straight | The line to the target admits the body the whole way |
| rodeando | around | Not in a line, but the search reached the target |
| no se llega | no route | Neither, and the target is inside the search box |
| *(nothing)* | *(nothing)* | The target is farther than `searchRadius` |

Three points about it, each of which was the wrong way round in the obvious
version:

- **The straight test runs over the route's own `Field`, not over
  `EntityRadar::LineOfSight`.** That sight ray has no thickness and the player
  has a body; the `Field` already has the native footprint dilated into it, so
  testing the line there is both correct and guaranteed consistent with what
  the route says.
- **Straight is checked before reachable.** A chest, a post or a level exit
  stands on its own blocked cell, which the search can never enter, so by
  `reachable` alone a chest in an open field reads "no route". The line test
  skips both end cells, exactly as the scan's sight test does, and calling
  that a straight walk is what actually happens.
- **Beyond the box it says nothing.** `searchRadius` is 96 subtiles; claiming
  "no route" for something merely far away would be a lie, so the plain
  "Guiando hacia X" is used instead.

Ported from the Grim Dawn accessibility mod `grimdark`, whose one navigation
aid is a ping that answers this and nothing else —
`reference/audio-navigation/grim-dawn/navigation-spec.md` §6. It uses three
sounds where this uses a word.

## Parameters

| Name | Value | Where it comes from |
|---|---|---|
| `searchRadius` | 96 | `Beacon::Settings`, half-side of the route search box |
| `lookAhead` | 8 | `Beacon::Settings`, cells ahead on the route that choose the heading |
| `arrivalDistance` | 2 | `Beacon::Settings`, straight-line distance counted as arrived |
| `reroute` | 3 | `Beacon::Settings`, player/target movement that forces a fresh route |
| `routeIntervalMs` | 400 | `Beacon::Settings`, otherwise-fixed route recompute cadence |
| `nearPingMs` / `farPingMs` | 160 / 1200 | `Beacon::Settings`, ping interval bounds, linear in remaining route length |
| `farDistance` | 40 | `Beacon::Settings`, remaining route length at which the ping is slowest |
| `lockDelayMs` | 600 | `Beacon::Settings`, silence after a lock so the spoken name is not stepped on |
| `UpdateMs` | 150 | `beacon_service.cpp`, how often the locked target is re-read and ticked |
| `OfferWindowMs` | 15000 | `beacon_service.cpp`, how long a scan announcement stays lockable |
| ping tone | 620 Hz, 80 ms notes | `beacon_audio.cpp` `Synthesize`, `PingHz`/`NoteMs` |
| rear pitch | 0.94 | `beacon_audio.cpp` `RearPitch`; shared with the ground-item loops' rear factor |
| level | 0.15 | `beacon_audio.cpp` `Level`, peak gain before the user's volume setting |
| stereo/front placement | native projection `0x334DB0` | `beacon_service.cpp` `Place`; same screen-axis convention as the ground-item loops |

## Log format

- `[beacon] locked id=<id> kind=<kind> at=<x>,<y> level=<n> name=<name>` — on lock.
- `[beacon] released reason=<cancelled|arrived|lifecycle|unregister> id=<id> name=<name>` — on release.
- `[beacon] position=<x>,<y> target=<x>,<y> seen=<0|1> distance=<d> remaining=<r> routed=<0|1> heading=<hx>,<hy> route_cells=<n> name=<name>` — once per second while locked (`LogIntervalMs`); `seen` is whether the target's room is currently loaded, `remaining` is the route length when routed or the straight distance otherwise.
- `[beacon-audio]` — output device ready/unavailable/reopening, and `silenced reason=<reason>`.

## Limits

- Ordinary targets come from nearby loaded units. Fixed level exits additionally
  use the current level's logical-room presets, with no range/visibility cutoff.
  Their room list, rectangles and presets now have whole-function guards.
  Rooms with no generated preset data remain unknown; see [coverage](distant-exits.md).
- The route is only as good as the collision copied into the search box: an
  unloaded or unknown cell is never routed through, so a target behind
  unloaded terrain falls back to heading for the nearest reachable cell.
- Ordinary unit-id reuse after death remains unverified. Fixed level exits use
  coordinate identity, including when represented by an unloaded-room preset.

## Next steps

- Live listening pass with a controller and NVDA: lock, route confirmation,
  ping cadence and stereo placement, corner-turning behaviour.
- Validate distant exit coverage in multiple dungeons, including rooms whose
  presets have not yet been generated.
- Optionally, a way to ask for the remaining distance/route length on demand
  instead of only inferring it from ping cadence.
