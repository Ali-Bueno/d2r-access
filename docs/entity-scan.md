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
- A short right-stick click, or **Ctrl+B**, follows what the scanner last named
  (`docs/beacon.md`). Holding R3 and the L3+R3 chord are unchanged; L3 alone is
  no longer bound.
- Keyboard: Ctrl+Numpad 8/2 previous/next category, Ctrl+Numpad 4/6
  previous/next object, `,`/`.` step categories, `\` repeats.

## What is listed

- **Landmarks** — exits, portals, waypoints, NPCs, the stash, shrines, other
  interactable objects, quest containers and visited rooms — are remembered by
  the registry (`MapSurvey::Memory`). Exits and objects stay for the whole game,
  so they are still there after a town trip.
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
- **Rooms** are logical rooms of the level, read whether or not the game has
  them loaded (`NativeRooms::Read`). *Room* lists the rooms stood in.
  *Unexplored room* comes right after the ways out. In exploration mode it
  lists only the rooms bordering explored ground (`RoomCatalogue::Exploration`,
  `RoomGraph::Near`). Explored ground is a room stood in, drawn on the automap,
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
  Readouts give straight-line distance and screen direction. Full assistance
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
