# Exploration: the unexplored mark, the two memories and their disk form

The mod's exploration history, in sound and in a word: which way out has not been
taken, where the ground the game has never streamed begins, how much has streamed.
This is not a verified reading of native automap discovery; see the
[2026-09-23 investigation](automap-discovery-research.md).
Design conversation of 2026-09-21; the user's rule for the radar still holds (four cardinals, the
original cues, only changes sound).

## Keyboard town exploration correction - 2026-10-04

The user's Kurast Docks session (level 75) had three physically visited rooms
by 08:23:28. H counted 30 of 48 rooms explored and 23 cut off, then said
Nothing left to explore. At 08:23:21 it chose room 31's centre, 5300,5140,
started a speculative map route and stopped with No way after moving six cells.
Manual navigation subsequently reached Ormus, as reported by the user and
recorded by the successful keyboard walk at 08:23:41.

The H producer used the same visited-plus-automap union as the beacon's
charted-route policy. `AutomapCells::RevealedRooms` marks a whole room when
any of its floor/wall tiles is drawn. Thus a town's map scenery could exclude
unvisited walkable ground from H, leaving only unreachable rectangles as
candidates. The log's 30-room count is that union, not a physical visit count.
Native automap cells were not separately captured before the game closed;
the exact 30-room membership remains unobserved.

`TerrainService::ExplorationRooms` now supplies H with physical visits plus
the current room in towns, using the established generated town flags. Other
levels keep their existing automap rule. `TerrainService::ExploredRooms` and
the beacon's route restrictions retain their existing meaning. No exploration
archive or save data is modified by this correction.

`ExploreGoal::Nearest` selects the fewest-step reachable cell inside the chosen
room when its collision is available, rather than aiming at a centre that may
be water or wall. Unknown distant rooms retain existing map/room guidance.
Actual collision, room-border, reachability and teleport-pad gates remain in
force. A synthetic dock fixture reproduces the old exhausted-goals result,
then selects reachable dock floor with the correction. The navigation-policy
suite covers towns, visited completion, other levels and the unchanged beacon
policy. Deployment/test state is in STATUS and the workspace HANDOFF; source
evidence and the user's session are in workspace `re/kurast-explore-2026-10-04/`.
Live Kurast exploration remains a user-led check.

## Supplied exploration sounds (2026-09-22)

