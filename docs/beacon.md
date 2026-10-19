# Target beacon (2026-09-16)

Follow-up: the scan also offers distant fixed level exits from existing logical
room presets, with stable identity across unit unloading. See [distant exits](distant-exits.md)
for coverage. Older nearby-only limitations below describe the initial scan;
complete unexplored routing remains unavailable.

Lets a blind player walk to a specific thing instead of only sensing walls
around them. The level scanner on the right stick (`docs/entity-scan.md`,
`src/map_survey.h`) names the target; **BACK+LB**, or
**Ctrl+B**, locks what it spoke last. From then on a ping repeats, faster the closer the
route gets, panned along the next leg of a route proved on the game's own
collision — so a dungeon corridor is followed turn by turn instead of the
ping just pointing straight through a wall. Clicking with a different target
just scanned moves the lock there in one press; clicking again on the same
target (or `access-beacon clear`) cancels it. Arriving announces the target and
releases it on its own.

## Controls and console

- **BACK+LB** (`InputDirection::TakeBackWithLb`) or **Ctrl+B**:
  lock the last thing the entity scan or the exit review announced. With a lock
  active, a still-fresh offer of a *different* target (`EntityRadar::SameTarget`)
  replaces it in one press, silently releasing the old one (`retargeted`); a
  refused retarget keeps the old lock. Otherwise the press releases the lock.
  Moved from R3 on 2026-09-25 (R3 opens the emote wheel), then from L3 on
  2026-10-11: L3 is the game's walk/run toggle, so each beacon press also toggled
  running. `pad_filter.h` keeps LB from the game while BACK is held in play.
- Arrival: within 3 subtiles of the exact player position (the keyboard's use
  reach), plus half a large object's footprint. A target that vanishes as it is
  reached (a chest opened, an item picked up, read within the last 2 s) is
  announced as arrived, not lost.
