# Level scanner (right stick)

One scanner on the right stick lists what is on the level, grouped in
categories. It replaces both the directional entity scan and the L3
long-range pathfinder, which were merged on 2026-09-24 at the user's request
(`docs/decisions.md`). Its goal is to read what a sighted player finds on the
automap; enemies are the one live category.

## Controls

- During play the right stick always drives the scanner: **up/down** step
  through the categories that have something in them (the category name is
  spoken first), **left/right** step through that category, nearest first.
  One dominant axis per push (`InputDirection::TakeRightStickPush`), so a
  diagonal push never changes both at once.
- In a menu, dialogue or pause the stick belongs to the menu (`RadarUi::InPlay`).
- A short left-stick click (L3), or **Ctrl+B**, follows what the scanner last
  named (`docs/beacon.md`); on a newly named target it switches in one press.
  R3 and the L3+R3 chord have no mod gesture since 2026-09-25.
- Keyboard: Ctrl+Numpad 8/2 previous/next category, Ctrl+Numpad 4/6
  previous/next object, `,`/`.` step categories, `\` repeats.

## What is listed

- **Landmarks** — exits, portals, waypoints, NPCs, the stash, shrines, other
  interactable objects, quest containers and visited rooms — are remembered by
  the registry (`MapSurvey::Memory`). Exits and objects stay for the whole game,
  so they are still there after a town trip.
- **Fixed places** — exits (stairs, level passages, area borders), waypoints
  and the stash (`MapSurvey::Fixed`) — are also kept on disk with the map they
  were found on, keyed like the explored rooms by map seed, level and room
  layout. A later game on the same map lists them from anywhere on the level,
  before the game streams their rooms again. A regenerated map has another key,
  so its places appear only once discovered again. Only what exploration counts
  as discovered is kept, in either mode. Shrines, chests, quest objects and
  portals are not kept: what they are, or whether they are there, changes
  between games (`TerrainService::RememberPlaces`/`KnownPlaces`,
  `docs/exploration.md#on-disk`).
- **Enemies** are never remembered: each answer lists the enemies in range and
  in sight right now, nearest first.
- **Players** (multiplayer) are the other living characters in the game, read
  live like enemies but not filtered by sight, by name, nearest first, so a
  teammate can be found and followed with the beacon. Anyone standing in the
  player's room or a loaded neighbour is read from their unit. A **party
  member** elsewhere on the same level is placed where the game's party roster
  says, the same position the automap marks. The server only sends that
  position when the teammate is more than 50 steps away; how often it is
  refreshed has not been measured. Players outside the party, or on another level, are not
  listed (`docs/entity-units-re.md` §12).
- **Corpses** (`cadáver`, right after players) are every player's body: "your
  corpse", or "X's corpse". Once read nearby, a corpse is listed from anywhere
  on its level for the rest of the game, in both modes and without the sight
  rule, until its place is read again without it or its owner takes it back.
  A party member's corpse is placed even unseen, on any level, from the party
  roster at the moment it appears (`CorpseMemory`, `docs/player-corpse-re.md`).
- **Rooms** are logical rooms of the level, read whether or not the game has
  them loaded (`NativeRooms::Read`). *Room* lists the rooms stood in.
  *Unexplored room* comes right after the ways out. In exploration mode it
  lists only the rooms sharing an edge with explored ground that loaded
  collision does not prove walled off (`RoomCatalogue::Exploration`,
  `RoomGraph::Touching`/`Border`, `docs/room-routing.md`). Explored ground is a room stood in, drawn on the automap,
  or underfoot (`TerrainService::ExploredRooms`). This is the edge of the known
  map, so a lost player can head somewhere new. Nothing inside an unexplored
  room is revealed: its exits appear once charted, like any landmark. Full
  assistance lists every room never stood in.
- **Ground loot and ordinary chests are not listed.** They are not on the
  automap, and their loops and step-on speech already cover them
  (`docs/ground-items.md`). Quest containers stay, as the automap draws them
  (`MapSurvey::Split`).
- In exploration mode (the default) a landmark is listed once it stands in a
  logical room the stock automap has drawn in, meaning at least one floor or
  wall cell (`NativeAutomap::RevealedRooms`, `MapSurvey::Charted`). Drawn
  terrain is saved with the character, so these landmarks are listed from the
  start of a later game. A landmark that has been in range and in sight
  (`MapSurvey::Discoverable`) is listed too. That test is also the whole rule
  whenever the automap cannot be read (`docs/automap-reader.md`).
  Readouts give straight-line distance and one of eight screen directions (diagonals such as "arriba y a la izquierda" included, `Radar::ScreenSector`). Full assistance
  lists everything loaded with walking steps (`docs/beacon.md#exploration-mode`).

## Code

- `src/map_survey.{h,cpp}`: `Split`, `Discoverable`, `Registry`/`Memory`,
  `Survey`, category order and cycling. Tested by `tests/map_survey_tests.cpp`.
- `src/map_survey_service.cpp`: input (`PollStick`, keyboard actions), world and
  unit reads, speech, and the beacon offer. Log prefix `[survey]`.
- `src/entity_radar.{h,cpp}`: shared entity types, `LineOfSight` and
  `DistancePhrase`. The native unit reader is `src/native_entities.cpp`
  (`docs/entity-units-re.md`). Unnamed placeholder units (`"dummy"`, "una
  fuerza maligna") are dropped there, before any reader sees them.

## Town NPCs after unloading and reload ? 2026-10-04

The 2026-10-03 integration moved town NPCs out of `MapSurvey::Memory` into
`TownNpcSightings`, but retained only seen identities. Its query still required
the NPC in the current native scan; full assistance bypassed even that memory.
This regressed the local rule that a discovered target remains in the list.
The user's current log shows Meshif at 5208,5072, then absent after returning
toward the waypoint. `TownNpcSightings` now retains entity snapshots per town
for the session, refreshes a known NPC from its current unit, and returns its
stored position when no current entry exists. Full assistance feeds all current
town entries through the same memory; exploration still requires discovery.

Reload was a second suppression in `NativeEntities::AddTownNpcs`: a unique
NPC preset was omitted when its logical room had been walked by the reader but
its live client unit was absent. Loaded collision does not establish NPC-unit
presence. At reload the user's log records all 49 Lut Gholein rooms explored,
then only Warriv in the town scene. A bounded read-only snapshot of that game
found 49 logical rooms, 25 active, with unique NPC presets including Fara,
Deckard Cain and Meshif even in active rooms. The native reader now uses the
current preset as a fallback regardless of room activation. A loaded unit wins;
repeated class presets (guards) and non-town presets remain excluded.

This initial list uses current game data and the existing discovery/automap
rule. Old game unit ids and moving NPC positions are not restored from disk.
The town memory clears at a new game; native presets repopulate it. Other
catalogue memories, the approved Ctrl+U reachability filter and pad routes are
unchanged. The town diagnostic now distinguishes currently available entries
from remembered entries.

Regression checks: `map-survey` retains Meshif when missing from the waypoint's
scan, updates positions, scopes memory to a town and clears on a new game.
`native-entities` tests a unique preset in a walked active room without its
client unit, live/preset identity, duplicate guards and non-town exclusion.
Workspace evidence: `re/skill-hotkeys/town-native-snapshot.json`. Live listening
and the initial reload list remain to be confirmed after deployment.