`sounds/item loop sounds/exploration` supplies three OGG recordings, converted to mono PCM
at build time and embedded in the plugin DLL with the other loop sounds
(`sounds/one shot sounds/exploration/discovered` joins them for [discoveries](#speech)):

| Recording | Playback |
|---|---|
| `unexplored room` | One spatial loop for the nearest active, unvisited maze room within the existing 30-subtile audio radius. The current room is excluded. |
| `inside unexplored room` | A centered, finite cue when the player first enters an unvisited maze room. Persistent visited memory prevents standing, returning or reloading from replaying it. |
| `zone transition` | Spatial loops only at loaded crossings traversed by walking. Entrances requiring interaction, including dungeon stairs, cave entrances, portals and waypoints, use the portal sound. |

`ExplorationCues::Select` uses the nearest point inside each native room's
rectangle, with coordinate-based tie breaking. Only `Levels.txt DrlgType = 1`
has meaningful chamber partitions; open terrain and towns do not emit room cues.
The terrain service checks exploration before marking the occupied room visited,
and waits for stored memory before enabling room sounds. Its 250 ms poll feeds
an independent mixer channel, so loot updates cannot overwrite the room loop or
restart the arrival recording. Dialogue, blocked UI, focus loss, world-read
failure and lifecycle transitions silence this channel.

Outdoor transition sounds use `ZoneTransitions::Find`: openings with loaded
walkable collision on both sides of a native room border, mapped to the actual
neighbouring level. Rectangle midpoints used for distant bearings do not qualify
as sound locations. Unloaded crossings remain silent until collision is available.
Interactive entrances come from the loaded entity reader and use `portal`, even
when their category is the generic `Exit`. The native reader skips `NoInteract`
warp units and only includes selectable exit objects. If an interactive warp
coincides with a detected crossing, it keeps one portal cue without a zone duplicate.
The existing exit loop toggle, Ctrl+P or `access-items exits-on|exits-off`, also
controls transition sounds. `[exploration] enabled = false` disables both room
sounds on restart, independently of the radar and exit loops.

Validation: automated selection, memory, service and audio-asset checks; live
listening remains pending. Approach two new dungeon rooms and hear only the
nearer one, enter one and hear a single arrival, then return and reload to check
silence. At the camp's Blood Moor gate, approach the actual gap and compare it
with the palisade. Open dialogue and move focus away to verify silence.
`[exploration-audio]` logs target changes and first entries; `[item-audio]` logs
source start/progress/completion without logging every frame.

## The two memories

`ExploredRooms::Memory` (`src/explored_rooms.*`) keeps two rings per level, keyed by the logical
room's tile-rect origin (`RoomEx+0x60/0x64`):

| Ring | Meaning | Written | Read by |
|---|---|---|---|
| seen | the game streamed the room at least once (`RoomEx+0x58` set) | every 250 ms, `TerrainService::Track` | terrain overview, frontier, level-exit rule, percent |
| visited | the player stood in the room | same poll, the rect under the player's tile | chamber rule, exit review tags, room entry line |

Seen means streamed, not verified automap revelation. The earlier claim that
activation exactly matches what the automap draws was not established. The native
near-room test compares room rectangles with a six-tile gap threshold; it is not
a verified reveal radius around the player. Visited records physical entry.
Both histories only grow inside a game.

## The unexplored qualifier on the radar

A passable reading (exit, floor) carries a boolean qualifier, `Passage::unexplored`, decided by
`Radar::Engine::Qualify` from data the terrain service stamps onto the `World`
(`TerrainService::Qualify`). It is never a state of its own: walls and doors are untouched, every
`switch` on `State` stays as it was.

| Level type (`Levels.txt DrlgType`) | Reading | Unexplored when |
|---|---|---|
| any | exit that arrives at a level exit (`Passage::exit`) | the destination level (`LevelExit::level`, from the warp node / portal / preset) has no room seen |
| maze (1) | exit that crosses into another chamber (`Passage::crossingRoom`) | that chamber was never stood in (`RoomGrid::visited`) |
| outdoor (3) | floor | reachable frontier lies in that screen quadrant (`World::frontier`) |
| preset (2), towns | floor | never |

The frontier (`src/frontier.*`): a walkable cell of the loaded collision, reachable from the player
(`Navigation::Build` at the full 150-subtile window), whose neighbour is unloaded and lies inside a
logical room never seen. Each such cell votes for the screen quadrant of its offset from the player
(dominant screen axis, native projection `0x334DB0`). Recomputed every 250 ms on outdoor levels
only; the log line `[terrain] explored rooms=… frontier_ms=` says what it costs.

The qualifier is part of the reading for the sensor state machine: a change of the qualifier alone
is confirmed by the same travel rule and sounds like a change of state, under the same cue interval.
Since the seen ring only grows, a quadrant can only go from unexplored to explored while the player
stands still; it never flickers with rooms streaming in and out.

## The sound

The base cue is untouched. A marked exit or floor ends in two 900 Hz pips after a
30 ms gap. Each pip lasts 35 ms, with 30 ms of silence between them (`radar_synth.cpp`).
The same tail accompanies both readings so
it is one thing to learn, lowered for screen-down like the cue itself. `access-radar test` now plays
exit, exit unexplored, open, open unexplored, blocked, door in each direction. The cue queue keeps
the mark with the cue (`CueSource::unexplored`) and drops a queued cue whose mark no longer holds.

On 2026-09-22 the user requested a distinct sound rather than a pitch adjustment:
the former steady 660 Hz tail was confused with the pathfinder's 620 Hz ping.
The new double pip changes the rhythm; the pathfinder sound is unchanged.
Directional lowering applies to both pips. Live listening remains pending.

## Speech

- **Room entry line** (maze levels): each way out never taken gets the tag: `Salidas: izquierda,
  sin explorar, derecha`. Level exits inside the room: `Den of Evil, sin explorar adelante`.
- **Exit review** (Ctrl+V): the unexplored ways out come first, nearest first within each
  half; the label carries the tag: `abertura, sin explorar, 8 pasos, a las 12, recto, 1 de 3`.
- **Terrain overview** (Ctrl+O): the place is followed by the share of the level's rooms
  seen: `Blood Moor, centro, explorado 22 por ciento. adelante: …`. For maze and outdoor levels every
  logical room exists once the level is generated, so the denominator is the whole level.
- **Radar readout** (Ctrl+T): `sin explorar` after a marked direction.
- `Strings::Table::wayUnexplored`, `terrainPercent`; `radarTest` extended. Thirteen languages.
- **New place** (2026-09-25): the first time the scanner lists an exit on a map it says
  `El radar ha registrado una salida nueva: Cementerio`; a waypoint or the stash says `un lugar
  nuevo`. Queued, not interrupting. The gate is `TerrainService::RememberPlaces`: a place whose
  identity is already on the map's list (restored from `explored.dat` or seen before) is known, and
  the exit back to the level the player came from (`ExploredStore::Classify`, `Novelty::WayBack`) is
  where they came in, not a find. Both are still remembered and logged (`(way back)`).
  `[discoveries] enabled` in the settings file turns the speech off. `placeFoundExit`, `placeFound`.
- **Dungeon entrances** are exits to the scanner: the stairs or cave mouth is a warp unit, an
  OpenWarp object or a level preset, all `Kind::Exit`/`Category::Exit` with a place identity
  (`MapSurvey::Fixed`). Live log 2026-09-25, Dark Wood: `place remembered level=5 … to=10
  name=Al nivel 1 del Pasadizo Subterráneo`.
- **Quest objects** (2026-09-26, user request): an unused quest object (`EntityRadar::QuestSource`:
  the sources of the quest and tome loops -- Tree of Inifuss, Malus, the quest chests, neutral Cairn
  Stones, the Moldy Tome, Horazon's journal) says `El radar ha registrado un objeto de misión nuevo:
  Árbol de Inifuss`. It has no place identity of its own, so it is kept with the map's places under
  `NativeLevelExits::Identity` of its position; `KnownPlaces` still offers only fixed ones. A Cairn
  Stone goes by its plain name, and a second place of the same kind, category and name is silent
  (`ExploredStore::Namesake`), so the five stones are one find. `placeFoundQuest`.
- **Discovery sound** (2026-09-26): each scan that announces anything plays
  `sounds/one shot sounds/exploration/discovered.ogg` once, centered, on its own one-shot voice
  (`ItemAudio::PlayDiscovery`), so it never cuts the room arrival cue. Same `[discoveries]` switch.

## On disk

The mod keeps both histories in one compact binary archive,
`<plugin dir>\d2r-access\explored\explored.dat`, limited to 4 MiB and 400 level-layout entries.
Each entry retains its map seed, level and room-layout digest; different maps
never inherit each other's progress. The digest is FNV-1a over the sorted room
rectangles. The bounded archive replaces the former file per level.
If either limit is reached, the least recently updated map entries are evicted.

Since 2026-09-24 (format `D2REXP02`) each entry also keeps the fixed places
found on that map (`MapSurvey::Fixed`: exits, waypoints, the stash): id, place
identity (`NativeLevelExits::Identity` of its position), position, destination
level, kind, category and the name the game gave it (and, from `D2REXP03`, its
string key), at most 64 per entry. The
scanner lists them in a later game on the same map, before the game streams
their rooms again; a regenerated map has another key and starts with none
(`docs/entity-scan.md`). A normal fixed place takes a newer name, destination or kind, so the loaded
warp and its preset (different id, fraction of a subtile) do not rewrite its
position. Measured area borders can correct their coordinates while keeping
their naming identity (see the version 4 correction below). A stored place whose kind
and category are not a fixed place is skipped on use. Formats `D2REXP01` through `D2REXP03` still load and upgrade on the next save.
An older plugin unable to read the current format leaves the archive untouched
and cannot load or save exploration until updated.

The game language is not part of the map key. Rooms are tile positions, so an
archive explored in Spanish serves an English game. Since 2026-09-25 (format
`D2REXP03`) a place also keeps the game string key of its name
(`Entity::nameKey`: the Levels.txt name or warp key, or the objects.txt name
key). On load, the name is read again in the language being played. A place
stored by `D2REXP02` has no key and keeps its old text until it is found again.
Earlier formats still load.

A background worker handles disk reads, migration and atomic replacement.
The game callbacks merge cached records and enqueue progress snapshots on the
existing lifecycle and 30-second save cadence. Old `explored-<seed>-<level>-<digest>.txt`
files are imported and removed only after the replacement archive is verified.
Files with malformed rows remain intact; valid matching-level rows can still be
imported. Unreadable files are retained for a later retry.

The native automap is not a verified replacement: its persistent reveal structures
remain unknown. A later [read-only investigation](automap-discovery-research.md)
identified the container structure of two `.ma0` files, but not a real-time room
discovery mask or a verified native parser.
The known room-active pointer clears when a room unloads. Automap visibility also
does not establish that the player physically entered a room, which this feature
needs independently. The mod never modifies the game's automap or character save.

## What the game does not offer (RE, 2026-09-21)

- `Levels.txt WarpDist` is **not** the distance at which a neighbour level is generated: it is the
  squared-subtile radius for monster pack placement around a room's warps (`LevelsTxt+0x14`, reader
  `0x509CC0`; D2MOO `MonsterRegion.cpp`). The "~40 m" observation comes from level-rect adjacency
  and the six-tile near-room gap, nothing else.
- Outdoor level rects are fixed at act creation, before any room exists (`0x3F1C60` writes
  `Level+0x24..0x30` from the act's placement tables; D2MOO `DRLGOUTPLACE_CreateLevelConnections`).
  `LevelAreas::Neighbours` already reads them; it may name a bearing towards a level with no rooms.
- `CompletionTotalRoomsOverride` is not in the `LevelDef`; it lives in the `0x18C` LevelsTxt row,
  set for one level only (Halls of Vaught, 70), most likely a denominator for the terror-zone
  completion ratio. No per-level explored-rooms counter exists in the game; no achievement either.
- `.ma0` format: initial container structure inspected on 2026-09-23; semantics
  remain unverified, and the plugin does not read it.
- Correction recorded in `docs/act-levels-re.md`: `LevelDef+0x08` is `Layer`, `+0x40/+0x44` are
  `SubWaypoint`/`SubShrine`.

## Verification

The 2026-09-23 portal restoration passed all four affected suites: ground items,
ground-item service, native entities and zone transitions. The installed DLL hash
matches the build. Tests distinguish interactive stairs from walking crossings,
cover coincident-exit deduplication and verify the shared exit toggle. Listening
at a dungeon entrance and a walking area boundary is pending.

- `exploration` suite: both histories, snapshots, layout isolation, queued updates,
  legacy migration including malformed rows, failed writes and retry, archive
  bounds/corruption, and reachable/walled/distant frontier quadrants.
- `radar-engine`: qualifier on exit and floor, qualifier-only change confirms and sounds plain,
  chamber rule, walls never qualified. `radar-passage`: `crossingRoom`, `exit` index.
- `radar-audio`: tail length, untouched base cue, two identical pips with a silent gap, same tail on floor, none on walls,
  lowered for down; queue retention by mark. `exit-review`: ordering and tags. `terrain-overview`:
  percent.

On 2026-09-22, all 61 CTest suites passed and the installed DLL hash matched the
build. A game-closed migration replaced nine installed text files (3,855 bytes)
with one 2,846-byte archive. Independent readback against a temporary backup
matched every room and visited flag across all nine keys: 286 seen, 176 visited.
No legacy text files remain in that installation. Listening in game is pending.
- Live: `[radar] … unexplored=1` on generated cues, `[terrain] visited room=`, `[terrain] level=…
  seen=… visited=…` on load, the grid-origin warning must never appear.

## Act 3-5 routing corrections - 2026-10-04

The user's Spider Forest log captured entry from Kurast Docks at 5143,4999,
but discovery stored 5160,4999, the middle of the shared border. Five later
walks from the northern forest were refused at that midpoint. Great Marsh
had the same error: a crossing at 5319,4589 versus the stored point 5319,4360.
`FoundAtCrossings` tested a moved copy but returned the original entity;
`MergePlaces` also refused position corrections. This is coordinate evidence
from the user's own session, not a diagnosis based on the localized names.

The native area reader now explicitly marks its rectangle bearings. Only a
collision-measured opening is discovered as a walking exit. Discovery keeps
that position and its stable naming identity; the registry, scanner, beacon
and keyboard refresh restore it over a fresh approximate bearing after the
opening's rooms unload. Stairs, portals and warp presets are never moved
just because they lead to the same level as an area border.

Archive `D2REXP04` adds one measured-border flag per place. Versions 1-3 still
read, preserving all rooms, visits and other landmarks. Old midpoint records
matching a live area bearing are not treated as measured exits. Rediscovering
the opening corrects the old entry in place, without duplicate exits. Measured
border positions may update; normal warp/preset positions retain their earlier
fixed-position rule. An older plugin cannot read version 4, so retain the
pre-deployment archive backup when rolling back to an older binary. Disk I/O
and speech continue on the existing worker paths.

Explore now routes over the remembered map to any standable floor inside the
target room, preferring an entirely seen route. Its geometric centre can be
water, cliff or wall, even while the room has a reachable path along its edge.
The route ends on entering the room, as before; unseen floor can still guide
an exploration walk. Seen walls and doors retain their existing route rules.
Learning streamed collision no longer fills cells outside the logical room
union, preventing false bridges through holes in irregular maps.

The installed Levels.txt was audited for every level 75-132. Act 3 jungles are
64x192 tiles; Act 4 mixes outdoor borders with City/River stairs; Act 5 includes
240x48 Siege terrain, variable-sized barricade levels, 128x80 Snow terrain,
and cave/summit/Worldstone stairs. Dimensions always come from native rooms;
the memory limit remains unchanged. `../re/act3-5-routing-2026-10-04/layout-audit.json`
records the source hash, all rows and coordinate evidence.

Regressions cover measured discovery even when automap charts the guessed
midpoint, streaming/revisit retention, archive correction and versions 1-4,
long Act 3/4/5 return routes, blocked room centres, disconnected room holes,
and Act 4/5 warps with a matching border destination. These use synthetic
collision; audible gameplay and generated Act 4/5 maps remain user-led checks.

## Dock departure and H follow-up - 2026-10-04

The user reported that the deployed routing change still refused Spider Forest
from Meshif's dock, and that H failed in both the Docks and Spider Forest.
Own 10:02-10:03 log: departure from 5118,5168 was refused at the old bearing
5160,5000. After walking closer, collision measured the opening at 5142-5145,5000,
and the 10:03:29 walk crossed successfully. Two subsequent forest H walks chose
rooms five hops away, then ended with no way as collision loaded.

H now counts physical visits plus the current room in every level. This
supersedes the town-only exception above; the beacon's automap policy is unchanged.
Automap scenery is not evidence that the player entered a room. Selection first
chooses a loaded reachable entry by actual walking steps, then uses room hops
for candidates beyond loaded collision. If streaming proves the chosen room
cut off, the same H request selects another candidate. It still finishes after
entering one new room and respects doors, pads, manual cancellation and combat.

Unmeasured outdoor exit bearings now approach any reachable floor in the shared
edge's two-cell inner strip instead of testing or walking to the rectangle
midpoint. Both levels' collision must identify the real crossing before an exit
is taken; the approach does not write a measured place or invent floor. Exact
measured exits and stairs/warp presets keep their existing targeting. SetOut also
refreshes an exit before its initial reachability gate, so a stale scanner choice
can pick up an opening already measured since that choice was made.

Read-only live capture at 10:07:34: level 76, player 5191,4607, 192 logical rooms,
36 loaded collision grids. The checked-in collision fixture and matching stored
visits choose room 1016,912 with a complete 72-step route despite charted scenery.
That is a replay of actual captured collision, not audible gameplay validation.
Synthetic regressions also reproduce the Meshif-dock midpoint refusal, prove the
edge route, check all border sides and retain exact measured/warp targets.

Full CTest 105/105 pass, timeout 20. User closed game/loader and requested
installation; verified closed, normal four-job/BelowNormal build/deploy completed.
Installed SHA-256 `36CCEA6064679873F7CAFF4E0FEF201C6EBE9E756A2C756A8EEB9A7D896CB5CA`
matches final build and tested staged code/data (four link timestamp bytes differ).
PRISM/config/exploration archive unchanged; backups retained. User confirmed
the dock-exit and H fixes working in game on 2026-10-04 and ended the session.
Other Act 4/5 live checks are separate. Own logs/collision, archive read-only copy, build/tests, staged
DLL and deployment helper: ../re/kurast-routing-followup-2026-10-04/.
No save/archive editing, game input, commit, push or release.

## Flayer border sections and route uncertainty - 2026-10-04

The user reports that routing stopped near Flayer Jungle, then clarifies that
unexplored-territory coverage explains the stop and the absolute "No way"
message is misleading. Route failures now say "No known route to <target>"
in every supported language and map. A failed search does not establish that
the game has no passage. Existing beacon explored-area policy and keyboard
speculative-ground policy are preserved.

Own latest loader session has a remembered route toward 5160,4040, explicitly
logged as partly unseen, then aborts at 11:06:16 from 5202,4155. A separate code
defect is confirmed: the prior shared-edge approach searched only the first
neighbor room for a destination. The read-only 11:12:00 logical-room capture
has eight Flayer rooms across x=5000..5320 at the forest's y=4040 boundary, with
1048,800,8,8 first in its near list. The chosen strip therefore covered only
x=5240..5280. It could reject an unmeasured destination when that section was
blocked, despite other possible openings. The precise contribution to the
logged failure remains unproved: the actual northern crossing's collision was
not captured, and the user's coverage explanation is retained as such.

`ZoneTransitions::ApproachRegions` now returns every facing logical-room
section for the destination. `LevelRoute::Field` seeds all sections in one
search, preserving the preference for complete seen-floor routes. `Reach`
reports cutoff only when every candidate section is cut off. Sections stay
separate on irregular levels; native near-list duplicates/order cannot change
the candidate set or cached field. Unknown floor retains its existing guess
status. Actual collision still identifies the doorway before crossing; stairs,
warps, pads, screen-reader/log workers and physical controllers are unchanged.

Regression coverage includes the current user's actual forest/neighbor layout,
its full Flayer and Great Marsh edges, all four sides of synthetic outdoor
shapes across Acts 1, 3, 4 and 5, disjoint border spans, a blocked first section
with an open later section, partial streamed terrain and route-cache changes.
These are logical-layout replay and synthetic collision checks; they do not
constitute live terrain validation of other maps. The existing long/variable
Act 4/5 and prior forest collision tests remain applicable.

Build/testing and deployment state: [STATUS](../STATUS.md). Records:
../../re/spider-flayer-routing-2026-10-04/. The active game was only read;
no gameplay input, save/archive change, commit, push or release.

Deployment follow-up: user reports current behavior working as intended and
requested deploy/commit/push. Game/loader verified closed; local build.ps1 used
default four jobs/BelowNormal. Full 105/105 and affected 8/8 pass, timeout 20;
installed 39BC66B833FCD963D5761D3052D43BAE62435E7D327EF20B8098FDDECB878F91
matches the final build and tested staged code/data except four link timestamp
bytes. PRISM, loader configuration and exploration archive unchanged; backups
retained. Latest staged update is newly installed; no later live capture exists.

## Resident floor and exploration reach - 2026-10-05

The user confirmed the previous deployment still could not explore the rest of
Kurast and rejected ?no known route to the unexplored ground? as a substitute
for routing. That phrase is removed. The central assumption was incorrect:
entering a native room marked its entire rectangle visited, and H selected only
unentered rooms. A dock or jungle rectangle can contain several walking branches
and substantial water/scenery. Room visits and automap reveal are not floor
coverage. This shared room policy affected every act; Act 3 geometry exposed it.

H now calls `ExploreGoal::Next` for uncovered standable floor, including inside
entered rooms. Session-scoped `Coverage` records only actual terrain walking
cells within twelve steps and a clear body line of a player observation. It
leaves branches across walls/corners uncovered. Nearest measured floor comes
first, then speculative routes toward unloaded collision and openable doors.
A failed point is deprioritized, never used to exclude its whole room or declare
completion; failed measured points cannot hide fresh streaming frontiers. The
same implementation is used in Acts 1 through 5. Archived room visits cannot
pre-cover this new floor bitmap, so previously entered rooms can be surveyed
again. The bitmap follows the existing eight-level/session routing cache and is
not saved to the exploration archive.

Walking targets a floor point, not just any room entry. It uses current movement
collision to steer around live blockers, keeps measured collision after room
unloading, and automatically reselects new floor when fresh streaming changes
the route or a walk fails. There is no H room-refusal list or duplicated cropped
border veto. A complete route in the live local grid takes precedence over a
remembered-map cutoff. A closed door produces an approach to its actual unit;
the walk stops beside it for D, and H continues normally after it opens. Teleport
pads also retain their explicit D interaction and split-leg behavior. Actual
walls, body fit, logical-room holes and native level ownership still define
where movement can go.

`NativeWorld` supplements connected active-neighbor collision traversal with
other resident rooms in the current logical level, under the existing radius,
copy and room bounds. `LevelRoute::Memory::Extend` follows logical layout growth
without discarding existing measurements. A native Blank cell is absent
collision, not a measured wall: learning skips it, preserving prior floor and
leaving streaming frontiers unknown. Metadata never creates movement floor.
The T percentage now says ?percent mapped? in all thirteen locales, because
charted rooms do not prove exploration of walking branches. H completion refers
to reachable floor/frontiers rather than total logical-room visits.

The user's own 08:06-08:08 log reproduces the failed old H selection and the
48-room/25-visit/23-cutoff outcome. Two existing Spider Forest captures each
contain eleven rejected room rectangles with zero measured walking floor in
those rectangles (workspace `captured-components.json`); their rejection is not
proof that actual content lies there. A production captured-map regression
instead finds uncovered, locally reachable floor at 5196,4609, five steps from
5191,4607, inside rooms the old entry policy discards. This establishes the
coverage defect without inventing a route through water.

Regressions sweep both branches of a dock-shaped floor inside an already visited
room under all five act level IDs, follow a three-room corridor as preceding
collision unloads, approach/open a closed door, and continue to a streaming
frontier after a failed measured point. Native-reader bounds/foreign-level
checks and existing captured forest/border/pad tests remain. These replay and
constructed-map results are not live validation of every Act 3-5 map. The user
confirmed the deployed NPC/exploration fixes work in game and authorized
commit/push to origin/bladestorm. Current build/test/deploy state is in
[STATUS](../STATUS.md); records are under
`../../re/kurast-routing-rootcause-2026-10-05/`.
