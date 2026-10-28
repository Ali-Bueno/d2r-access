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
  diagonal push never changes both at once. Nearest by the distance each is
  spoken with (walking steps when known, else the straight line), one key for
  all, and a flick ten seconds after the last answer starts again at the nearest
  (`StaleSelectionMs`; a tester heard far ones first, 2026-10-12).
- In a menu, dialogue or pause the stick belongs to the menu (`RadarUi::InPlay`).
  While the settings list is open (BACK+RB) it steps the list instead.
- **BACK+LB**, or **Ctrl+B**, follows what the scanner last named
  (`docs/beacon.md`); on a newly named target it switches in one press.
  L3, R3 and the L3+R3 chord are the game's.
- **BACK + right stick down**, or **Ctrl+E**, describes the way there as legs
  ([route description](route-description.md)). With BACK held the stick never
  steps the scanner.
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
  as discovered is kept, in either mode, and so are quest objects (2026-10-14:
  the automap keeps showing them). Shrines, chests and portals are not kept:
  what they are, or whether they are there, changes between games (`TerrainService::RememberPlaces`/`KnownPlaces`,
  `docs/exploration.md#on-disk`).
- **Enemies** are discovered in range and in sight (all loaded ones with full
  assistance) and then remembered. A remembered enemy follows its live unit
  while the game has it loaded, is forgotten as soon as the game holds it dying
  or dead anywhere it is loaded (`NativeEntities::TakeDeadMonsters`), and at once
  when its place is within reach and it is not there. One that unloads alive
  stays at its last live position. This fixed a tester's phantom Fallen and
  Zombie in the Den of Evil (2026-10-11).
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
  them loaded (`NativeRooms::Read`). *Area* lists the areas stood in.
  *Unexplored area* comes right after the ways out. In exploration mode it
  lists every unexplored room of the level (`unexplored-everywhere`, below);
  with that switch off, only the rooms sharing an edge with explored ground
  that loaded collision does not prove walled off (`RoomCatalogue::Exploration`,
  `RoomGraph::Touching`/`Border`, `docs/room-routing.md`). Explored ground is a room stood in, drawn on the automap,
  or underfoot (`TerrainService::ExploredRooms`). This is the edge of the known
  map, so a lost player can head somewhere new. Nothing inside an unexplored
  room is revealed: its exits appear once charted, like any landmark. Full
  assistance lists every room never stood in. With `unexplored-spread` (Ctrl+S,
  on; a tester, 2026-10-21) the list takes one per screen direction in turn,
  nearest first in each round (`MapSurvey::SpreadByDirection`): the same areas,
  only the order changes, so a player picks a way rather than three neighbours.
  `unexplored-everywhere` (Ctrl+S, Navigation, on; a tester, 2026-10-25, made
  the default by the user, 2026-10-26) lists every unexplored room of the level
  in exploration mode, far ones included; a route to one guesses across ground
  not seen yet (`docs/beacon.md`). Off restores the user's rule of 2026-09-24,
  the edge only. Percent explored keeps its own edge either way.
  **Either way, and in full assistance, an unexplored room the kept map shows no
  way into is left out** (`LeaveOutShutRooms` in `map_survey_service.cpp`:
  `KeyboardService::CutOff`, the whole-level `LevelRoute::Reach`, unseen ground
  and doors passable, teleport pads too). A tester's log, 2026-10-27: in the
  Outer Cloister eight of the listed areas, rows 0-1 of its 7x5 grid of rooms,
  were the monastery's roofed blocks, every cell wall (`0x0005`), so each answered
  "No route through explored areas" and none could ever be explored. What the
  map cannot tell, a room past ground not seen yet, stays listed. Log, on a
  change: `[survey] level L: N unexplored rooms with no way in from x,y left out: ...`.
- **Ground loot and ordinary chests are not listed.** They are not on the
  automap, and their loops and step-on speech already cover them
  (`docs/ground-items.md`). Quest containers stay, as the automap draws them
  (`MapSurvey::Split`).
