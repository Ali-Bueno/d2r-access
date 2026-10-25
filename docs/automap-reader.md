# Native automap reader

Read-only access to the cells the stock D2R automap has drawn, for a later
"what a sighted player sees on the map" scanner. Code: `src/automap_layout.h`
(offsets and fingerprints), `src/automap_cells.{h,cpp}` (decode and guarded tree
walk, unit-tested), `src/native_automap.{h,cpp}` (live read) and
`src/automap_service.{h,cpp}` (the `access-automap` console command). Nothing
reads it for navigation yet. This supersedes the native-layout parts of
[the discovery research](automap-discovery-research.md).

Target: D2R 3.3.93787, image base `0x140000000`. The addresses are live RVAs.
They were read from the live `.text` in `re/D2R.rebuilt.exe` and cross-checked
byte-exact against a second live dump,
`<game>/d2rloader/plugins/d2r-access/D2R.text.bin`. The `.rdata` strings in the
rebuilt image are misplaced near the automap strings, so this work used code
shape, not string references. It started from the LevelDefs accessor `0x32C200`,
whose `row+0x08` result goes to `0xD5360`. Confidence: **verified** means read
from the code. **Live** means it still needs the session below.

## Layer model

| Value | Meaning | Evidence |
|---|---|---|
| bank `+0x14A0` / `+0x14A8`, stride `0x9C`, `+0x08` | LevelDefs rows, count and `Layer` | `0x32C200`; `0xD2240`, `0xD22F0` and `0xD4310` pass `row+0x08` to `0xD5360`. The stride matches classic LevelDefBin (156). Verified |
| `0x2A2CF60` | head of the layer list | `0xD5360` walks it and links new layers at the head. Verified |
| `0x2A2CF68` | **current layer** | Written only by the switch `0xD1710` at `0xD18B6` and by free-all at `0xD3408`; the renderer `0xD2600` draws only this layer. Verified |
| layer `+0x00` / `+0x04` / `+0xA8` | layer number / town-map sheet / next | `0xD5360` allocates `0xB0` bytes. `0xD22F0` sets the sheet to 1–3 in Lut Gholein, Pandemonium and Harrogath. Verified |
| layer `+0x08`, `+0x30`, `+0x58`, `+0x80` | trees: floors, walls, objects, extras | Insert sites `0xD5160` (floors/walls), `0xD52B0` (objects), `0xD22F0` (extras); the loader `0xD5710` groups them in the same order. Verified |
| 100 layers | one `.ma` slot per layer | `0xD5710` reads a 400-byte offset table. Verified |

**Layers are shared.** From `Levels.txt Layer`:
- Layer 0 covers the Rogue Encampment, the Act I outdoor levels, and the Monastery through Cathedral levels.
- Layer 27 covers Lut Gholein through Valley of Snakes.
- Layer 57 covers Kurast Docks through Travincal.

The reader therefore keeps a cell only if it lies inside one of the current
level's room rectangles (`NativeRooms::Read`).

**Only the current layer is resident.** `0xD1710` does four things in order:
1. It saves the session cells (`0xD6230`).
2. It clears all four trees (`0xD4D60`).
3. It points `0x2A2CF68` at the new layer.
4. It reloads that layer's chains from the `.ma` save (`0xD5710`).

The updater `0xD4310` (called from `0x9521B`) switches only when three things
hold: the player has moved at least 80 pixels since the last pass, and
`0xCC1E0()` is false. It then reveals the rooms near the player. Until then the
layer can still be the previous level's. The reader then reports
`not-switched`; it does not guess.

## Tree and node layout (verified)

- **Tree header** (`0xD5360` init, `0xD4D60` clear, `0xD1460` insert):
  - `+0x00` root, `+0x08` leftmost, `+0x10` rightmost, `+0x20` node count.
  - Empty tree: root is null, and leftmost and rightmost both point to the header.
