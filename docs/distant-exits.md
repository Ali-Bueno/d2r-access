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
| `0x326480`, 254 bytes; `0x334ED0`, 17 bytes | Converts RoomEx tile coordinates/dimensions to subtiles by multiplying by five |
| `0x361750`, 473 bytes; existing `0x3DA9A0` / `0x3DA910` guards | Warp node: destination RoomEx `+0`, next `+8`, enabled `+0x10`, LvlWarp row `+0x20`, row Id `+0x2C` |

Existing destination-Level and Levels-table guards supply localized captions.
Walks have shared budgets, cycle detection, owner checks and coordinate bounds.
Malformed traversal discards distant results while nearby targets remain usable.
Permission caching is scoped to one synchronous read. No pointers survive a scan.
Only the directional scan and a fixed-exit beacon request this broader snapshot;
NPC proximity and ambient exit loops continue reading nearby native units.

## Coverage and validation

A never-generated room may have no preset data and remains unknown. This is not
full-map generation or full-map pathfinding. Unloaded OpenWarp objects without
a verified preset destination and open overland borders have no inferred location.

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
