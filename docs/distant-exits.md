# Distant dungeon transitions

The directional scan includes fixed stairs and level passages regardless of its
ordinary 15-subtile range or intervening walls. Enemies, NPCs, loot and temporary
portals retain their existing local scan rules. Push the right stick toward an
exit, repeat to cycle candidates, then click the stick / Ctrl+B to follow it.
Ctrl+E and `access-scan on|off` still control the scan.

The adapter walks the current level's existing logical rooms, including inactive
rooms. Type-5 presets are matched to room warp nodes for their actual destination
and position. No room generation, automap reveal, memory write or new dependency
is involved. Same-level rooms are outside the requested scope. Noninteractive
proxy tiles are skipped; loaded OpenWarp stairs/trapdoor objects remain targets.

Coordinate identity survives unloading and recreation of a native warp unit.
Loaded and preset representations merge into one target. Beacon refresh uses
this identity so another entity with the same numeric unit ID cannot take over
the lock. Guidance still routes through available local collision; a distant
destination does not provide a complete route through unexplored collision.

Outdoor and town levels are also left by walking across a shared border with no
warp on it. There (`Levels.txt DrlgType` other than 1) each logical room's near
rooms contribute one exit per bordering level, named with that level's own name
at the subtile midpoint of the shared tile edge, where the automap draws it.

## Evidence for D2R 3.3.93787

Recovered from local `re/ghidra/D2R` / `D2R_live.exe` using `D2RDecomp.java`.
Full-function fingerprints are in `entity_layout.h`; no historical third-party
offset is used as a current-build authority.

| Native function | Fact |
|---|---|
| `0x3614C0`, 653 bytes | RoomEx `+0x90` -> Level; Level `+0x10` -> first RoomEx; next `+0x48`; tile rectangle `+0x60/+0x64/+0x68/+0x6C` |
| `0x361040`, 33 bytes; wrapper `0x2EFD20` | RoomEx `+0x98` -> presets. The getter changes flags; our reader copies the pointer without calling it. |
| `0x99650`, 486 bytes; `0x48F190`, 837 bytes | Preset next `+0x10`, class `+4`, X `+8`, type `+0x20`, Y `+0x24` |
| `0x4926B0`, 211 bytes | Spawns preset type/class at active-room subtile origin plus preset X/Y |
| `0x3614C0` (writer); readers `0x360610`, 287 bytes / `0x360770`, 45 bytes | RoomEx near rooms: array `+0x10`, count `+0x18`, a `size_t` the getter narrows to 32 bits |
| `0x326480`, 254 bytes; `0x334ED0`, 17 bytes | Converts RoomEx tile coordinates/dimensions to subtiles by multiplying by five |
| `0x361750`, 473 bytes; existing `0x3DA9A0` / `0x3DA910` guards | Warp node: destination RoomEx `+0`, next `+8`, enabled `+0x10`, LvlWarp row `+0x20`, row Id `+0x2C` |

Existing destination-Level and Levels-table guards supply localized captions.
Walks have shared budgets, cycle detection, owner checks and coordinate bounds.
Malformed traversal discards distant results while nearby targets remain usable.
Permission caching is scoped to one synchronous read. No pointers survive a scan.
Only the directional scan and a fixed-exit beacon request this broader snapshot;
NPC proximity and ambient exit loops continue reading nearby native units.

A 2026-09-16 session logged the scan offering a Den of Evil exit thousands of subtiles from the
player; traced to the ordinary native walk reading a neighbour room owned by a different level, not to
this preset walk (already level-safe by construction, per the guards above) — fixed in
`docs/level-exits.md`.

## Coverage and validation

A never-generated room may have no preset data and remains unknown. This is not
full-map generation or full-map pathfinding. Unloaded OpenWarp objects without
a verified preset destination have no inferred location; whether a border is
walkable is unknown, so the reach verdict answers around / no route / nothing.

`native-level-exits-tests` covers inactive distant rooms, destination matching,
coordinates, missing data, foreign levels and corrupt/cyclic lists. Radar tests
cover distant/walled stairs, unchanged ordinary range, direction filtering and
stable selection after unit recreation. User-led live check: scan toward known
stairs from more than 15 subtiles, lock them and confirm name/heading while
moving away. Repeat in a dungeon with multiple floors. Native guard success,
speech and route accuracy need separate live confirmation.

Build/deploy and 38 CTest suites / 9353 checks pass. The installed game startup
at 2026-09-16 22:08:39 logs `[scan] Verified native monster/object/exit reader`,
confirming all six new full-function guards match the live image. Actual distant
stair selection and guidance in a dungeon still require the controller/NVDA pass.

## Areas that are not joined by a warp at all

Evidence: the game's own `global/excel/Levels.txt`, extracted from the installed
CASC storage with `tools/casc-extract` (D2R 3.3), read alongside the catalogue's
own `[survey]` lines from 2026-09-20.

The exits category was empty in the Rogue Encampment. The reason is in the table,
not in the reader:

| level | `Vis0..7` | `Warp0..7` |
|---|---|---|
| 1 Rogue Encampment | all 0 | all -1 |
| 2 Blood Moor | 8, 8, 8, 8 (Den of Evil) | 0, 1, 2, 3 |
| 8 Den of Evil | 2 (Blood Moor) | 4 |

**The encampment has no warp of any kind**, and the Blood Moor's four warps all
lead to the Den of Evil — never back to town. Kurast Docktown, the Pandemonium
Fortress and Harrogath list none either; Lut Gholein is the only town that does
(to the sewers). Twenty-four named levels in the game have no warp link at all.

So a warp in D2 is the way into or out of an **enclosed** level: a cave, a crypt,
a tower, a sewer. Two outdoor areas that run into each other — a town and the
wilderness beside it, or one wilderness and the next — are joined by nothing but
the map: they sit next to each other on the act's grid and the player walks
across the boundary. There is no record for `native_level_exits.cpp` to find, and
no amount of relaxing its gates will produce one.

`native_level_areas.cpp` answers that half instead. `DrlgGetOrAddLevel`
(RVA `0x3267C0`, already fingerprinted in `drlg_layout.h`) looks a level up by
walking `drlg+0x868` and then `level+0x1B8`, so those two are the act's list of
the levels it has built; each level's rooms carry tile rectangles in the same
world grid as the player's own, so a level's footprint is the union of them.
`LevelAreas::Neighbours` keeps the areas within the game's own six-tile "near"
gap and reports where two of them meet.

**That crossing is a bearing, not a doorway.** The shared edge of two areas is
mostly solid — a town's palisade, a cliff — and the gap through it is somewhere
along that edge rather than at its middle. It says which way the Blood Moor lies
and roughly how far; the collision grid still does the real routing once the way
through is inside its window.

### The town's rooms are map squares, not buildings

`Levels.txt` gives the Rogue Encampment as 56 x 40 tiles, and the catalogue
counted 35 rooms there on 2026-09-20. 56/8 x 40/8 = 7 x 5 = 35: the town is one
preset (`Act1/Town/TownN1.ds1`) divided into a grid of 8x8-tile rooms. A room is
a ~40-yard square of ground, not a tent or a hut.