- In exploration mode (the default) a landmark is listed once it stands in an
  explored logical room: one stood in, the one underfoot, or one the stock
  automap has drawn in, meaning at least one floor or wall cell
  (`TerrainService::ExploredRooms`, `MapSurvey::Charted`). A sighted player sees
  a room whole on stepping into it (the user, 2026-10-14). Drawn terrain is
  saved with the character, so these landmarks are listed from the start of a
  later game. A landmark that has been in range and in sight
  (`MapSurvey::Discoverable`) is listed too. Enemies and loot are listed in
  sight or in the room underfoot. Whatever is listed can be routed to
  ([exploration mode](beacon.md#exploration-mode)).
  Readouts give straight-line distance in units and a screen direction as one of eight compass
  points or twelve clock hours ([spoken directions](radar-readings.md#spoken-directions-2026-10-13)). Full assistance
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

## Town NPCs

In a town (`Levels.txt` town flag) the NPC category lists every NPC row
(MonStats `npc` column) the level's presets place exactly once; a row placed
more than once, such as a guard, is left out. The loaded unit is listed where
there is one, else a stand-in at its spawn point with the same identity
(`TownNpcs::Identity`, the row + 1), so a beacon on one follows the other. A
spawn in the player's room or a neighbour with no unit there is left out: the
NPC is not there. Other loaded rooms prove nothing: in a Lut Gholein snapshot
(bladestorm360, `bdb7e91`, branch `port/bladestorm-npcs`) 25 of 49 rooms were
loaded, with Fara, Cain and Meshif presets in them and no unit read. Switch
`town-npcs-everywhere` (Ctrl+S, on by default) or full assistance lists them
all; off, only those seen (`TownNpcSightings::Listed`). NPCs that wander
(Warriv, Gheed, Ormus) are found near their spawn. Code: `src/town_npcs.{h,cpp}`,
`NativeEntities::AddTownNpcs`.

An NPC the presets never place takes the row's identity while it is the only
loaded unit of that row. `TownNpcSightings` keeps the last seen entity per
identity, so once seen it stays listed from anywhere, at its last place, as a
stand-in with no unit; the live unit takes over when loaded again. While a town
lists all its NPCs, every loaded one counts as seen wherever it stands
(`TownNpcsFound`, 2026-10-25): Jerhyn, loaded on arrival at the waypoint, used to
drop out a few steps later because his spot was not charted.

### NPCs at marker objects (2026-10-25)

Jerhyn, Kaelan, Hratli, Natalya, Anya, Nihlathak and Larzuk are no monster preset
(Lut Gholein's list holds no 201, Kurast Docks' no 253 or 297). The town's DS1 places
an invisible marker object whose objects.txt `InitFn` spawns the NPC when the
marker's room is built (`tools/gen_npc_markers.py`: the object's Class starts with
the NPC's MonStats Id; `src/generated/npc_markers.h`). `ReadMonsters` also returns
the object presets; `TownNpcs::MarkerSpawns` turns a marker into a spawn when
`NpcSpawnRules::Spawns` says its InitFn would spawn now, from the player's quest
flags (`NativeQuestFlags`), and Merge lists it as any preset.

The init functions, decompiled from `re/D2R_live.exe` (D2R 3.3.93787). The dispatcher
`0x510CB4` indexes a table at `0x239B980` filled at run time, so slots were matched by
content (MEASURED tests, INFERRED slot numbers). Each tests the **game's** quest record
through `0x514FD0(game, row, bit)` (`game+0x21D8 -> +0x10`, bit 13 primary goal done);
the mod reads the player's, and also takes bit 0 or 15 for done.

| InitFn (row) | Function | Spawns while | Mod's rule |
|---|---|---|---|
| 18 JerhynPlaceholder1 (121) | `0x5161B0` -> `0x59E5A0` | rows 8 and 12 bit 13 clear, Seven Tombs (row 14) not begun | rows 8, 9, 12 not done, row 14 neither begun nor done |
| 19 JerhynPlaceholder2 (122) | `0x5161F0` -> `0x59E6D0` | Jerhyn unless rows 8 and 9 clear and Seven Tombs not begun (at x+15, or x-10 before it, y-3); Kaelan at x+1 while row 14:13 clear | the opposite of 18 |
| 49 HratliStart (378) | `0x5EB9E0` | row 16 (Act III intro):13 clear | row 16 not done |
| 50 HratliEnd (379) | `0x5EB870` | row 16:13 set | row 16 done |
| 52 NatalyaStart (382) | `0x5EBC20` | row 22 (The Guardian):13 clear | row 22 not done |
| 66 DrehyaInTown (459) | `0x547A40` | quest data `+0xA4 == 2`: after the thaw, on talking to Malah | row 37 bit 0 or 15 |
| 68 NihlathakInTown (461) | `0x548EE0` | Prison of Ice data `+0xA8 == 0` (below) | row 37 without bit 0 or 15, none of levels 112-114, 121-124 entered this game, no other player |
| 70/71 Larzuk (542/543) | `0x548A40` (one slot) | once, Siege data `+0x3D == 0` | none: slot unknown |

**Nihlathak (2026-10-27, `re/` decompile by an agent, MEASURED unless noted).** Harrogath's
`expansion/Town/townWest.ds1` places him as monster preset 514 at subtile 76,126; the map loader
`0x3DFAE0` turns it into object 461, so the mod reads marker 461 (and Natalya's 297 likewise
becomes 382). The Prison of Ice's quest data (one 0x318-byte object per quest off `game+0x21D8`,
set up by `0x5483E0`) is zeroed every game. `+0xA8` becomes 1 when the game's first player joins
with row 37 bit 0 or 15 (`0x54D280`), on entering 121-124, on entering 113 or 114, on entering 112
while the quest has not begun (`0x54CB40`), and on finding Anya (`0x54A2D0`, which also removes his
town unit). The Betrayal of Harrogath (row 38) never touches it. The mod's rule
(`NpcSpawnRules::NihlathakHome`) takes 112 as always taking him (it may hide him when he is there,
never the reverse), counts the levels its own reads have been in this game (`GameLevels`, by
session), and gives no answer when other players are in the game, whose creator and walks it cannot
see. InitFn 69 (462) is in the Halls of Vaught's `wildtemple/Nihl*.ds1`, never in town.

A marker with no rule gives no spawn: that NPC is listed once seen, as before. Kaelan
is not placed from the marker (one NPC per marker); he too is listed once seen. Live
check: stand at Atma and at the Kurast stash; `[survey] town NPCs` should name
`Jerhyn@x,y/spawn` and `Natalya@x,y/spawn`, with `201x1` and `297x1` in the presets list.

### Presets of rooms not built yet (fixed 2026-10-12)

A tester at Charsi, in the Rogue Encampment, heard only Charsi, Warriv and
Kashya. The reader took presets from the rooms (`RoomEx+0x98`), which the game
fills only when it builds a room. Until then the map holds them. Evidence: D2R
3.3.93787, Ghidra `re/ghidra` `D2R_live`; fingerprints in
`preset_map_layout.h` `PendingChecks`.

| Native function | Fact |
|---|---|
| `0x3DE0E0` (a room's preset setup) | The first room of a map to be set up loads the DS1 (`map+0x10`) and calls `0x3DF600` once, for the whole map. |
| `0x3DF600`, 813 bytes | Copies the DS1's presets (`file+0x98`) onto `map+0x58`, through `0x3DBB60`. |
| `0x3DBB60`, 393 bytes | The copy: class `+4`, type `+0x20`, X `+8` / Y `+0x24` plus the map origin (`map+0x18/+0x1C` tiles x5), so level subtiles. |
| `0x3DE420`, 692 bytes (room build `0x3F38D0`) | Moves each `map+0x58` preset inside the room's subtile rectangle to `RoomEx+0x98`, minus the room origin. |
| `0x3DDBB0`, 634 bytes | Frees `map+0x58` only with the level's maps (next `+0x88`). |

The two lists never share a preset and together hold all of the map's.
`NativeLevelExits::ReadMonsters(..., unbuilt)` reads both. A map preset goes to
the room whose rectangle holds it, where the build will put it. The map is
`RoomEx+0x40 -> +0x08` (`docs/places.md`). A malformed map list drops only the
map presets. Live check: `[scan] Verified unbuilt-room preset reader.` at
startup, and `[survey] town NPCs on level <n>: <total>, ...` naming every town
NPC, the far ones as `/spawn`.

## Natalya in Kurast Docks (open, 2026-10-20)

A tester at the docks' stash could not find Natalya until near her. Kurast Docks is one preset
(LvlPrest 529, `DockTown3.ds1`), so her preset should come with the map's. Candidates: her row
placed twice (`TownNpcs::Merge` drops a row placed more than once), or her preset missing from
the map's list. `[survey] town NPCs on level 75` now ends with the town's NPC presets as
`row x count` (`NativeEntities::TownPresets`; Natalya is MonStats row 297) to tell which.