- Console `access-beacon lock|clear|status|exploration|full` (`lock` also accepts
  `toggle`, `clear` also accepts `off`); `status` speaks the navigation mode,
  then whether something is locked and, if so, its name. `exploration` and
  `full` switch the mode for the session (see [exploration mode](#exploration-mode)).

## The model

- The scanner and the exit review hand the beacon service an *offer*
  (`BeaconService::Offer`) every time they announce something: entity, level and timestamp. A lock only
  takes the most recent offer, and only while it is still the same level and
  within `OfferWindowMs` of being spoken — never something older than what
  the player just heard.
- Whichever spoke last is what a click follows, which is the whole rule the
  player has to know.
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

## Area borders

A way into a neighbouring outdoor area is named by the middle of the edge the
two levels share (`LevelAreas::Neighbours`), which is often inside the trees or
cliffs lining the border. While the real crossing is in loaded collision, the
beacon follows it instead: `ZoneTransitions::Find` (the zone-transition sound's
source), within `Navigation::MaxRadius`, nearest the player
(`ZoneTransitions::MoveToCrossing`). Arrival, the route and, since the same
day, the scanner's distance and direction use that point. The identity keeps
the midpoint, so it stays one place. In
exploration mode the room holding the crossing may be entered unexplored, as
with an unexplored-room target.

Live log, 2026-09-25, Dark Wood → Black Marsh: the midpoint was 15640,5169 and
the player crossed near 15623,5160. Entering the midpoint's room released the
lock (`known-route-unavailable`) and later locks were refused (`no-route`).

**Found at the crossing (2026-09-28).** Exploration lists a border once it is
discovered, and that used to be judged at the midpoint only: in Tamoe Highland
the Monastery Gate's midpoint (15160,5090) lay in a room the automap had not
drawn while the player stood at the eastern entrance near 15265,5089, the
zone-transition loop already playing, so the scanner did not list the gate until
after the player had walked through and back. `ZoneTransitions::FoundAtCrossings`
now also judges a border at its walkable crossing; the listed entry keeps the
midpoint as its position.

**A corridor's mouth, not its inner end (2026-10-18).** A tester: the Outer Cloister notice and entry sat at
the inner door of the gate corridor. The shared edge of the two areas ends that corridor, so the crossing was
its far end. `ZoneTransitions::Find` now reads the corridor there (`Passages`) and moves the crossing to the
mouth facing this area (`CorridorMouth`); inside the corridor it stays the way on. The place is also
remembered at that point. Unverified live: the Gate's corridor must read as a corridor.

**Crossings seen are kept for the game (2026-10-14).** A tester crossed from
the Cold Plains into the Stony Field at 5400,5222, came back by waypoint, and
the beacon refused the Stony Field eight times from 129 to 334 units away
(`refused reason=no-route`): with the crossing not loaded, the way was planned
to the edge's middle, 5400,5240, inside the border's trees. It locked at 97
units, once the crossing loaded. Every crossing `ZoneTransitions::Find` gives
(scanner passes, beacon updates) is now kept per area for the game
(`CrossingMemory`), and the beacon, the route and the scanner's distance use
the one seen nearest the player when none is loaded. A border never seen
loaded is led to anywhere within 20 units of its edge's middle
(`BorderReach`, a `Recall::area`), moved into this area when the middle falls
just outside it; the crossing loads before the player gets there.

**Stepping across a border keeps the lock (2026-09-28).** A route can run along
an area border. Monastery Gate is 18 tiles tall. Its eastern entrance opens into
an alcove, and the only way on is a strip at y 5080–5089 along the Tamoe
border (y 5090) back to the central gate. The route to the Outer Cloister was
right, but one step south crossed into Tamoe, and any lifecycle change released
the lock. That happened twice in the log. Now, when the level changes to an
area that shares an edge with the target's level (`LevelAreas::Neighbours`), the
lock is parked instead (`released reason=left-area`). Walking back into the
target's level resumes it with the lock sound (`[beacon] resumed`). Guidance is
silent while outside. These drop the parked lock:

- a third level, or an area that is not a neighbour (a warp);
- a new lock or a Clear;
- BACK+LB with nothing offered, which announces the release;
- a mode change;
- a new game.

An exit target whose destination is the new level still ends there.

## Exploration mode

The default since 2026-09-24 (`docs/decisions.md`): the pathfinder orients and
helps the player return, but never discovers for them.

- **Scanner.** Something enters the scanner once it stands in an explored
  room (stood in, drawn on the automap, or underfoot: a room is seen whole on
  stepping into it, the user's rule of 2026-10-14), or once it has been within
  the scan's range (`EntityRadar::Settings::range`) on known terrain with a
  clear line of sight (`MapSurvey::Discoverable`); it then stays while out of
  sight. Enemies and loot count in the room underfoot only. Rooms are listed
  once stood in. Readouts give straight-line distance and
  direction (`surveyOrientation`), never walking steps. Exits and objects
  (waypoints, stones, doors) are kept per level for the whole game, so they are
  still there after a town trip (`MapSurvey::Memory`). Enemies are never
  remembered: they are read live, within range and in sight.
- **Beacon.** Routes use only the collision of explored logical rooms: those
  the player has stood in (`TerrainService::VisitedRooms`), those the automap
  has drawn in (`NativeAutomap::RevealedRooms`), and the current room, plus the
  target's own room: what the scanner lists can be routed to (2026-10-14).
  Everything else is masked unknown (`NavigationPolicy::Restrict`; on the
  remembered map, ground not seen yet), and the room graph is rebuilt from
  explored rooms (`BeaconNavigation::Prepare`). A way over explored rooms is
  preferred; without one the way guesses across rooms never stood in, and the
  first of them joins the floor followed, so the ping leads into it. Stepping
  in explores it and the plan is made again.
  A lock needs a route whose end reaches the target or the next waypoint
  (`Beacon::Reaches`); otherwise it says "No route through explored areas to X",
  stays silent and logs `[beacon] refused reason=unreadable|outside-explored|no-route`.
  A large object is reached from beside its native footprint: arrival grows by
  half its longer side, beyond the centre cell. Before this, a Cairn Stone could
  never be locked. Every route update repeats the check
  (`Settings::requireKnownRoute`): a target leaving the visited rooms or a route
  stopping short releases with "The known route to X is no longer available"
  (`reason=known-route-unavailable`) once no route has been proved for
  `routeLossMs`. The ping is silent meanwhile and resumes when the route returns;
  before 2026-09-25 a single failed update released the lock. A failed world or
  room read only skips that update.
- **Rooms.** A room target, explored or not, is reached at the first step
  inside it: the remembered way ends there (`Recall::room`,
  `LevelRoute::Area`), its last loaded cell is the waypoint, and the lock
  arrives as soon as the player steps in (`InTargetRoom`), since its centre may
  be inside a wall. Before 2026-10-14 the way was planned to the centre, and a
  route to the unexplored room next door failed when the centre was wall. Once
  entered, the room is explored, and everything in it joins the scanner.
- **Other players** are followed as in full assistance, through rooms this
  player has not explored: the lock is never refused and never ends for lack of
  an explored route (`Unrestricted` in `src/beacon_service.cpp`, user's request
  of 2026-09-26). Before, a friend who ran ahead could not be followed.
- Explored is room-level: one drawn cell or one step inside a logical room
  qualifies its whole rectangle, so a large room can route over floor never
  seen (`docs/automap-reader.md`).
- **Full assistance** restores the earlier behaviour: the whole generated
  catalogue with walking steps and routes through unvisited rooms.
  `[navigation] full_assistance = true` in `d2rloader/config/d2r-access.toml`
  (read at startup), or `access-beacon full` for the session. Switching mode
  releases any lock and clears the catalogue.

## Walkable guidance

Added 2026-10-12, after a blind tester's report: "In dungeons, the tracker
leads through walls". An item "56 steps right" really lay down, right, up and
right. When the loaded window had no complete route, the ping followed the
search's nearest reachable cell, often a dead end at the wall on the target's
side. With no route at all it fell back to the straight line. Now the ping
follows walkable ground or stays silent:

1. **The window's route.** When the loaded floor reaches the target
   (`BeaconNavigation::Prepare`, `ReachableNow`), the route search below leads
   there, as before.
2. **The remembered map.** Otherwise guidance plans over the level's floor as
   the game has shown it this game. This is the same `LevelRoute::Memory` the
   keyboard walks use, shared through `KeyboardService::RememberedMap` and
   learnt from each look. The waypoint is the last cell of that way still on
   the loaded floor (`LevelRoute::Ahead`). The route search leads there, so the
   ping turns where the corridor turns, not toward the target. The plan crosses
   ground seen open first, then ground not seen yet. It never crosses ground
   seen shut.
3. **Nothing.** When the map has no way either, no waypoint is guessed: the
   room graph's sub-goal is used only when no map is kept. A route search that
   stops short of the target is never followed (`Beacon::Reaches`, now in every
   mode). Without a proved route the beacon plays no ping.

The modes keep their rules:

- **Exploration** plans only over explored rooms (`Memory::Copy` keeps their
  ground alone). It refuses a lock with no route, and releases one after
  `routeLossMs` without a route, with the same strings as before.
- **Full assistance** and unrestricted targets (other players, unseen town
  NPCs) plan over the whole map. When no walkable route is known they keep the
  lock in silence, never releasing it, until a route appears.

**Shut doors.** A walking plan never passes a shut door: the game will not walk
through one. Guidance plans past it (`Plan::From(..., doors)`), because the
player opens it. The waypoint is the floor in front of the door. The ping stops
there, where the radar names the door, and goes on once it is open.

**Cost.** The plan is a search of the whole level (3.6 ms on 400 x 400, see
[keyboard play](keyboard-play.md#a-far-walk-the-remembered-map)). It runs only
when the loaded floor does not reach the target. `RememberedWay` plans on a
copy of the map, refreshed at most every `RefreshMs` (1 s). A moving target is
planned to where it was until it is 1 s old or `PlaceDrift` (10 cells) away.
New explored rooms or another level refresh the copy at once. Outside
exploration's grace, a lock without a route is searched again only at the usual
interval. Log, on change only: `[beacon] lead=direct|room-graph|remembered-map|remembered-map-guess|no-way way_cells=N to=x,y name=NAME`.

**Not verified in the game:** the tester's case itself, the ping stopping at
a shut door, how often the plan is rebuilt on a large level (the Arcane
Sanctuary), and whether full assistance's silence without a route needs
spoken feedback. Tests: `tests/beacon_guidance_tests.cpp`.

## Route search

`Beacon::FindRoute` (`src/beacon.cpp`) runs per tick, not per frame:

- An empty cached room graph is retried on the next service tick, even when
  the player has not moved. A transient native read failure cannot leave
  guidance using an empty graph indefinitely.
- A square box is cut around the player, sized to the straight-line distance
  to the target plus `lookAhead`, clamped between a 16-cell minimum
  neighbourhood and `searchRadius`; the world's rooms are blitted into that
  box once (`Field`/`Copy`) instead of re-scanning the room list per cell.
- `searchRadius` is **158**: the whole 152-subtile collision window the game
  keeps around the player, plus the look-ahead. That is the real ceiling on
  how far a beacon can lead — not a policy number to raise, because past it
  there is no collision to search. It was 96 while the only source of targets
  was the fifteen-metre scan; the catalogue can name things across the level,
  so the box now reaches as far as the data does.
- Both recompute triggers stretch in proportion past `farDistance`: the box
  grows with the distance while a long route changes slowly, so a level-wide
  lead is re-proved on roughly the work per second a short one costs instead
  of running the widest search several times a second. Near targets keep the
  original 3-cell / 400 ms cadence exactly.
- A target beyond the box, or in a room that is not loaded, is led to along
  the remembered map ([walkable guidance](#walkable-guidance)). Before
  2026-10-12 the search headed for the reachable cell nearest it, which a
  detour longer than the box fooled.
- Only the **terrain layer** is read (`TerrainCollisionFlags(raw) &
  walkingMask`), the same layer the sonar radar uses: a monster or another
  player standing in a doorway does not make the beacon reroute around them.
  The player's body footprint is then dilated over that field, following the
  native footprint patterns: 1/3/5 are a five-cell cross, 2/4 a 3x3 square,
  0 a single point.
- A breadth-first search from the player's cell finds the target cell. If it
  cannot be reached, the search keeps the reachable cell nearest to the
  target. Guidance follows that route only when it ends within
  `arrivalDistance` of the target, as beside a chest on its own blocked cell.
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
- **Beyond the box it says nothing.** `searchRadius` is 158 subtiles; claiming
  "no route" for something merely far away would be a lie, so the plain
  "Guiando hacia X" is used instead.

Ported from the Grim Dawn accessibility mod `grimdark`, whose one navigation
aid is a ping that answers this and nothing else —
`reference/audio-navigation/grim-dawn/navigation-spec.md` §6. It uses three
sounds where this uses a word.

## A shut door on the way (2026-10-19)

A player: "No route through explored areas" where guidance had led. Standing on the floor in front of
a shut door (or a chest or barrel in a corridor), the remembered way's next cell is one the loaded
grid stands no body on (`DoorBits` is door and object), so `LevelRoute::Ahead` found no waypoint;
`Toggle` then tried the far target, failed, and refused, and a lock in exploration mode was dropped
after `routeLossMs`. `Prepare` now makes the player's own cell the waypoint when that first cell
lies against door ground in the map (`AgainstDoor`): the ping is silent at the door and leads on once
it opens. Reproduced offline first (`beacon_guidance_tests.cpp` `DoorOnTheWay`). There is no
distance limit near 120: the route box is 158, the world window 152, the grid 150.

## Parameters

| Name | Value | Where it comes from |
|---|---|---|
| `searchRadius` | 158 | `Beacon::Settings`, half-side of the route search box |
| `lookAhead` | 8 | `Beacon::Settings`, cells ahead on the route that choose the heading |
| `arrivalDistance` | 3 | `Beacon::Settings`, straight-line distance counted as arrived |
| `reroute` | 3 | `Beacon::Settings`, player/target movement that forces a fresh route |
| `routeIntervalMs` | 400 | `Beacon::Settings`, otherwise-fixed route recompute cadence |
| `nearPingMs` / `farPingMs` | 160 / 1200 | `Beacon::Settings`, ping interval bounds, linear in remaining route length |
| `farDistance` | 40 | `Beacon::Settings`, remaining route length at which the ping is slowest |
| `lockDelayMs` | 600 | `Beacon::Settings`, silence after a lock so the spoken name is not stepped on |
| `routeLossMs` | 3000 | `Beacon::Settings`, how long exploration waits, silent, for an unproved route before releasing |
| `UpdateMs` | 150 | `beacon_service.cpp`, how often the locked target is re-read and ticked |
| `RefreshMs` | 1000 | `BeaconNavigation::RememberedWay`, most often the remembered map is copied anew to plan on |
| `PlaceDrift` | 10 | `BeaconNavigation::RememberedWay`, cells a moving target may move before it is planned to afresh |
| `OfferWindowMs` | 15000 | `beacon_service.cpp`, how long a scan announcement stays lockable |
| ping tone | 620 Hz, 80 ms notes | `beacon_audio.cpp` `Synthesize`, `PingHz`/`NoteMs` |
| rear pitch | 0.94 | `beacon_audio.cpp` `RearPitch`; shared with the ground-item loops' rear factor |
| `pan_strength` / `pitch_strength` | 100 % | `radar.toml [beacon]`, 0-200 %, scale the stereo spread and the rear pitch drop |
| level | 0.15 | `beacon_audio.cpp` `Level`, peak gain before the user's volume setting |
| stereo/front placement | native projection `0x334DB0` | `beacon_service.cpp` `Place`; same screen-axis convention as the ground-item loops |

## Log format

- `[beacon] locked id=<id> kind=<kind> at=<x>,<y> level=<n> name=<name>` — on lock.
- `[beacon] released reason=<cancelled|retargeted|arrived|lifecycle|unregister|known-route-unavailable|navigation-mode> id=<id> name=<name>` — on release.
- `[beacon] Registered; audio=<0|1> full_assistance=<0|1>.` at startup, and `[beacon] full_assistance=<0|1>` on a console mode switch.
- `[beacon] position=<x>,<y> target=<x>,<y> seen=<0|1> distance=<d> remaining=<r> routed=<0|1> heading=<hx>,<hy> route_cells=<n> name=<name>` — once per second while locked (`LogIntervalMs`); `seen` is whether the target's room is currently loaded, `remaining` is the route length when routed or the straight distance otherwise.
- `[beacon] lead=<direct|room-graph|remembered-map|remembered-map-guess|no-way> way_cells=<n> to=<x>,<y> name=<name>` — when the way guidance takes changes ([walkable guidance](#walkable-guidance)).
- `[beacon-audio]` — output device ready/unavailable/reopening, and `silenced reason=<reason>`.
  A buffer not returned one second after its length counts as a failed device and is reopened.

## Limits

- Ordinary targets come from nearby loaded units. Fixed level exits additionally
  use the current level's logical-room presets, with no range/visibility cutoff.
  Their room list, rectangles and presets now have whole-function guards.
  Rooms with no generated preset data remain unknown; see [coverage](distant-exits.md).
- A town NPC whose unit is not loaded is led to as its spawn point; its unit
  takes over the lock when it loads (same identity, `docs/entity-scan.md#town-npcs`).
  If the spawn comes around the player without the NPC, the lock keeps that
  place and says the NPC is gone on arrival.
- The route is only as good as the collision copied into the search box and
  the remembered map: an unloaded or unknown cell is never routed through. A
  target behind ground never seen is led toward over it as a guess (full
  assistance), and nothing is played where no way is known.
- Ordinary unit-id reuse after death remains unverified. Fixed level exits use
  coordinate identity, including when represented by an unloaded-room preset.

## Next steps

- Live listening pass with a controller and NVDA: lock, route confirmation,
  ping cadence and stereo placement, corner-turning behaviour.
- Validate distant exit coverage in multiple dungeons, including rooms whose
  presets have not yet been generated.
- Optionally, a way to ask for the remaining distance/route length on demand
  instead of only inferring it from ping cadence.
