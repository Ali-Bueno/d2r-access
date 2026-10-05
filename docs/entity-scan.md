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

## Kurast NPC omission investigation - 2026-10-05

The user's own 05:00-05:43 session lists only Meshif, Ormus, Asheara, Deckard
Cain and Alkor in town memory at each Kurast entry. Hratli and Natalya never
appear anywhere in the session log; additional NPC scanner entries are Iron
Wolves. The installed DLL hash matches the previous deployment. These are
reader/catalogue observations, not proof that any native game unit was removed.

Two gaps are confirmed in source, and remain unfixed:

- `NativeEntities::AddTownNpcs` assigns `TownNpcIdentity(classId)` only after a
  unique monster preset passes `NpcName` (npc flag, valid localized name).
  Loaded NPCs without that qualifying preset keep `placeId=0`. `MapSurvey::Split`
  consequently sends them through ordinary `Registry` memory, which can forget
  a unit when its former position is observed without it. In exploration mode,
  ordinary NPC memory is also discarded when changing levels. The promised
  town-NPC retention therefore covers only the preset-backed subset.
- `TownNpcSightings::Found` overwrites its snapshot with every accepted current
  entry; `Seen` prefers any matching current entry. Neither distinguishes a
  loaded unit from the `NoUnit` spawn fallback. Once a moved NPC unloads, the
  preset can replace its last observed live position with its original spawn.
  This yields a stale target rather than removing the stable town identity.

At 05:40:46 H said `Nothing left to explore here`; its diagnostic recorded
48 rooms, 25 visited, no given-up goals and 23 cut-off rooms. Cut-off count is
computed over all rooms, including visited ones. Earlier `seen=48` represents
charted/history rooms, not visits. These readings cannot establish that every
NPC location was visited, or explain either NPC's specific absence.

The game and loader were already closed, so no current native unit/preset
snapshot was available. The log lacks per-preset rejection diagnostics; the
exact reason Hratli/Natalya fail to appear remains unproved. Next diagnosis:
read only the user's current Kurast units/presets and their MonStats flags,
match identities independently of localized strings, and identify their actual
exclusion path before changing NPC identity rules. Preserve repeated guards,
discovery policy, native commands and the worker/thread split.

Existing `map-survey` and `native-entities` suites pass (2/2, timeout 20), but
cover unique preset retention and absent-current-entry memory, not live NPCs
without qualifying presets or live-to-preset fallback ordering. No production
source, build, deployment, game input, save/archive edit or commit/push in this
investigation. Own-session evidence and structured limits are retained under
`re/kurast-npcs-2026-10-05/` in the workspace.

## Moving town characters - 2026-10-05

The user reports that the previous identity/reader deployment did not restore
Hratli or Natalya. The concrete missing source is now identified without a live
snapshot: both characters are created by quest object markers, not ordinary
monster presets. The shipped `game-data/global/excel/objects.txt` rows 378/379
are HratliStart/HratliEnd (InitFn 49/50); row 382 is NatalyaStart (InitFn 52).
Installed quest handlers at RVA 0x5EB9E0 and 0x5EBC20 spawn MonStats 253 and 297
at these markers (workspace `re/decompiled/decompiled_0012.c`). The earlier
fallback reader ignored these object presets, so neither character had a
fallback when their current unit was not resident. Five other named town NPCs
remain in the user's latest own-session log; this is catalogue omission, not
removal of native NPCs.

`NativeLevelExits::ReadNpcMarkers` decodes only these native Kurast markers,
with Hratli's vendor end location preferred and his starting location retained
as an alternate approach. `NativeEntities` combines them with ordinary NPC
presets. These locations are search approaches, not assertions about an
unloaded NPC's current position. Loaded actual units always override them.
Live interactive town characters receive stable class identities independently
of presets, including friendly interactive rows without the npc flag; npc
rows do not require redundant friendly alignment. Same-class simultaneous
units remain distinct and repeated guard presets are not collapsed into one
quest character. Whole-level reads include current-level resident logical rooms
missing from the active neighbor ring. A dynamic path moving between room lists
is accepted when both rooms belong to the same level.

`TownNpcSightings` retains actual observations over fallback coordinates and
refreshes discovered characters from current units outside the discovery view.
On unloading it preserves the last actual position, carries the native alternate
approaches, and returns `NoUnit` rather than a stale interaction type. Scanner
walking refreshes before routing, searches the alternate approaches, then
uncovered town floor when the NPC has moved elsewhere. Floor search uses actual
arrival distance, not the wider unit-loading vicinity, to retain progress/stall
tracking. The first newly loaded matching unit replaces the search position.
An interaction whose NPC unloads returns to searching before issuing a native
command. Beacon refresh accepts actual units and ignores fallback coordinates.
Existing charted/in-view discovery and session reset remain; moving coordinates
are not persisted between games.

Tests reproduce a Kurast containing only Hratli/Natalya object markers and no
client units or monster presets, retain both Hratli approaches, prefer a moved
live unit, and accept its same-level path room. Other cases cover no/multiple
presets, repeated guards, isolated resident units, foreign levels, current
position precedence, unload/recreation, updates outside discovery and beacon
refresh. Build/deployment and current validation: [STATUS](../STATUS.md).
Records: `../../re/kurast-routing-rootcause-2026-10-05/`. The user confirmed the
new deployed NPC/exploration fixes work in game and authorized commit/push to
origin/bladestorm. The previous deployment's failure remains user-confirmed.
