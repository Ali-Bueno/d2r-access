# Act level list, `Level` rect and Levels tables (RE, 2026-09-21)

D2R 3.3.93787. Evidence: Ghidra `re/ghidra` (`D2R_live`, `D2RDecomp.java`) + capstone over
`re/D2R.rebuilt.exe`. RVAs below; VA = RVA + 0x140000000. Confidence high unless marked.

## `Level` struct

| Offset | Field | Evidence |
|---|---|---|
| `+0x00` | DrlgType (1 maze / 2 preset / 3 outdoor) | `0x3267C0` dispatches on it |
| `+0x0C` | activation refcount | `0x328010` |
| `+0x10` | first `RoomEx` (next at `RoomEx+0x48`) | `0x3275B0`, `0x326580` |
| `+0x24/+0x28/+0x2C/+0x30` | posX, posY, sizeX, sizeY (tiles) | writers `0x327650`, `0x3EC820`, `0x3F1F45` |
| `+0x1B8` | next `Level` in the act list | `0x3267C0` walk + push front |
| `+0x1C8` | `pDrlg` back pointer | `0x3267C0`, `0x360880`, `0x3DAAD0` |
| `+0x1E0` | LevelType | `0x3267C0` |
| `+0x1F8` | level id | `0x360FC0` |

The rect is the union of the level's `RoomEx` tile rects (`0x326580` unions `RoomEx+0x60..0x6C`,
`0x3EC820` writes the result into `+0x24..0x30`). `DRLG_GetLevel` = `0x3267C0(bank, pDrlg, id)`.

## `pDrlg` (act DRLG)

`+0x868` first `Level`; `+0x830` difficulty; `+0x860` act seed; `+0x870` act number;
`+0x118` per-act level-def override list (node: `+0` id, `+4` Vis[8], `+0x24` Warp[8], `+0x48` next).
Route from the player: `Room1+0x18 → RoomEx +0x90 → Level +0x1C8 → pDrlg +0x868`.

## Levels tables

The DRLG reads a second table, `LEVELS_GetLevelDef` = `0x32C200(bank, id)` → `bank+0x14A0 + id*0x9C`
(count `bank+0x14A8`). Row: `+0x08` Act, `+0x0C` SizeX[3], `+0x18` SizeY[3], `+0x24/+0x28` OffsetX/Y,
`+0x2C` Depend, `+0x30` DrlgType, `+0x34` LevelType, `+0x48` Vis[8], `+0x68` Warp[8]. Accessors:
Vis `0x360800`, Warp `0x3DAA50` / `0x3DAAD0` (honour the override list). The 0x18C row at
`bank+0x1460` is the string row (`LevelName +0xFD`, `LevelWarp +0x125`); no code reads Vis/Warp from it.

**Vis/Warp are paired warp slots (dungeon links), not outdoor neighbours.** Blood Moor (2):
`Vis = 0,0,0,8,8,8,8,0`, `Warp = -1,-1,-1,0,1,2,3,-1`. Town ↔ Blood Moor ↔ Cold Plains adjacency comes
from the act placement table + rect adjacency.

## When levels exist

All levels in the act's static placement table are allocated and positioned at act creation:
`0x326BA0` (act creation) → `0x3F0130` (per-act driver) → `0x3F1C60` (loops the id table, `GetLevel`
+ 16-byte rect). Dungeons reached later are created on demand by `DRLG_GetLevel`. Rooms (`RoomEx`) of a
level are generated only when the game initialises it, near the player.

Design note: the mod does **not** announce levels the player has not reached (see
`docs/terrain-overview.md`); this table is documentation only.

Fingerprint candidates (RVA/size): `0x3267C0`/488, `0x327650`/160, `0x326580`/117, `0x3275B0`/160,
`0x360800`/122, `0x3DAA50`/122, `0x3DAAD0`/132, `0x32C200`/246, `0x3F1C60`/1186, `0x3F0130`/644,
`0x326BA0`/1264.
