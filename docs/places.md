# Named places (2026-10-02)

Walking into a named part of the map speaks it, without interrupting:

> *"taberna, barra"* … *"cocina"* … *"fuera, puestos"* … *"puente"*

Three layers from the game's own data, nothing drawn by hand, plus corridors read from the
walkable grid ([below](#corridors)).

| Layer | Example | Source |
|---|---|---|
| Named map piece | puente, río, campamento de los Caídos, cementerio, forja | the LvlPrest row the room was cut from |
| Interior | taberna, entrada del palacio, cabaña, choza, templo, fortaleza | acoustic and lighting volumes in the piece's HD layout |
| Landmark in reach | barra, cocina, chimenea, mesas, escaleras, muelle, puestos, trono, altar, celdas | props in the piece's HD layout, nearest by walking distance |

Each change is spoken once, in that order. A named piece is spoken when its kind changes. The name
holds on plain ground within one piece's size of the last named piece, so a river built from a
chain of pieces is one "río" (seen live on the Blood Moor: twice in 5 s before this). Inside one interior, only the new landmark is spoken.
The floor between two landmarks keeps the last one. Leaving an interior says *fuera* plus whatever
is outside. The room service's hysteresis gate (`Rooms::BasicHysteresis`) keeps a threshold from
chattering. The service polls every 250 ms. It is silent in menus and dialogue, and waits while
the UI reader is speaking.

The keyboard settings list toggles it. Ctrl+U retains the reachability filter on this branch. `access-places on|off|status` does the same; `access-places where` speaks
the whole place at once. Each spoken line logs one `[places]` line: the DS1 path, Def, origin,
interior, landmark and seed.

## The map under the player

Every room the game builds from a preset knows which map it was cut from. Ghidra on D2R 3.3, all
offsets in `src/preset_map_layout.h` with their fingerprints:

- `RoomEx+0x74` is the room type: 1 an outdoor filler cell, 2 a room cut from a preset map (mazes too).
- `RoomEx+0x40` → preset room data; `+0x08` → the `DrlgMap`.
- `DrlgMap+0x00` LvlPrest Def, `+0x04` the File index the generator picked, `+0x08` the LvlPrest
  record, `+0x18..+0x24` origin and size in tiles (the tile space of `RoomEx+0x60`).
- The record keeps File1..File6 inline, 60 bytes each, at `+0x40`. So the DS1 path is
  `record + 0x40 + file * 0x3C`. It tells the two Lut Gholein layouts apart. The game prefixes the
  tiles folder when it loads the table. Seen live: `data\global\tiles\Act2\Town\LutW.ds1`, Def 301,
  file 1, origin tile 1000,1000. The lookup drops that prefix.
- The LvlPrest Name ("Act 1 - Bridge") is not in memory. The generator copies Def → place from
  `game-data/global/excel/LvlPrest.txt`.

Named outdoor pieces (the bridge, the Fallen camps, the cottages) are placed by the outdoor
generator (`0x3EE780`) as preset maps, so their rooms carry the link. Whole levels (towns,
the Act 3 temples, Tower 2) are left out: the level name already says it.

## The HD layouts

For every DS1, the game ships `data/hd/env/preset/<path>.json`, the 3D layout of the HD renderer.
Every prop is an entity with a model path and a position. Two facts make it usable:

- **Coordinates.** HD `x / 2` and `z / 2` are the DS1's subtiles, with (0,0) at its origin; `y` is
  height. Checked on Lut Gholein: 95% of the HD wall models land on DS1 wall tiles, against an 11%
  wall density. Swapped or mirrored axes score 9–20%.
- **Names.** Models and sounds are named for what they are: `barstool02`, `table_kitchen01`,
  `SFX_Emitter_Fireplace_Loop`, `anvil01`, `celldoor01`, `bonethrone01`. Interiors carry volumes:
  `AudioEnvironmentOverrideComponent` (`ESOUNDENVIRON_TOWN_2_BAR` is Atma's tavern) and
  `VisualDataBoxDefinitionComponent` (`visbox_cottage`). Boxes are relative to the entity and scaled
  by it; none is rotated.

`tools/gen_places.py` turns them into `src/generated/place_data.h`. It classifies the props with one
regex per landmark kind, merges props of one kind within 2 subtiles, and merges overlapping volumes of one
interior kind (a tavern's acoustic zone and lighting box are one room). Prefab placements are classified by their prefab name, with the footprint below. The output
covers 578 pieces, 2418 landmarks and 82 interiors (`--hd-root`, prefabs extracted with `"\pf_"`). Its docstring has the extraction command; the run prints
the matched model names per kind, so a false positive is visible after a game patch.

## Footprints and walking distance

A landmark is an area, not a point. The user asked to hear "forja" when passing through the forge,
not near one point of it. A prefab's footprint is the extent of the entities inside its JSON, placed
with the prefab's own position, scale and yaw. `pf_town_blacksmith01` is 31×30 subtiles, and its
origin sits at one edge. The camp tents are 10–26, the wagons about 17×12, and `pf_town_campfire01`
29×34 (the camp centre around the bonfire). A loose model is a 1×1 point. Footprints that do not
contain their own anchor fall back to the point (71 of 507 prefab placements). Checked against the
DS1 spawns in `TownN1`: Charsi stands in the forge yard, Akara in her tent, Gheed by his wagon.

Being at a landmark means standing on its footprint, or within one map tile of it by a
breadth-first walk over the live collision grid (terrain layer, doors open). A straight-line
distance would name the kitchen through the kitchen wall. A footprint that blocks (the forge, a
stool at the counter) is reached from any cell around it. The Crypt's burning star is told from
40 cells (`Places::Reach`, a whole 8x8-tile piece): at one tile it came when the tester was almost
on its points (2026-10-19). Only props in the player's interior count;
outside, only props outside every interior. At equal distance the earlier kind in the enum wins.
Replaying the user's camp walk from the log: before footprints, only "forja", at Charsi. With them:
hoguera, forja, hoguera, carro, tiendas, carro, tiendas.

## Corridors

Corridors, bends, junctions and dead ends have no name in the game's files; `src/passages.cpp`
reads them from the shape of the live collision grid (terrain layer, doors open), in maze levels
only. D2 walls follow the world axes, so a corridor runs along one of them.

- **Corridor:** walls on both sides no further apart than a corridor, for at least a corridor's
  length along the axis. A doorway through a wall is too short; a slit along a wall too narrow.
- **Dead end:** following the corridor along its axis, an end walled while still narrow
  (*pasillo sin salida*). Ground not streamed yet is never a wall: that end is unknown.
- **Bend / junction:** a small patch of wider floor with two corridors leaving at a right angle,
  or three or four (*recodo*, *cruce*).
- **Towards:** for each open end, the level exit nearest it (within two corridor widths), or else
  the landmark of this piece there: *pasillo, hacia Escalera a Catacumbas nivel 3*.

Walking out of a room into a corridor speaks it; out of a junction into one of its arms, only
when the arm is shut or leads somewhere named. Leaving a corridor says nothing.

**The yardstick is the level's own.** Offline masks rebuilt from the DS1/DT1 files measured
corridors about 20 subtiles wide in the Catacombs, Crypt and Worldstone Keep, 10 in the sewers, 9
in the Kurast temples, and 6–8 for Lut Gholein's streets. No single width fits. So every
measure scales with `Rooms::TypicalOpeningWidth` (the median gap between the maze's streamed
rooms): width from half of it to it plus 2, length at least it, followed 6 widths each way.
Those offline masks also showed walls the game does not have (DT1 rarity variants OR-merged),
so the final tuning has to come from the live grid. `access-places dump` writes it to the log, one
row per line (`#` wall, `.` floor, `?` not streamed, `@` the player), with the typical width.

## Coverage and limits

- Lut Gholein: tavern (kitchen, bar, fireplaces, tables), palace entrance (stairs), docks, market
  stalls, Warriv's wagon. Kurast Docks: Asheara's and Alkor's huts, docks, boats. Pandemonium Fortress,
  Harrogath's building (Malah), Act 1 cottages and tower, Kurast huts and temples, farmhouses,
  the cathedral. Barracks forge, jail cells, the Catacombs' bone throne, graves, bookshelves.
- The Act II desert's cliff stairs (Dry Hills, Far Oasis), the only way between a level's two
  halves (a tester, 2026-10-17): feature `CliffStairs`, said "stairs", one per `act2/bigcliff/cliffrp`
  and `clifflp` piece (LvlPrest "Act 2 - Desert Cliff Right/Left Path", models
  `act2_bigcliff_stairs/stairs_right01`, `stairs_left01`). It is also a tracker entry ("object"):
  `Places::Tracked` features of the maps of the loaded rooms (`NativeWorld::LoadedRooms`, the near
  lists) become scanner entities with no unit (`PlaceService::MapLandmarks`, `Entity::mapLandmark`),
  kept with the map like exits (`MapSurvey::Fixed`), so the first sight says "The radar has logged
  a new place: stairs".
- Prefab placements count too (`pf_town_blacksmith01` is Charsi's forge, at her spawn in all four
  camp layouts; Akara's tent; Gheed's wagon; Kurast bar counters; Barracks cells).
- The palace, harem and other indoor levels have landmarks but no interior volume of their own.
- Corridors only in maze levels: towns, open ground and preset levels (the Arcane Sanctuary, the
  Harem) have no doorway width of their own to measure by.
- Words for 13 languages live in `src/strings/<locale>.h` (`placePresets`, `placeInteriors`,
  `placeFeatures`). Spanish and English were checked; the other eleven are first-pass
  translations a native speaker should review.

## Cottage kinds, Tristram, the crypt fire, stairs both ways (2026-10-18)

A tester (build 2026-10-17): every Act I building was "cottages", Tristram's blood fountain was a
plain fountain and its fires were silent, the crypt fire star and the cliff stairs' upper end said
nothing, and the Hangman's Tree only spoke at its trunk. `tools/gen_places.py` now:
- names a piece by its **file** where its LvlPrest row picks among kinds (`PresetFileNames`):
  "Cottages 1/2" (Def 47, 48) are *cottage*, their sixth piece (`Cott6`, `Cott6A`: hay, manure,
  a cow's stall) is *stable*, "Cottages 3" (`Cott7`: burnt planks, Tristram's fire models) is
  *cottage on fire*; the tower pieces (Defs 162, 163) are *tower ruin*;
- adds **BloodFountain** (`fountain_gore`, which drops the plain fountain models on its spot),
  **BurningBuilding** (Tristram's fire and smoulder effects, burnt beams and planks; also in the
  Act V ruins; not in `Cott7`, whose name says it) and **FireStar** (`FX_Act1Crypt_fire_pentagram`
  in the crypt themes `CryptSEWTheme`, `CryptNSEWTheme`; a call-out, not a scanner entry);
- pads the **Hangman's Tree** seeds one tile (`FEATURE_PADDING`), about its canopy;
- places the **cliff stairs** from their collision mesh (`stairs_*_collision.physics`: header
  word 6 is the node count, 32-byte bounding-volume nodes from 0x30): the nodes lowest at their top
  are the bottom landing (*stairs up*, `CliffStairs`), the highest at their base the top landing
  (*stairs down*, `CliffStairsTop`). Both are scanner entries; walking from one to the other is no
  news (`Places::Stairway`). Extract `act2_bigcliff_stairs\` with the presets.

## Buildings named each time, the tower from afar (2026-10-18)

A tester (build 2026-10-18): a second cottage next to the first was silent, the stable then said
"cottage", the tower ruin was named only at its feet, the Lut Gholein sewers had "tables" that are
crates, and Cottage 1's porch building had no name of its own. Causes and fixes:
- **Held names.** A named piece's name holds on the plain ground around it (`Remember`), which is
  right for a river but silenced every cottage after the first in a row. `Places::Separate` kinds
  (cottage, stable, cottage on fire, cottage with porch, tower ruin) now carry the map they were earned
  in (`Spot::namedMap`): the next such piece is named again, and the held name lapses after
  `SeparateHoldMs` so coming back to a building speaks it again. Rivers and bridges keep one name.
- **Stable.** `Cott6`/`Cott6A` carry the cottage's `visbox_cottage`, which said "cottage" on stepping in.
  `INTERIOR_SKIPPED_FILES` drops that volume, so a stable is only a stable.
- **Cottage with porch.** By file like the stable: `PresetPlace::CottageWithPorch` (corrected
  2026-10-19, below).
- **Tower ruin range.** `PRESET_PADDING` (`PresetPads`) names the tower 6 tiles before its piece, read
  off the loaded rooms every 2 s (`PlaceService::RefreshPadded`).
- **Sewer "tables".** The prefabs `pf_table01/02` under `sewer/prefabs/perimeter_deco` are piles of
  crates, vases and rags; `PREFAB_NOT_FEATURES` stops them being tables (30 seeds, 20 sewer pieces
  left with no landmark). The word is ours (`placeFeatures`), the classification was wrong.
- **Cliff stairs, both ends.** The landings were one bounding-volume node each, the south half of
  a stairway about 12 subtiles wide; the tester climbed its north side, 6-8 subtiles from the top
  rect, so "stairs down" never came and the way down said "stairs up" at the bottom. Each landing is
  now every node within `STAIRS_LANDING_SLACK` (one subtile of height) of the lowest top or the
  highest base, the full width (Dry Hills' left stairs: bottom 30,43-39,55, top 20,45-25,56). A
  stairway left `StairwayHoldMs` ago no longer holds (`Places::HeldStairway`): coming back to the top
  says "stairs down" again, while walking straight from one landing to the other stays quiet.

## The porch cottage's file, the sewer grate (2026-10-19)

A tester (build 2026-10-18-2): `Cott1A` in the Cold Plains and the Black Marsh said "cottage with
porch", but it is their plain "Stone Cottage" (screenshot "Cottage 2"). Each `CottN`/`CottNA` pair
places the same models at the same spot (no rotation): `cottage0N` under `roof0N`. The porch
screenshots (Dark Wood) match `Cott5A` by time in `hidden_cottage_d2r-access.log` (log times run five
hours ahead of the files), so **`Cott5`/`Cott5A`** (`cottage05`, the larger `roof05`) are the porch
cottage and `Cott1`/`Cott1A` plain cottages. `Cott2` to `Cott4` stay "cottage": nothing shows a porch.

**Sewer grate.** The round floor grate with steam is `act2_sewer_manholes/manhole01` (entity
`manhole01_01`, with `sewer_well_trim01` and `rising_steam01`), placed directly in 20 sewer pieces and
through the prefab `pf_steamy_grate01`; feature `SewerGrate` ("sewer grate"), 22 seeds. In `SewNSW`
it is at subtile 37,33, where the tester walked round it (`sewer_d2r-access.log`). The flat
`water_channel_grate01` along the channels is not matched; `pf_sewer_grate` is Lut Gholein's street
grate, left for later. The "tables" heard there is `bench01_03` in a pile of broken furniture.

## Porch cottage from the southwest, Outer Cloister fountain (2026-10-20)

**Porch cottage.** From the southwest a tester heard only "cottage": `Cott5`/`Cott5A` carry a
`visbox_cottage` volume (14,15-32,32 subtiles) that said the interior word, while the piece word
"cottage with porch" was not heard. `INTERIOR_SKIPPED_FILES` now skips the cottage interior for
`cott[56]a?`, so the porch cottage says one word, its own. Not reproduced in game: the code reading
(preset word only on a piece change, interior word on a volume change) fits but the walk was not traced.

**Outer Cloister fountain.** The courtyard's Rogue fountain (`objects.txt` *ID 12 `Fountain`,
"RogueFountain") is not in the HD layouts: `act1/court/courtw|n|e.json` hold no fountain, statue or
plaza model (only walls, walkways, bushes and 26-29 `act1_courtyard_water/water_plane` tiles spread
over the whole level). It is an object placed at runtime, a target in no mode, so the generator
cannot name it. `NativeEntities` reads it anyway as a map landmark (`EntityLayout::RogueFountainClass`,
"Rogue fountain"): it is listed in the scanner, found ("The radar has located a new place") and
guided to like the desert's cliff stairs. Unverified live: that the fountain is that object.

## One stairway, and the Countess's cistern (2026-10-22)

**The cliff stairs.** The tester's log (`precious_*`): the route to "stairs up" (the bottom
landing's centre, 4395,5529 in the Dry Hills) went "off the route" five times on the stairs' bend,
with legs of 1 to 6 units, while routes to items dropped by either landing worked. At their
request a stairway is now one scanner entry, "stairs" (`Places::Listings`): halfway between the
two landings' centres, with a footprint as wide as the way between them, so the beacon's arrival
distance (3 plus half the footprint, `BeaconService::SettingsFor`) takes either end and the middle.
The walk-in announcer says either landing as "stairs" (`Places::Spoken`) from two tiles off
(`Reach`), and still not again when walking from one landing to the other.

**The Gilded Cistern of Virgins.** The tester asked for the Countess's blood-filled vessel in Tower
Cellar Level 5 as a landmark: the doors on either side of it lead to her room. Its only candidate in
her two layouts (`act1/crypt/cryptcountess1.ds1`, `cryptcountess2.ds1`, LvlPrest Def 159) is the
model `act1_crypt_countess/ritual_platform01`, with the room's blood pool and bloodstain decals
round it; nothing is named bath, basin or chalice. Feature `BloodCistern`, padded 7 subtiles (the
platform is about 14 across and blocks; its seed is one point), skipped in `cryptnwarpnext.ds1`,
which keeps an unused copy of the room. It is tracked: listed in the scanner and routed to, a route
ending beside the platform. Unverified live: that the platform is the vessel of the screenshot.

## The cistern is the basin; Tristram's fountain tracked (2026-10-22, second list)

**The cistern was on the wrong thing.** The tester's two layouts (`no blood bath_*`, CryptCountess1,
and `blood_bath_off_*`, CryptCountess2) and screenshots: the route to "Gilded Cistern of Virgins" led
to the Countess's quest chest, and near the basin nothing was said. `ritual_platform01` is the dais of
her chest: each layout's DS1 places `PlaceUniqueChest` (Act I preset object 38) beside it, at 26,70 and
65,20. The basin is not a layout model at all but a game object: Act I preset object 18,
`BloodFountain` (`objpreset.txt`; objects.txt ID 31, "Fountain", 3x3, collision), at 68,71 and 71,63,
the HD layout's candles round it (HD 135-139,140-145 in the first). `gen_places.py` now reads a
DS1's own object list (source `object`, `objpreset.txt` for the class, `objects.txt` SizeX/SizeY for
its footprint), and `BloodCistern` is that object, unpadded: the walk-in announcer's own reach
covers it. The tester's note (TombPool, ID 118) does not match the tables: objects.txt 118 is
`Fountain4`, a well. Every other seed is unchanged (two lines of `place_data.h`).

**Tristram's fountain** ("fountain overflowing with blood", `fountain_gore01`) is tracked too: listed
and routed to, as a way out of the burning houses, whose doorways do not creak (below). Its layout
entity carries a physics box about 4 subtiles across (`fountain01`, extents 3.77 HD scaled 1.1), so
the beacon's arrival distance of 3 from its centre is clear of it. Unverified live.

**Tristram's houses** still do not creak (the tester, build test-2026-10-22-2; the cottages and the
Tower's ruin do). Tristram's DS1 has no door objects, and its ruined walls are unlikely to be the
gap `Rooms::Doorways` looks for (a run of 3 cells to two tiles with wall a tile past each end).
Unchecked: it needs the collision round one house. The tester called it not essential.

## Jungle log bridges, the Moldy Tome's piece, the palace entrance (2026-10-23)

**Log bridges.** The tester (build test-2026-10-23, screenshot in the Spider Forest): the jungle's
stone bridges said "bridge", but a fallen tree over a river, which works the same, said nothing. The
Act III jungle keeps its bridges in `act3_jungle_bridges/`: `bridge_stone01` (inside the prefab
`pf_bridge_stone01_01`, already a bridge), `bridge_tree01` (the log, 48 pieces), `bridge_wood01`
and `bridge_wood_end01` (plank bridges, mostly in the Great Marsh's `bog*` pieces) and
`bridge_wood02`. The trees and planks are now "bridge" too, one word as the tester asked. Their model
is anchored at one end (the log's mesh runs about 39 HD units one way and 29 the other from it), so
their footprint is the collision mesh's whole extent (`MESH_FOOTPRINTS`, `collision_footprint`,
sharing the cliff stairs' mesh reader): about 20x15 subtiles for a log. The `bridge_log0N` models
under `act3_jungle_debris/` have no collision and are left out. 224 seeds; extract
`act3_jungle_bridges\` with the presets.

**The Moldy Tome's piece.** "Act 1 - Tower Tome" (Def 162, `tome.ds1`, the Stony Field) was named
"tower ruin" by the LvlPrest name rule, padded six tiles like the tower; the tester heard it there
and asked for it gone, keeping the Black Marsh's (Def 163). The rule now skips it; the tome itself
is a quest object in the scanner.

**Lut Gholein's palace entrance** is a scanner entry: the tester asked for the palace square or
gate as a place to route to. Its hall is the town's `Palace` interior (`TOWN_2_PALACE`, 23-24 by 25
subtiles in both layouts, `lutw.ds1` and `lutn.ds1`); `Places::Tracked(PlaceInterior)` lists it whole
(`Listing::interior`, named by the interior word, "palace entrance"), as wide as the hall, so the
beacon's arrival distance (3 plus half the footprint) ends a route at its door. Unverified live:
where on the hall's edge the route ends while the guards still bar the way.

## Town landmarks from anywhere; dead ends that go on (2026-10-25)

**The palace entrance** was listed only once its hall had been charted, and the hall behind the
guards never is from outside, so the tester never found it in the scanner. In a town the map's
landmarks are now listed from anywhere, as its exits and NPCs are (`Discovered` in
`map_survey_service.cpp`; doorways excepted). Lut Gholein's palace hall is the only one so far.

**Dead ends.** The tester heard "dead end" in straight corridors that go on. `Passages::Follow` walks
the corridor along the player's own line and called any wall cell met there the end. In six logged
samples it was the jamb of an arch off the line (four), a 5x3 object in the corridor and a slanting
wall; none was a real dead end. A wall met ahead now closes the corridor only when the cross-section
there, between the corridor's side walls, has no opening as wide as the body with floor one step
further (`GoesOn`, `Settings::passable` from `Rooms::PassableWidth`). A corridor walled across, or
plugged by stairs, is still a dead end. Tests: `passages_tests.cpp`.

## The Fortress's hall, the Hellforge, the pentagram (2026-10-27)

**The Pandemonium Fortress said "outside" on the way to the stash** (a tester; the log's `[places]`
lines: local 48,43 by the waypoint `interior=0`, local 26,41 `interior=-1`). Its HD layout is drawn
22 subtiles right of and below its DS1: the hall's walls (`int_wall*` at HD x 36.9, y 36.4–91.6;
`ext_wall*` at 57.9) are the DS1's west wall at x 15 and colonnade at x 35, y 14–70 (the log's
`[radar-collision]` dumps stitched), its fireplace lands on the DS1's two `Hellfire1` objects at
23,7, and its brazier mounts on the `PandemoniumBrazier2` objects at x 17 and 37. The generator now
takes `HD_LAYOUT_OFFSETS` off every entity of that file before reading it, so the fortress is the
hall west of the colonnade (volume 6,0–35,115): the stash and Tyrael are inside, the waypoint's
courtyard outside. Walking into the hall says "fortress", out of it "outside", as every interior.
Other layouts were not measured this way; no other report places an interior wrong.

**The Hellforge** (a tester): feature `Hellforge`, the River of Flame's `SoulStoneForge` object in
`act4/lava/forgee.ds1` and `forgew.ds1`, named with the game's own word (objects.json "Hellforge").
The first time it comes into reach its words are said instead, as the Hangman's Tree's
(since 2026-11-01 in `data/descriptions/<locale>.txt` only, [descriptions](descriptions.md)). Not a tracker entry: the
object itself is already listed.

**The pentagram** (a tester): feature `Pentagram`, a tracker entry like the cliff stairs, at the
Chaos Sanctuary heart's `DiabloStart` object (53,53 of `act4/diab/heart.ds1`), where Diablo
appears, padded 10 subtiles: it is said within the star's inner pentagon and a route to it ends
there. The HD star model (`act4_diab_cathedral/pentagram01`) sits 7 subtiles off that object, so the
object is the anchor. Tests: `places_tests.cpp` (`FortressHall`, the heart and both forges).
