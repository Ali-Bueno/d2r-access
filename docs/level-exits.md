# Level exit loops (portal sound)

Tal Rasha tomb entrance names now include the fixed symbol on each entrance
(2026-10-03); loaded warp units, existing distant presets and restored places
share the label. The quest log names its visible matching graphic. No loop or
interaction behavior changes. See [symbol evidence](tal-rasha-symbols.md).

Implemented 2026-09-16 for the installed D2R 3.3.93787 executable. Every unit that
takes the player to another area plays the user's `exploration/portal.ogg` loop at its world
position, through the same spatial mixer as the ground item loops: fixed screen
axes, distance attenuation to silence at 30 subtiles, rear pitch 0.94.

Restored on 2026-09-23: all entrances requiring interaction, including dungeon
stairs and cave entrances, use `portal`. Only actual crossings between loaded
areas that the player walks across use `zone transition`; see
[exploration sounds](exploration.md). The generic `Exit` category does not imply
that a transition is walkable.

## What counts as an exit

| Unit | Game rule used | Spoken name in the entity scan |
|---|---|---|
| Level exits, cave / dungeon entrances and exits, stairs (unit type 5, "warp") | The warp's node in its room, found by LvlWarp `Id`, must be one the game would label (`node+0x10`); walk-through warps marked `NoInteract` are skipped because their click proxy is an object | Levels `LevelWarp` caption of the destination ("A las Llanuras Frías"), like the game's hover label; `Salida` when unavailable |
| Town portals, permanent red portals | objects.txt `SubClass` bit 2, only while `Selectable[mode]` (mode 2, open) | Destination level name from `ObjectData+8` |
| Arcane Sanctuary portal, hell gate | `SubClass` bit 4 | objects.txt key |
| Waypoints | `SubClass` bit 6 (16 rows, including the sewer / temple / Travincal variants) | objects.txt key |
| Trapdoors, unfolding stairs, Mephisto's door | objects.txt `OpenWarp` | objects.txt key |
| Duriel's portal, the Ancients' gateway, summit door, final portals | The object's name key equals a Levels `LevelName` / `LevelWarp` key | objects.txt key |

Ordinary objects (chests, shrines, doors) keep `Kind::Object`; exits are
`EntityRadar::Kind::Exit`, so the right-stick scan names them with distance and
the NPC announcer ignores them.

## Behavior and controls

Exit loops start enabled. **Ctrl+P** toggles them; console
`access-items exits-on|exits-off|status`. They are independent of the item loops
(**Ctrl+L**) and item speech (**Ctrl+I**); with item loops off the exits still
sound. Exits are never spoken by the proximity reader.

The exit list is refreshed every 500 ms (`GroundItems::ExitRefreshMs`) from the
verified entity reader over the player's room and its neighbours; the listener
position is polled every 50 ms with the items, so the loop pans continuously.
Exits always get a voice before items, so a pile of loot never silences the way
out. The same pause / dialogue / focus rules as the item loops apply. The log
prints `[items] exits nearby: N` only when the count changes.

## Native evidence

Recovered offline from the analyzed `re/ghidra` program `D2R_live.exe` and the
extracted tables (`objects.bin`, `Levels.bin`, `LvlWarp.bin` correlated column by
column against the `.txt` files). Fingerprints in `src/entity_layout.h`.

