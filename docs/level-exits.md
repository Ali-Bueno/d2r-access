# Level exit loops (portal sound)

Implemented 2026-09-16 for the installed D2R 3.3.93787 executable. Every unit that
takes the player to another area plays the user's `portal.ogg` loop at its world
position, through the same spatial mixer as the ground item loops: fixed screen
axes, distance attenuation to silence at 30 subtiles, rear pitch 0.94.

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

## Pending live checks

- `WarpNode+0x10` as the "labelled" gate is inferred from `0x3DA910`; if no exit
  ever sounds, log it for a normal exit and compare.
- `Unit+0x10` and `Unit+0xC` of a type-5 unit were never read by decompiled code;
  the plugin does not use them.
- Confirm the `+0x125` string is the "To The ..." caption on a real exit.
