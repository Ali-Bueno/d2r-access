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
stool at the counter) is reached from any cell around it. Only props in the player's interior count;
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
