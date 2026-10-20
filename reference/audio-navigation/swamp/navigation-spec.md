# Swamp — Navigation System Specification

Reverse-engineered from the native VB6 decompilation of `Swamp.exe` (Ghidra) plus the on-disk map
file format. This documents the **collision**, **zone announcement**, **radar**, **facing/heading**,
and **coordinate** systems so they can be reimplemented in another engine.

Sources cross-validated: `navigation.c`, `collision_disasm.txt`, the map files in `..\maps\`, and
`strings.txt`. Confidence is noted per section. Addresses (`FUN_xxxx`, `DAT_xxxx`) refer to the
decompiled artifacts in this folder.

---

## 0. Core data model (the foundation of everything)

The world is a single fixed **601 × 601 tile grid** (stride `0x259 = 601`). Tiles are stored as a
flat array of tile-type IDs:

The map lookup uses row-major width 601 to obtain a tile ID, then resolves its type in the flag table. Map bases are the form fields `0x1AB` / `0x1BD`; the flag table base is form field `0x20A`. Reference: the map/terrain analysis in this study.

Each **tile type** comes from the map file's palette and carries:

| field   | meaning                                                            |
|---------|-------------------------------------------------------------------|
| color   | RGB integer — identifies the tile when the map is painted as image |
| **flag**| **1 = ground (walkable), 2 = wall, 4 = impassable obstacle**       |
| name    | display name (e.g. `Grass`, `Wall`, `Trees`)                      |
| sound   | sound file base name (e.g. `Wall.wav`, `door.wav`, `Fence.wav`)    |

(There is also a "top surface" tile category used by the inspect command; flags 1/2/4 are the ones
that drive movement and radar.)

Player state (globals; radar copies and the keyboard handler share these):
- `playerX` = `DAT_009d8a58` (also form `[0x356]`), `playerY` = `DAT_009d8a68` (`[0x35d]`) — **tile units**.
- `heading` = `DAT_009d8aa8` — **degrees**, always snapped to a multiple of 45.
- **Orientation convention: `0° = East`, angle increases CLOCKWISE** (screen space, Y-down):
  `East → SE → South → SW → West → NW → North → NE`.

Each cell also has **two zone layers**: `zoneID1`, `zoneID2`, resolved to zone names by id.

Spoken distances are in **"feet" = tileDistance × 24** (`0x18`).

> Confidence: **High** (confirmed independently by the radar code, the collision code, and the map
> file palette).

---

## 1. Collision (walking into walls / obstacles) — the main question

Handled in the movement/keyboard routine `FUN_005b9730`.

### Flow per movement step
On a movement key, the native movement path derives the destination tile from current position and pressed/facing direction, resolves the tile flag and either advances or invokes the obstacle response. The conditions, sound behavior and source functions are documented in the surrounding movement analysis.

### How the collision sound is triggered
The blocked branch builds the path and plays it positioned:
The collision path chooses the obstacle type's sound under `/sounds/Obstacles/`, sometimes selecting a random variant through `rtcRandomNext`, then plays it with its native pan/volume behavior. Reference: collision disassembly at `0x5D464B` and `0x5F62F7`; examples include doors, islands, walls and fences.
- The **sound name is defined by the tile type itself** (the `sound` field in the map palette), so a
  wall tile plays its wall sound, a door tile plays `door.wav`, a fence plays `Fence.wav`, etc.
- It is played through the same positional audio object used by the radar, so the bump is heard in the
  direction of the obstacle.

### What the screen reader says on collision
**Nothing.** A normal bump is communicated **by sound only** (the positioned obstacle sound). This is
deliberate for an audio game — it keeps movement fast and non-verbose.

The spoken line *"Tile ID N is a wall tile named, X. It uses the sound file, Y."* is a **separate
inspect/debug command** invoked by the typed command **`SAY TILE ID=`** (in `FUN_007e63b0`, at
`collision_disasm` 0x801c73–0x802376). It also has variants for ground / top-surface / impassable
tiles. It is **not** part of walking collision.

> Confidence: **High** for the sound path construction, the flag 1/2/4 gate, and "no speech on bump".
> The exact compare/branch instruction that decides blocked-vs-move sits earlier in the 296 KB blob
> `FUN_005b9730` and was confirmed by the model (same `tileFlags[map[...]]` lookup as the radar) plus
> the obstacle-sound disassembly, not line-by-line decompiled.

---

## 2. Zone announcement

Zones are **named regions**; every cell carries two zone ids (`zoneID1`, `zoneID2`). Examples from a
real map: `Lobby`, `North hallway`, `South hallway`, `Cubicle office`, `Door, to exit Press Enter.`

After entering a different tile, the zone announcement path compares both zone IDs with their saved values and speaks enabled zone changes, then records the current IDs. Reference: the zone/area native routines and settings listed in this section.
So: the screen reader announces a zone **only when you cross into a different zone** (id change), and
only while spoken-zones is on. Two layers let a tile belong to both a broad area and a sub-area.

> Confidence: **High** on the data model (two zone layers, names by id, spoken/hidden toggle strings
> `Zones spoken.`/`Zones hidden.`). The exact change-detection call site lives in the skipped 284 KB
> handler `FUN_0050a2d0`; the trigger-on-id-change mechanism is inferred from the model and the toggle.

---

## 3. Radar (`FUN_006e7a70`, "radarstuff")

A **polar ray-march**. Mode `DAT_009d8c28` (0–10) selects which direction(s) to sound and the range:
`0`=silence, `1-4`=Forward/Right/Backward/Left short, `5`=Area short, `6-9`=same dirs long, `10`=Area long.

`FUN_006E7A70` scans the directions selected by radar mode among 16 headings spaced 22.5 degrees apart. It marches at half-tile intervals to 5 tiles in short modes or 40 in long modes, stopping at map bounds or the first wall/impassable tile (flags 2/4). It retains hit type/distance per direction, chooses the wall, impassable or open radar sound and applies bearing/distance pan and volume. The pan floor -5000 marks an out-of-range silent result. The native sound paths and mode mapping are retained in this section.
Key constants: 22.5° (= 360/16), radius 0.5 step, max 5/40, π/180 deg→rad, −5000 pan floor / silence
sentinel.

> Confidence: **High** (full function decompiled and read).

---

## 4. Facing / heading announcement

The heading readout normalizes to one full turn, rounds to the nearest 45-degree sector at the midpoint boundaries and announces the corresponding name. This game's table starts at East for zero, then proceeds South East, South, South West, West, North West, North and North East. Reference: the heading helper and native string table in this section.
The scan/look line also prepends a movement-state word based on a signed "approach" value toward the
currently referenced entity: `|approach|<=0.1 -> "Facing"`, `>0 -> "Heading"`, `<0 -> "Backing up"`,
formatted as `"<word> on <name>."` (or `"<word>."` when no name).

> Confidence: **High** for the degree→name table and the 45° snap. The "Facing/Heading/Backing up"
> field semantics are **inferred** (operands register-passed, not captured by Ghidra).

---

## 5. Direction-to-target ("Go <dir>") and coordinates

Direction to a target point (helper `FUN_00621270`, a quadrant-resolved atan2 in degrees):
`FUN_00621270` derives a quadrant-correct target bearing from the player/target delta, including axis-aligned cases, normalizes it to one turn and rounds through the same eight-direction table. The target readout speaks the corresponding “Go” direction. Coordinate announcements use raw player tile X/Y. See the helper and `returncoords` references here.
Coordinates (returncoords): `speak("You are at coordinates " + playerX + ", " + playerY)` — raw tile
units, no transform.

> Confidence: bearing math **High**; the "Go" and "You are at coordinates" call sites live in the
> skipped 284 KB handler `FUN_0050a2d0` (strings confirmed, driving code not in the dump).

---

## 6. Summary for reimplementation

1. Represent the world as a tile grid with a per-type **flag (1 walk / 2 wall / 4 impassable)** and a
   per-type **sound name**.
2. **Movement**: step; read destination flag; if 1 → move + terrain footstep; if 2/4 → block + play
   `Obstacles/<tile.sound>` positioned toward the obstacle; **no speech**.
3. **Zones**: two id layers per tile; on id change (when enabled) speak the zone name.
4. **Radar**: polar ray-march in 16 directions, first flag-2/4 hit → wall/impassable/open sound,
   panned by bearing, attenuated by distance.
5. **Orientation**: heading in degrees, 0=East clockwise, snapped to 8 compass names; distances in
   feet = tiles × 24.

### Not fully recovered (inside Ghidra-skipped oversized functions)
- Exact block-vs-move branch inside `FUN_005b9730` (296 KB).
- `/zones on|off` + zone-change call site, "You are at coordinates", "Go <dir>" call sites
  (`FUN_0050a2d0`, 284 KB). Strings confirmed; decompile these in the Ghidra GUI per-function if the
  exact code is needed.
