# Exploration: the unexplored mark, the two memories and their disk form

The mod's exploration history, in sound and in a word: which way out has not been
taken, where the ground the game has never streamed begins, how much has streamed.
This is not a verified reading of native automap discovery; see the
[2026-09-23 investigation](automap-discovery-research.md).
Design conversation of 2026-09-21; the user's rule for the radar still holds (four cardinals, the
original cues, only changes sound).

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
  `NativeLevelExits::Identity` of its position. Since 2026-10-14 (a tester: the Cairn Stones were
  gone from the scanner after a restart while the automap still showed them) `KnownPlaces` offers
  them again, as quest objects; a loaded object standing on the same spot replaces it. A Cairn
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
(`docs/entity-scan.md`). One position is one place: a known place only takes a
newer name, destination or kind, so the loaded warp and its preset (different
id, fraction of a subtile) never rewrite the archive. A stored place whose kind
and category are not a fixed place is skipped on use. `D2REXP01` archives still load and are
upgraded on the next save. An older plugin cannot read `D2REXP02`; it leaves the
archive untouched and cannot load or save exploration until updated.

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

## Discoveries after the banner, by their full name (2026-10-20)

A tester: arriving in the Canyon of the Magi, the waypoint beside the arrival was never said. A place
found in the first 2.5 s of a level (`TerrainService` `ArrivalHoldMs`) was said at once and cut by
the area's banner, which interrupts; such finds now wait and are said after that window. A find
is said by the scanner's own name, so a tomb carries its symbol ("Tomb of Tal Rasha: Double
Chevron"); it is still stored by its plain name. Unverified live.