- **Node** (0x30 bytes, a red-black tree built by `0xD1460`):
  - `+0x00` parent (the root's parent is the header), `+0x08` left, `+0x10` right, `+0x18` colour.
  - `+0x20` int16 origin, `+0x22` int16 cell number, `+0x24` int32 x, `+0x28` int32 y.
  - Missing children are null.
  - `0xD4B70` orders nodes by (y, x, `prio[cell]`), using the int32 table at `0x2A2AF60`.
- **Origin**:
  - `0` means revealed this session (`0xD5160`, `0xD52B0`, `0xD22F0`).
  - `1` means restored from the `.ma` save (`0xD5710`).
  - The saver `0xD7CE0` writes back only origin 0, as three int16 values (cell, x, y). These are the 6-byte records already seen in `.ma0`.

## What adds a cell

`RevealAutomapRoom 0xD6550(bank, room, all, layer)`:
- **Floors** (`Room+8` → `+0x20` pointer, `+0x28` count) and **walls** (`+0x00`, `+0x08`), tile stride `0x48`.
- A tile with flag `0x8` is never drawn.
- If `all` is 0 and the reveal byte is 0, a tile needs flag `0x20000`. That flag is set by the client tile pass (`0x21D366`, `0x21D7C4`, `0x21E2C0`, `0x21E9FE`): *drawn on screen*.
- `0xD5160` then sets `0x40000` and inserts the cell number that `0x32BDF0` finds in AutoMap.txt, keyed by `LevelDefs+0x34` and the tile's style and sequence.
- The updater always calls this function with `all = 0`.

Objects are added by `0xD6680` for the units in the room:
- A unit needs `Unit+0x124` bit 28 set (setter `0x34E020`, called from client draw code at `0x1CC50F`) and bit 29 clear. The function then sets bit 29.
- Type 2 objects use objects.txt `+0x164` AutoMap, with three exceptions:

  | Class | Row | Drawn only when |
  |---|---|---|
  | 267 | Bank | `0x326710(level)` returns 2 or 3 |
  | 366 | sewer stairs | the unit's mode is 2 |
  | 402 | a waypoint row | in level 74 |

- Type 1 monsters use monstats `+0x4C`, then monstats2 (stride `0x128`) `+0x118`. That field is probably AutomapCel; this is a **hypothesis** (no monstats2 extracted).

The byte **`0x2A2ADD4`** makes both passes add everything, bypassing discovery.
It is toggled at `0xD42E8`. D2RCore resolves it together with `0xD2240`,
`0xD6550`, `0xD6680` and `0xD78F0`, and the loader has a `revealmap` cheat. The
reader logs this byte; a non-zero value means the cells are not discovery.

**Unit markers are not cells.** Players, party, NPCs, mercenaries, corpses and
portals are drawn every frame by `0xD76E0` / `0xD78F0` from the units in near
rooms. Treat any level or exit text the same way (live, hypothesis): no tree
writer carries text.

## Pixel conversions

Cell coordinates are client pixels / 10.

**Floors and walls** (`0xD5160` → `0x334EF0`):
- Tile `(tx, ty)` = RoomEx `+0x60`/`+0x64` plus tile `+0x08`/`+0x0C`.
- Pixels: `x = 8(tx-ty)`, `y = 4(tx+ty)`; tile type ≥ 16 (lower wall) adds 24 to `y`.
- Exact inverse: `tx = (x+2y)/16`, `ty = (2y-x)/16`. Both must divide exactly (the /16 self-check).
- A lower wall decodes three tiles further on both axes and still passes the check, so the discovery mask should come from floors.
- Subtiles are `5t .. 5t+4` (`0x334ED0`).

**Objects** (`0xD52B0`):
- Pixels: `x = trunc(cx/10) + 1`, `y = trunc(cy/10) - 3`, where `cx`/`cy` are the unit's path `+0x08`/`+0x0C` (`0x34AF60`, `0x34AFB0`).
- A static path stores `cx = 16(x-y)`, `cy = 8(x+y)` from its subtile (`0x34D540` → `0x334E00`). A dynamic path computes the same from 32nds of a subtile (`0x3818E0` → `0x334DE0`).
- The reader takes the middle of each truncation interval, then applies the game's inverse `0x334E80`: `sx = (cx+2cy)/32`, `sy = (2cy-cx)/32`.
- Accuracy:
  - Static objects round back to their exact subtile.
  - A moving unit is within `(10 + 2·5.5)/32 ≈ 0.66` subtile away from the world origin. The tests measure 0.59.

**Extras** (town-map pieces) use the same inverse without the bias. They are approximate.

## Reader design

- **Verification.** `NativeAutomap::Initialize` verifies the checks listed below once. On failure the reader stays off, and the command reports `status=unavailable`; the load log names the changed RVAs.
- **Player roots.** The player and the table bank come from `NativeGroundItems::RuntimeRoots`, so the ground-item reader must also be verified.
- **Level check.** The reader looks up `LevelDefs[level].Layer` and reads `0x2A2CF68`. It requires `layer+0` to equal that layer; otherwise it reports `not-switched`.
- **Tree walk.** Each tree is walked in order, iteratively, with:
  - a cap of `2^19` nodes (checked against the header count before reading),
  - a depth cap of 64,
  - checks that every parent link matches, that the first and last nodes are the header's extremes, that the visit count equals the header count, and that the (y, x) order holds.

  A failure drops that tree only; the other trees still read.
- **Threading.** Everything runs on the UI tick, the thread the other native readers use. The game's updater and layer switch run on the client update, and a switch frees every node. The guarded reads (SEH) and the consistency checks are the backstop if that assumption is ever wrong.
- **Cost.** `status` reads only headers. `dump` walks every node: tens of thousands on a shared outdoor layer. It is a diagnostic, not a per-frame read.

Fingerprints (`AutomapLayout::Checks`, `LayoutCheck::Hash`):
- `0xD5360`, `0xD1710`, `0xD1460`, `0xD4B70`, `0xD4D60`
- `0xD5160`, `0xD52B0`, `0xD6550` (297 bytes), `0xD6680`
- `0x32C200`, `0x334EF0`, `0x334E80`, `0x34AF60`, `0x34AFB0`
- `0xD4310`, `0xD5710`
- `0x34D540`, `0x334E00`, `0x334DE0`: the client-pixel converters that the object inverse relies on.

The tile-flag getters are not included, because the reader does not read tile flags.

## Live validation

Run these in the D2RLoader console. Results go to `<game>/d2rloader/logs/d2r-access.log`
under `[automap]`, and speech says only that the log was written.

Without the console: **Ctrl+J** (or `access-automap watch`) toggles a watch that
dumps once, then every 500 ms reads the tree headers and logs only changes. A
level or layer switch logs a full dump, and new object/extra cells are logged as
`[automap] new object ...`. Floor growth is summarized at most every 5 s. The
whole route below can then be played with the watch on, locking a Cairn stone
with L3 so `[beacon] locked ... at=` gives its exact position.

| Step | Do | Expect |
|---|---|---|
| A | In the Rogue Encampment, `access-automap dump` | `status=ok level=1 expected_layer=0 layer=0 ... reveal_all=0`; `tree=floors status=ok ... off_lattice=0` with `in_level` > 0 |
| B | Stony Field, far from the Cairn: `dump`. Walk until the stones are on screen, then walk on a little: `dump` | New `object cell=314 origin=session ... name=StoneAlpha` (or another stone key) lines appear, each within 1 subtile of the stones' positions (`access-items chests-dump` or the scanner give those). The waypoint shows as `cell=307` if it was seen |
| C | Walk until the Cairn rooms unload: `status` | The objects count does not drop |
| D | Enter the Den of Evil: `status` right away, then again after walking | First `not-switched ... expected_layer=1 layer=0` or `ok` with layer 1; after walking, `layer=1`, with the counts of that layer only |
| E | Return to the Blood Moor / Cold Plains / Stony Field: `dump` | `layer=0`; the earlier cells now show `origin=restored` |
| F | Save and exit, reload, go to Stony Field: `dump` | The Cairn cells are present with `origin=restored` |
| G | Hide the automap and keep walking into new terrain: `status` before and after | Whether the floor and object counts still grow (settles "hidden automap still reveals") |

After every step, `reveal_all=0` should hold. If it is not 0, a reveal cheat is on and the step proves nothing.

### Results, 2026-09-24 (one session, watch on)

- **A:** passed. Town: `layer=0`, `reveal_all=0` all session, `off_lattice=0` on every floor/wall dump, and `in_level` picks the current level's subset of the shared layer.
- **B:** passed. Six `cell=314` objects (`origin=session`) appeared within about 10 s as the Cairn stones came on screen, in a ring about 20 subtiles across. At 11:06:07 the player stood at 5089.9,5094.7 and the scanner said "Cairn Stones, 6 steps, behind"; the nearest decoded stone is 5095,5095, 5.1 away, down-right on screen. Match within about 1 subtile.
- **C:** passed. The object count stayed at 9 until the layer switch.
- **D:** passed. Den of Evil (level 8, layer 1) and Tristram (level 38, layer 25): `not-switched` for about 0.5 s, then `ok` with that layer's counts.
- **E:** passed. Layer 0 came back on leaving the Den, with the earlier object cells.
- **F:** passed for terrain; objects are session-only. The second session revealed 290 new floor cells (`origin=session`) in Black Marsh and Tamoe Highland. After the game was closed and reopened, layer 0 read 2913 floors and 2189 walls, all `origin=restored`: new terrain is saved. Object cells are never resident after a reload (`objects=0`). They are re-added as `origin=session` when seen again; the Inifuss tree and a Dark Wood shrine came back this way.
- **G:** passed. The automap panel was open only 2 s (`[ui] ... OpenPanel "AutoMap"`), yet floors grew steadily through Black Marsh and Tamoe. The game reveals while the automap is hidden. The Pit (level 12, layer 5) started with 30 `origin=session` floors on entry.

## Ruled out

- **Classic 32-bit `AutomapLayer` / `AutomapCell` layouts.** They are still published for D2R, for example `jagarop/d2.re` `D2R/IDA.h`, but they do not match this build.
- **Calling `0xD2240`, `0xD6550`, `0xD6680` or `0xD5360`.** They change state; a switch does disk I/O and frees nodes. The reader calls none of them.
- **Parsing `.ma` for the current level.** It holds only the other layers plus this session's appended cells, with coordinates cut to 16 bits. The node key (from global `0x2A233B8+4`) is still unexplained.
- **`RoomEx+0x50`/`+0x58` and the classic room `AUTOMAP_REVEAL` flag.** The automap code does not read them (see the discovery research).

## Drawable tiles (2026-10-25)

Goal: count the tiles the automap *would* draw, so that the explored share is
drawn cells ÷ drawable cells and reaches 100 on a complete map. Read offline from
`re/D2R.rebuilt.exe` (build 3.3.93787) with the Ghidra project `re/ghidra`.
**Verified** means read from the code. **Hypothesis** means inferred and still
needing a live check.

**Why the walkable ratio saturates.** AutoMap.txt maps only some tiles to a
cell. `1 Crypt` (LevelType 4: the Tower Cellar levels 21–25) and `1 Cave` (Den
of Evil) have **no `fl` rows at all**: their automap draws walls only. A
walkable floor tile there gets a cell only by chance, when a wall cell decodes
onto it.

### Room and tile lists

| Value | Meaning | Evidence |
|---|---|---|
| Room (the 0xB8-byte active room, built by `0x2EF340`) | What `0xD6550` gets: `0xD4310` passes each entry of the player room's near list (`+0x00` array, `+0x40` count; `0x2EFDE0`). The mod already holds these rooms: `NativeWorld::LoadedRooms`, `RoomEx+0x58`. | Verified |
| Room `+0x08` → tile block | `= *(RoomEx+0x88) + 0x20`, passed by `0x326480` to `0x2EF340`. The room is built only if the block has walls or floors. | Verified |
| block `+0x00`/`+0x08` | walls: pointer and `size_t` count (getter `0x2EFDF0`) | Verified |
| block `+0x20`/`+0x28` | floors: pointer and count (getter `0x2EFB70`, null-safe). Counts are asserted to fit 32 bits. | Verified |
| block `+0x40`/`+0x48` | a third list (getter `0x2EFD30`), never passed to `0xD6550`. Used by the collision builder `0x363A90` and the debug labels `0xB9480`. Probably roofs. | Verified unused; *roofs* is a hypothesis |
| Room `+0x80` / `+0x90` | subtile rect / tile rect (x, y, w, h), copied from `0x326480` | Verified |

**Tile record**, stride `0x48`:

| Offset | Meaning | Evidence |
|---|---|---|
| `+0x00`/`+0x04` | pixel offsets used by the draw passes | Verified |
| `+0x08`/`+0x0C` | tile x/y **relative to the room**. Absolute tile = RoomEx `+0x60`/`+0x64` + these. | `0xD5160`; `0x21CB40` uses `×5` + room subtile origin. Verified |
| `+0x18` | flags. `0x8` hidden; `0x400` faded out; bits 14–16 draw layer; `0x20000` drawn on screen; `0x40000` processed by the automap | Verified |
| `+0x20` | tile-library record (non-null: its getters dereference it) | Verified |
| `+0x28` | int tile type/orientation. ≥16 means a lower wall (`0xD5160` drop); 15 is skipped as a roof by the debug overlay `0xB9CD0`; 0 is labelled floor by `0xBA020`. | Verified as used. That it equals library `+0x14` is a hypothesis |

**Tile library**: `+0x14` orientation (`0x1460210`), `+0x18` main index =
AutoMap *Style* (`0x1460190`), `+0x1C` sub index = AutoMap *Sequence*
(`0x1460150`). These are the classic DT1 header positions. Verified.

### `0x32BDF0` AutoMapCell(levelType, orientation, style, sequence)

- **Key.** `levelType` = LevelDefs row `+0x34` (`0x3269B0` → `0x32C200`). This is
  the Levels.txt `LevelType` column, the int that AutoMap.txt `LevelName` is
  converted to at load. Verified.
- **Not in the table bank.** It is a static vector in `.data`:

  | RVA | Contents |
  |---|---|
  | `0x2A9A8C8` | rows pointer |
  | `0x2A9A8D0` | row count (`size_t`) |
  | `0x2A9A8D8` | capacity |
  | `0x2A9A8E0` | `{int first, int end}` row range per level type, 36 slots (end exclusive; unused slots hold −1) |

  Loaded by `0x32CAE0` (from `0x301590`), cleared by `0x32B510`. Verified.
- **Row (0x20 bytes)**, written by `0x32CAE0`:

  | Offset | Field |
  |---|---|
  | `+0x00` | int level type: LevelName looked up in 36 built-in names; unknown → 0 |
  | `+0x04` | int orientation: TileName looked up in 20 built-in names |
  | `+0x08` / `+0x09` / `+0x0A` | u8 Style / StartSequence / EndSequence |
  | `+0x0C` | int Cel1–Cel4 |
  | `+0x1C` | int count of leading Cels ≠ −1 |

  `*Type1–4` are not loaded. The name tables (`0x1CFE110`, `0x1CFE350`) are
  encrypted in the static image.
- **Lookup.** It returns **−1 (no cell)** in three cases:
  - `levelType` is 0;
  - the range's `first` is −1;
  - no row in `[first, end)` matches.

  A row matches when all of these hold:
  - `orientation == +0x04`;
  - Style is `0xFF` or equal to the main index (byte zero-extended against the full u32);
  - StartSequence is `0xFF`, or `Start ≤ sequence ≤ End`.

  **The first match wins.** A count < 1 returns Cel1 as is. Otherwise a Cel is
  picked with the global PRNG `0x2372A40`, so existence is deterministic but the
  cel number is not. In the extracted AutoMap.txt every row except `Expansion`
  has a Cel1. Verified.

### Answers

- **`0x40000`.** `0xD5160` sets it *before* the lookup, so every tile that passes
  the `0xD6550` filter gets it, cell or not. It is a one-shot guard that lives in
  the tile record and is not saved to `.ma`. Whether it survives a room unload is
  unknown. Verified (the lifetime is a hypothesis).
- **Cell position.** The cell's pixels are
  `TilePixel(RoomEx+0x60 + tile+0x08, RoomEx+0x64 + tile+0x0C, tile+0x28)`:
  - `0x334EF0` gives `80(tx−ty)`, `40(tx+ty)`; divided by 10; plus 24 for type ≥ 16.
  - `PixelTile` therefore returns the tile's own `TileKey` for floors and walls of type < 16.
  - Type ≥ 16 would decode at (+3, +3). The extracted AutoMap.txt has no lower-wall TileName (only `fl wl wr wtll wtlr wtr wbl wbr wle wre wld wrd co tr`), so no such cell is expected.

  Verified; the absence of lower-wall cells is a hypothesis.
- **Never drawn in practice:**
  - **Flag `0x8`** is skipped by `0xD6550` and by all four client passes. `0x21E0A0` also sets it later, when a timed tile hide fires.
  - **Flag `0x400`** stops the client passes from setting `0x20000`. `0x21D4F0` sets and clears it as a fading wall's alpha reaches 0, so it is transient.
  - **Off-screen tiles.** `0x20000` is set only for tiles rendered inside the view clip rect (`0x21D090`), and only for the player room's near rooms (`0x21CB40`).
  - **Floor pass limits.** `0x21CB40` draws only floors with draw layer 1–2 and library orientation 0.
  - **No AutoMap row.** Roofs, shadows, lower walls and any style or sequence missing from AutoMap.txt get no cell; the lookup itself excludes them.

  Verified.

### Live checks still open

- Row count `0x2A9A8D0` should be 3287, the data rows of the extracted file. Hypothesis.
- Range slot 4 (`1 Crypt`) should be `{257, 275}` and slot 3 (`1 Cave`) `{424, 550}`, from the file's row order. Hypothesis.
- LevelDefs `+0x34` for level 23 should be 4. Hypothesis.

Fingerprints computed with `LayoutCheck::Hash` on `re/D2R.rebuilt.exe`. The
method reproduces the stored `0xD5160`, `0xD6550`, `0x32C200`, `0x326480` and
`0x192B20` entries.

| RVA | Size | Hash | Covers |
|---|---|---|---|
| `0x32BDF0` | 871 | `0xC031EEC4C5687834` | lookup, globals, row match |
| `0x32CAE0` | 1655 | `0x6C0E84E5C4DE8690` | loader: row fields, `+0x1C` count, 36 range slots |
| `0x2EFB70` | 116 | `0x709027C9B95A4DFE` | floors `+0x20`/`+0x28` |
| `0x2EFDF0` | 81 | `0xC9F6569597E11766` | walls `+0x00`/`+0x08` |
| `0x1460210` | 51 | `0xB00F61F2FA029B02` | library `+0x14` |
| `0x1460190` | 51 | `0x5C5777AD0AFC1012` | library `+0x18` |
| `0x1460150` | 51 | `0x2AA8A3B5F029B132` | library `+0x1C` |
| `0x3269B0` | 17 | `0x0D58E1A6DEBF99A9` | LevelDefs `+0x34` |
| `0x21D090` | 838 | `0x9E00C0ACD0DC5697` | floor pass `0x408` → `0x20000`; only if `0x400` is used |