| Fact | Evidence |
|---|---|
| Type-5 units sit in the room unit list and use the static path (room at +0, X/Y at +0x10/+0x14) | `0x2EFEE0` walks `Room+0xA8` filtering type 5; `0x34B440` and `0x324FC0` treat types 2, 4 and 5 alike |
| `Unit+4` of a warp is the LvlWarp `Id`, compared as a byte | `0x2EFEE0`, `0x3DA910` (`& 0xFF`) |
| `Room+0x18` → RoomEx; `RoomEx+0x78` warp nodes: destination RoomEx +0, next +8, gate +0x10, LvlWarp row +0x20 | `0x192B20`, `0x3DA9A0`, `0x3DA910` |
| Destination level id = `RoomEx+0x90` → `Level+0x1F8` | `0x361070`, `0x360FC0` |
| Levels rows at `bank+0x1460` (count `+0x1468`), stride 0x18C; `LevelName` key at +0xFD, `LevelWarp` key at +0x125 (bin offsets 0x105 / 0x12D minus 8) | `0x32C4A0`, `0xC17E0`, `0x9F2A0` |
| objects.txt `SubClass` is the whole byte at `+0x127`: bit0 shrine, bit2 portal, bit4 gate, bit6 waypoint, bit7 no label | byte-exact over 587 rows; `0xC17E0` branches on bits 2, 6 and 7 |
| `OpenWarp` at `+0x163` is exactly Trapdoor, UnfoldingStairLeft/Right, MephistoDoor | byte-exact over 587 rows |
| `ObjectData+8` is a portal's destination level id | `0x34AD40`, `0xC17E0` portal branch |
| LvlWarp rows are the `LvlWarp.bin` layout (stride 0x60): `Id` +0x2C, `NoInteract` +0x58 | byte-exact over 88 rows |

The game's own warp-name path (`0x9F2A0` → `0x3DA9A0`) may generate the
destination room, so the plugin never calls it; every read above is a guarded
`Peek`.

## Foreign-level leakage (fixed 2026-09-16)

Live-session evidence: inside the Den of Evil the scan offered "To The Den of Evil" at two points
thousands of subtiles from the player, and the beacon locked one, clamped the unreachable goal to the
nearest reachable cell in its search box, and walked the player into a wall, pinging forever. The
offending unit ids changed between visits (4, 11, 2) — only a live `ReadWarp` unit does that, a
preset-derived exit keeps a stable id — which places the bug in the ordinary native walk below, not the
distant/preset feature (that walk is already level-safe; see `docs/distant-exits.md`).

Cause: a room's neighbour list can include the far side of a level passage, which belongs to a
*different* level. That neighbour room's units — including that other level's own warp units — were
read with no level check, and exits are exempt from both the range gate and the line-of-sight check, so
distance never filtered them either. Fix: `src/native_entities.cpp` now resolves each room's owning
`Level` (`Room1 -> RoomEx +0x18 -> Level +0x90`) and keeps a neighbour room only when it belongs to the
player's level, leaving the walk unchanged when the level can't be read. This also stops foreign
monsters/objects leaking into the proximity reader and the exit loop above. No store needed clearing:
the entity list is rebuilt per read and the beacon already releases its lock on a level change. Covered
by two new `tests/native_entities_tests.cpp` checks (a neighbour room in another level contributes no
entity; one in the same level is still walked).

## Pending live checks

- `WarpNode+0x10` as the "labelled" gate is inferred from `0x3DA910`; if no exit
  ever sounds, log it for a normal exit and compare.
- `Unit+0x10` and `Unit+0xC` of a type-5 unit were never read by decompiled code;
  the plugin does not use them.
- Confirm the `+0x125` string is the "To The ..." caption on a real exit.

## Objects named after a level are exits; the stairs' rows logged (2026-10-25)

**Tal Rasha's Chamber.** After the Horadric Staff goes in the orifice, the way to Duriel's Lair is
an object, `DurielPortal` (objects.txt 100, key "Duriel's Lair", SubClass 0, OperateFn 43). It was
already an exit by its key but filed as "object", so the scanner listed it only in sight and never
said "new exit". An object whose key is a Levels key is now `Category::Exit` too, with the level
whose LevelName (else LevelWarp) key it is as its destination, and a place id like the stairs'. The
same rule covers the Ancients' gateway, the Worldstone Chamber portal, Tyrael's portal to
Harrogath, the summit door and the Throne's portal (objects 547, 563, 564, 566, 569).

