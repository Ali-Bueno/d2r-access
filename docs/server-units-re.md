# Server-side monsters in single player — RE notes (D2R 3.3.93787 live image)

Source: Ghidra project `re/ghidra/D2R` (program `D2R_live.exe`), decompiled headless with
`D2RDecompX.java` (copy of `re/scripts/D2RDecomp.java` that also creates missing functions),
driver `dec.sh`, run read-only. Byte/capstone scans: `scan.py`, `and7f*.py`, `n158.py`.
All addresses are live RVAs (Ghidra = RVA + 0x140000000). Not yet checked on 3.4.95041:
fingerprint each function below before trusting its offsets.

## 1. Game table (the server's pGame list)

`0x4049D0` creates a game (calls quest init `0x513EE0(game)`, which stores `game+0x21D8`).

| Global | Meaning | Evidence |
|---|---|---|
| `0x2A9C010` | `ptr` to a hash map id->node: `[0]` bucket count (power of 2), `[1]` bucket array, `[2]` element count, `[3]` float max load | `0x4049D0`, `0x4062F0`; setter `0x407270` |
| `0x2A9C018 + id*8`, id 1..1024 (`0x2A9C020..0x2A9E020`) | slot state: `id` when live, `-1` while being created, `0` free | `0x4049D0` writes `-1` then `id`; `0x4061E0` |
| `0x238AB7C` (u16) | round-robin cursor of the last id handed out | `0x4049D0` |

Node: `+0 next`, `+8 key (game id, u64)`, `+0x10` **the Game struct** (node alloc 0x1AAF8, game 0x1AAE8).
Hash: murmur3 fmix64 (`0xff51afd7ed558ccd`, `0xc4ceb9fe1a85ec53`), bucket = `hash & (count-1)`.

- `0x4061E0(u16 id)`: returns slot value if `1<=id<=1024` and value not 0/-1, else 0. Pure.
- `0x4062F0(id)`: map lookup, returns `node+0x10`. Pure (no lock, no allocation).
- **The client itself does `game = 0x4062F0(0x4061E0(1))`** at `0x8B8E9..0x8B8FB` (fn `0x8B3C0`,
  next to `LocalIndex 0x8B2D0`): single player's local game is id 1. Fallback: scan the 1024 slots
  and require exactly one live game.

Game fields seen: `+0x104` difficulty (byte), `+0x106` table bank (byte, `0x300A90`), `+0x170` frame,
`+0x188` ?, `+0x190/+0x198` acts vector (data, size; one `Act*` per act, null if not built),
`+0x1B0/+0x1B8` per-act saved-unit lists (vector), `+0x1D0 + level*8` monster regions,
`+0x21D8` quest control (`[0]` = last quest object; quests linked via `+0x310`).
Quest object (0x318 B): `+0` quest id, `+8` **game** (back-pointer), `+0x10` byte, callbacks at `+0x270..`.

## 2. Where server monsters are

Act (`*(game+0x190)[act]`): `+0x18` first Room1 of the act (chain `Room1+0xB0`), `+0x70` DRLG
(`0x192B20`, `0x2EFC50`, `0x2EFA20`). DRLG: `+0x868` first level, `level+0x1B8` next,
`level+0x1F8` id, `level+0x1C8` back to DRLG (`0x3267C0`). **Never call `0x3267C0`: it creates the
level when missing.** Level -> RoomEx -> Room1 is the mod's own `LevelExitLayout` (`+0x10`, `+0x48`,
`+0x58`). Room1 units: `+0xA8` first, unit `+0x160` next (`0x2EFD90`, `0x34B4A0`) — same as client.

Room lifetime (`0x4088C0`, called from the game update `0x406550`): per act, per Room1, a counter
`Room1+0x10` grows while `Room1+0x70` (players in room) is 0; past 10 and when `0x2F0630` allows,
every unit goes through `0x504260(game, unit)` and the room is freed (`0x2F0110`, which clears
`RoomEx+0x58` via `0x361260` and destroys remaining units).

Monsters in a freed room (`0x504260` -> `0x504B70` -> `0x5045D0` -> `0x505090`): alive ones are
**saved** as 0xD8-byte records and the unit is freed. Record: `+0` x, `+4` y (subtiles, from the
path getters `0x341A20/30`), `+8` class id, `+0xC` unit id, `+0x10/+0x14` copies of unit
`+0x124/+0x128`, `+0x18` flags (bit 0x800 superunique, id at `+0x44`), `+0x34` level id
(`0x543810`), `+0x48` life stat 0xD, `+0x5C` game frame, `+0xD0` next.
Linked at `node+0x10` of the per-room node returned by `0x505000(game, room, create)`; nodes
(0x28 B: `+0` room x, `+4` room y, `+8` saved item blobs, `+0x10` saved monsters, `+0x20` next,
sorted) hang from `*(game+0x1B0)[act]`. On re-population `0x505D70` walks the same `+0x10` list
(`+0xD0` next) and respawns them. So: live monsters = units in the level's live Room1s + saved
records of that level.

## 3. Den of Evil counter

Monster regions: `0x508750` allocates one 0x9F0-byte region per level into `game+0x1D0+level*8`.
`+0x2C0` level id, `+0x2C8` unique count, **`+0x2CC` spawn count** (`0x509520` ++ on spawn,
`0x509630` -- when a spawn is undone), **`+0x2D0` kill count** (`0x509590` ++ on death from
`0x520CF0/0x57C590/0x57CDF0`, `0x5095E0` -- on resurrect), `+0x2FC/+0x300/+0x304/+0x308` other
getters. Den callback `0x5DDD00`: `quest+0xA8 = region(8)+0x2CC - region(8)+0x2D0`
(`0x5DDE87`), complete when populated rooms (`0x2EFA20`) <= `region+4` and kill == spawn
(`0x5DDF55`), sets `quest+0xA4 = 1`. Matches D2MOO `ACT1Q1_Callback08_MonsterKilled`.
So remaining = `*(int*)(R+0x2CC) - *(int*)(R+0x2D0)`, `R = *(game+0x210)`.

## 4. Unit fields

Same struct and getters as the client (`0x34B9D0`, `0x349860`, `0x34A330`, `0x34AB60`, `0x34C2C0`,
`0x34B440` room, `0x34B4A0` next) — the server code (`0x4088C0`, `0x504B70`, `0x505090`) calls these
very functions. Type +0, class +4, id +8, mode +0xC, data +0x10, path +0x38, flags +0x124/+0x128,
bank +0x1BD. Alive: mode not 0/12 and not `+0x124 & 0x10000`. Level: unit room -> `+0x18` RoomEx ->
`+0x90` level -> `+0x1F8`. Do not call client-only name builders (`0x985A0` frees/caches) on server
units; resolve names from MonStats by class id.

## Not done

- Not fingerprinted on 3.4.95041.
- `0x2F0630`/`0x328880` (when a room may be freed) not read; irrelevant if both sources are read.
- Thread of the game update vs the UI thread not proven; use `ThreadService::runOnGameThread`.
