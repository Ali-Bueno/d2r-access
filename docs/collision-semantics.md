# Collision semantics (D2Common ground truth)

What the game's own collision rules are, and how our layers map onto them. Source: D2MOO, the
open-source D2Common reimplementation; D2R keeps these layouts byte for byte (our `Room1+0x38` grid
struct and `Path+0x5C/0x60/0x64` match it).

- `source/D2Common/include/D2Collision.h` — the bit values.
- `source/D2Common/src/Path/Path.cpp` — path types, step loop, unit footprint stamp/remove.
- `source/D2Common/src/Path/PathMisc.cpp` — pattern dispatch, wall following, invalid-room mask.
- `source/D2Common/src/Path/AStar.cpp` — node budget and 8-neighbour expansion.

## The collision word (uint16 per subtile)

| Bit | Name | Meaning |
|---|---|---|
| 0x0001 | WALL | impassable terrain |
| 0x0002 | VISIBLE | blocks line of sight only |
| 0x0004 | MISSILE_BARRIER | blocks missiles |
| 0x0008 | NOPLAYER | players may not walk here |
| 0x0010 | PRESET | preset/alternate floor marker |
| 0x0020 | BLANK | invalid subtile: no data |
| 0x0040 | MISSILE | missile present |
| 0x0080 | PLAYER | player footprint present |
| 0x0100 | MONSTER | monster footprint present |
| 0x0200 | ITEM | item present |
| 0x0400 | OBJECT | object collision |
| 0x0800 | DOOR | closed door |
| 0x1000 | NO_PATH | a unit's own centre marker |
| 0x2000 | PET | pet footprint present |
| 0x8000 | CORPSE | corpse present |

WATER is `0x00C0` (MISSILE|PLAYER), not a bit of its own. An out-of-room query returns `0x0027`
(COLLIDE_MASK_INVALID), never 0.

## The walking player

- Move-test mask = WALL|NOPLAYER|OBJECT|DOOR|NO_PATH = `0x1C09`. We read it live from `Path+0x64`.
- MONSTER and PLAYER are **not** in it. Units block the player only through NO_PATH, which the game
  stamps on the centre of every present unit's footprint (small units: centre; big units: 5-cell
  cross). Pets stamp PET, which never blocks.
- Player stamp = PLAYER on the 5-cell cross plus NO_PATH on the centre. Player size is hardcoded 2,
  so the player's pattern is always 1 (the cross); a point body is a misread, not a smaller player.
- The game removes a unit's whole footprint before pathing and re-adds it after.
- Body test at (x,y) uses the pattern **at that cell**: 0 = the single cell; 1/3/5 = the 5-cell cross;
  2/4 = the 3x3 box. A* expands 8 neighbours testing only the destination cell. The cross implicitly
  covers both corner cells, so a cross body cannot cut a diagonal corner; a point body can.
- Doors: a closed door's cells carry DOOR and block; opening one clears its bits. The live grid
  already encodes door state, so we never model doors separately. **Except** the Jail's (MEASURED
  2026-10-22, a tester's log, Jail Level 2 at 17875,6644-6646): a three-cell shut door whose cells
  carry OBJECT alone, cleared when opened. The kept map tells it from a chest by the shut door
  units nearby (`LevelRoute::Memory::NoteDoor`, docs/route-description.md).

## Player path type: STRAIGHT

- Ray-trace toward the target, testing the pattern at every subtile.
- On a failed test, wall-follow for up to 73 steps, trying 3 candidate directions (preferred +/-45).
- A* only if the target is within 18 subtiles, and at most 200 nodes.
- Nothing is computed beyond +/-100 subtiles.

## How our layers map to it

- **Movement layer** (`World::Cell`, `Layer::Movement`): raw cell AND the live mask. Keeps NO_PATH, so
  monsters and other players block exactly as they do in game. Used by the snapshot diagnostics and by
  the navigation grid when it is handed a physical world.
- **Terrain layer** (`Layer::Terrain`, `TerrainCollisionFlags` in `src/radar_grid.cpp`): drops the
  bits only a live unit ever writes — MONSTER, NO_PATH, PET — from any cell carrying a unit bit. The
  PLAYER bit itself survives wherever MISSILE marks the cell (WATER shares both bits and is terrain).
  MISSILE is never stripped for the same reason. A unit standing in a doorway must not close it.
- **Structural world** (`src/radar_structure.cpp`): the terrain layer plus removal of bypassable
  clutter (components of WALL/OBJECT only, at most one map tile across, with a proven detour) while
  the game slides round it; with wall-slide prevention on, clutter stays. This is the layer the
  radar's `ScanPassage` reads, so no actor can ever produce a wall cue.
- **Beacon** (`src/beacon.cpp`) and **rooms** (`src/rooms.cpp`) read the terrain layer too.
- **BLANK is Unknown on every layer**, as are cells outside any streamed room: never a wall, never an
  opening. `0x27` contains BLANK, so an invalid room answers Unknown as well.
- Self-stamp: `NativeWorld::RemovePlayerPresence` clears the local player's own footprint and marker
  from our copy. A stamp is removed atomically — never the footprint bit alone — because an orphan
  NO_PATH looks exactly like a bare barrier (the false wall of 2026-09-16).