**Where a stair is taken from.** A tester: routes end beside a stair but not where A takes it
(Flayer Dungeon, the Harem and Palace Cellar, Act I's Jail and Catacombs). In the four logs every
stair taken was used with the game's interact command (`[actions] game command 0x40 a=5 b=<unit>`)
from within about 3 subtiles of the unit, from several sides; no fixed offset fits (unit + Offset,
unit ± ExitWalk, the select box), and the failed tries at the Flayer stair were as close. ExitWalk
is the walk the game makes after arriving by a stair. The LvlWarp row's columns are now proven over
all 88 rows of `LvlWarp.bin` (`game-data/global/excel/`): Name +0x00 (char[32]), UniqueId +0x28,
Id +0x2C, SelectX/Y/DX/DY +0x30..+0x3C (pixels), ExitWalkX/Y +0x40/+0x44, OffsetX/Y +0x48/+0x4C,
LitVersion +0x50, Tiles +0x54, NoInteract +0x58, Direction +0x5C. Each stair now logs its row once,
`[exit] unit=U warp=W row=<name> to=L at=x,y walk=… offset=… select=… dir=…`, to be joined with the
`0x40` line of the stair taken. No routing change until a log shows the rule.

## A stair is taken within 4 subtiles of it; routes end there (2026-10-25b)

**The rule (data + log).** The pad's auto-target files a warp tile under the room-tile category,
`controllersettings.json` `primaryTargetingData.RoomTile`: 360°, distance 4.0, distance only
([controller targeting](controller-targeting.md)). A stair becomes the game's target, and A takes it,
whenever the player stands within 4 subtiles of the warp unit's position, from any side. The tester's
`flay_me_2` log, Flayer Dungeon stair `warp=54` at 5376,3169 (`offset=-4,-6 select=-130,-10,170,100`):
targeted at 5379,3167 (3.61), 5378,3167 (2.83), 5374,3167 (2.83), 5372.5,3169.5 (3.54); not at
5380,3169 (4.0), 5380,3168 (4.12), 5382,3168 (6.08). In the `flay me_` log the player never came nearer
than 5.1 and was never offered it. The walls on three sides matter only because they keep the body
outside the 4 units there; the unit stands near the stairs' open top.

**What was wrong.** A route ended in a square of ±3 round the stair (`ArrivalMargin`), whose corners
are 4.24 away, and "Arrived" came within 3 units of the way's last cell (`RouteFollow::EndUnits`),
so up to about 6 from the stair: every failed try in the log heard "Arrived" at 4 or more.

**Now.** For an exit taken with the game's own command (`TakenWithinReach` in `beacon_service.cpp`:
Kind::Exit, no area border, no object footprint, not a pad or doorway), the way ends on cells whose
centre is within 4 − √2/2 of the stair (`LevelRoute::Area::reach`), so any point of the cell is within
reach, and the follower says "Arrived" only within 3.5 of the stair (`RouteFollow` goal). With no
known floor within reach the way falls back to the square. The beacon's own arrival (3 units) was
already inside the reach.

**The name that was not said.** At 5372,3170 the game targeted the stair and the mod logged
`behind a wall, not spoken -> "To The Flayer Dungeon Level 1"`: a Flayer behind a wall bore the
word "Flayer", and exits are skipped by the sight gate, so the monster stood for the label.
`TargetSight::Find` now gives a label carrying an exit's whole name to the exit
([target sight](target-sight.md)).

## The Kurast sewer stairs say "new exit" when the lever opens them (2026-10-25b)

`BeneathTheCityStairs` (objects.txt 366, OperateFn 44) is listed only once open (Selectable in mode
2) and has no place id on purpose: a new game shuts it until the lever is pulled again, so it is not
stored for later games. It never reached the "new exit" line, which only stored places get. Now an
object exit with no place id is told once a game when it first appears
(`[terrain] exit revealed level=L name=N (this game only)`, `TerrainService::RememberPlaces`).
