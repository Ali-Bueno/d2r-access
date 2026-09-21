# D2R navigation research and proposed experiment

> Follow-up: the local graph now drives the installed passage radar. See
> [connected passage implementation](radar-connected-passages.md). The audit
> and isolated PoC results below describe the earlier research stage.

Investigated 2026-09-16 against the supplied
`docs/navigation-radar-research.md`. Read together with
[CurrentRadarAnalysis](CurrentRadarAnalysis.md). Sources were inspected as source
code, not executed or incorporated as dependencies.

## Recommendation

Use the **existing direct D2R collision snapshots** to build a local graph at
one collision subtile per cell. Separate immediate straight clearance from
connected exploration. Establish stable passage identities before making those
connections drive automatic cues. A new map generator or BFS by itself will not
settle the current audio clutter.

| Strategy | What it supplies | Assessment for this plugin |
|---|---|---|
| A: direct active-room collision | Current terrain, objects, doors and actor occupancy at native resolution | Preferred; already implemented and version-checked. Loaded coverage only |
| B1: port generator algorithms | Full deterministic geometry if all generation/data dependencies are reproduced | The reference delegates generation to Legacy engine DLLs; this is not a small standalone algorithm port |
| B2: embed Legacy library | Existing generator wrapper | Its generator is x86 and loads classic DLLs/MPQs; unsuitable for direct loading into this x64 plugin |
| B3: bundle helper | Seed/difficulty/area -> generated map through a pipe | Technically exemplified by D2RMH, but adds a process and Legacy assets/version dependence; unnecessary for local navigation |
| C: generated static map + live overlay | Far static coverage with current local obstacles | Possible later; requires proving geometry parity, coordinate registration and dynamic reconciliation. Adds work without solving cue grouping |
| Local observed-map cache + fresh occupancy | Previously seen topology and current physical blockage | Worth pursuing after the PoC. Old observations must retain age/coverage rather than claim current passability |

No project binaries, game assets, extra DLLs, runtime offsets or GPL source have
been copied into the mod. The plugin remains an MIT project. `d2mapapi_mod` and
D2RMH have MIT licenses; MapAssist has GPLv3. This study uses conceptual findings
and references; the PoC is independently written against our existing adapter.

## Sources pinned for reproducibility

| Repository | Inspected revision | Relevant files |
|---|---|---|
| [d2mapapi_mod](https://github.com/soarqin/d2mapapi_mod/tree/f61d05244f323409aa48b326033639326d7c285c) | `f61d052` | `mapdata.cpp`, `collisionmap.*`, `pathfinder.cpp`, `session.cpp`, `d2map.cpp`, `d2structs.h`, CMake/README/LICENSE |
| [D2RMH](https://github.com/soarqin/D2RMH/tree/32d55b8ab9a3e9b380103e73e3c8d328cd4f3ad4) | `32d55b8` | `src/d2r/processdata.cpp`, `d2rdefs.h`, `src/ui/maprenderer.cpp`, `src/main.cpp`, `doc/ChangeLog.md`, LICENSE |
| [MapAssist](https://github.com/ResurrectedTrader/MapAssist/tree/6b2630352229d2990adc9bf6924d6d3a02c87087) | `6b26303` | `Helpers/MapApi.cs`, `ProcessContext.cs`, `GameMemory.cs`, `Structs/{Path,Room,RoomEx,Level,MapSeed}.cs`, `Types/{RawAreaData,AreaData}.cs`, LICENSE |

These are historical implementations. Similar field offsets are corroboration,
not validation for D2R 3.3.93787 or its newer content.

## How d2mapapi produces the graph

`Session::getMap` calls Legacy `D2COMMON_LoadAct` using seed/difficulty. `MapData`
initializes the requested level, temporarily adds inactive room data, copies
`Room1::Coll()->pMapStart` and removes temporary rooms. Logical level coordinates
are multiplied by five; collision origins are already in subtiles. Preset objects
use `roomTileOrigin*5 + presetOffset`.

`MapData::genPathData`, lines 417-476, reduces geometry into 5x5 blocks. It connects
horizontal neighbors if any of three rows has a four-cell clear seam; vertical
neighbors use the transposed test. Both directions are set. It tests wall bit 1,
without our runtime movement mask or native body footprint. Different accepted
seams can belong to disconnected fragments **inside one coarse node**. Its
connectivity is therefore not an exact substitute for full-body reachability.
See [mapdata.cpp](https://github.com/soarqin/d2mapapi_mod/blob/f61d05244f323409aa48b326033639326d7c285c/mapdata.cpp#L417).

There are three easily confused containers:

| Container | Contents |
|---|---|
| `CollisionMap::mapData` | Row run lengths alternating blocked/open, terminated by -1; full subtile resolution, derived from wall bit 1 |
| `CollisionMap::pathData` | Run-length encoded `(direction byte,count)` pairs |
| `CollisionMap::path` | Decoded direction bytes on the coarse 5x5 graph |

`CollisionMap::decode` expands the direction runs; `extractCellData` separately
renders collision runs and optional visible border cells. The border operation
is useful for drawing an outline; it supplies no new route semantics.
[collisionmap.cpp](https://github.com/soarqin/d2mapapi_mod/blob/f61d05244f323409aa48b326033639326d7c285c/collisionmap.cpp),
[collisionmap.h](https://github.com/soarqin/d2mapapi_mod/blob/f61d05244f323409aa48b326033639326d7c285c/collisionmap.h).

`pathFindBFS`'s parameter named `mapData` is actually the **decoded direction
graph**, not collision RLE. Offsets at line 27 confirm:

| Bit | Index delta | World-grid direction |
|---|---:|---|
| 1 | -1 | -X / left |
| 2 | +1 | +X / right |
| 4 | -width | -Y / up |
| 8 | +width | +Y / down |

It follows outgoing bits, bounds-checks row crossings, records BFS parents and
returns target-to-start nodes. The optional merge simplifies the route with
further directional-edge checks. It does not recover details lost during
coarsening or describe rooms/doors. The displayed function's initial validation
also omits negative-coordinate checks; we should not transplant its API unchecked.
[pathfinder.cpp](https://github.com/soarqin/d2mapapi_mod/blob/f61d05244f323409aa48b326033639326d7c285c/pathfinder.cpp#L16).

## What D2RMH actually does

`ProcessData::update` reads D2R process memory: focused player's dynamic path,
Room1 -> Room2 -> Level, difficulty and an obfuscated seed. Historical seed
selection uses a check field and one of two values. `MapRenderer::getMap`
caches by session seed/difficulty and level; it queries `PipedChildProcess`.
`main.cpp:36` starts `d2mapapi_piped.exe` against a Legacy installation. D2R CASC
support elsewhere supplies display/game data, not a replacement map generator.
[processdata.cpp](https://github.com/soarqin/D2RMH/blob/32d55b8ab9a3e9b380103e73e3c8d328cd4f3ad4/src/d2r/processdata.cpp#L119),
[main.cpp](https://github.com/soarqin/D2RMH/blob/32d55b8ab9a3e9b380103e73e3c8d328cd4f3ad4/src/main.cpp#L36).

The renderer subtracts generated map/crop origin and divides by five before
calling `pathFindBFS` on `CollisionMap::path`. Rendered route points are scaled
back into map coordinates. This is an overlay route approximation, not a D2R
movement query. Its changelog records walkable-area/border rendering and later
BFS/target lines; none constitutes a precise reactive accessibility radar.
[maprenderer.cpp](https://github.com/soarqin/D2RMH/blob/32d55b8ab9a3e9b380103e73e3c8d328cd4f3ad4/src/ui/maprenderer.cpp#L554),
[ChangeLog](https://github.com/soarqin/D2RMH/blob/32d55b8ab9a3e9b380103e73e3c8d328cd4f3ad4/doc/ChangeLog.md).

## MapAssist and exits

MapAssist's historical `Path`, `Room`, `RoomEx`, `Level` fields corroborate our
coordinate high words and room chain. `ProcessContext` derives base addresses
with byte-pattern/RIP displacement searches; its version-specific constants are
not a replacement for our installed-binary evidence.
[structures](https://github.com/ResurrectedTrader/MapAssist/tree/6b2630352229d2990adc9bf6924d6d3a02c87087/Structs),
[ProcessContext](https://github.com/ResurrectedTrader/MapAssist/blob/6b2630352229d2990adc9bf6924d6d3a02c87087/Helpers/ProcessContext.cs).

`MapApi.StartPipedChild` launches `MAServer.exe`; `GetMapDataInternal` sends
seed/difficulty/area and decodes returned JSON. `RawAreaData.GetCollisionGid`
expands collision runs. `AreaData.CalcViewAreas` marks display borders using
neighbor pixels. Those are generated rendering data, not freshly read D2R body
collision. `AdjacentLevel` carries area identity, exits and a portal flag.
[MapApi](https://github.com/ResurrectedTrader/MapAssist/blob/6b2630352229d2990adc9bf6924d6d3a02c87087/Helpers/MapApi.cs#L447),
[RawAreaData](https://github.com/ResurrectedTrader/MapAssist/blob/6b2630352229d2990adc9bf6924d6d3a02c87087/Types/RawAreaData.cs#L134),
[AreaData](https://github.com/ResurrectedTrader/MapAssist/blob/6b2630352229d2990adc9bf6924d6d3a02c87087/Types/AreaData.cs#L28).

d2mapapi matches tile presets to RoomTile destinations for portal exits; walkable
area-edge intervals produce approximate normal exit locations. Its own README
notes non-portal positions can be inaccurate. NPC/object presets include typed
identifiers, useful conceptually for waypoints/doors. Our current runtime adapter
does not yet expose verified preset/warp associations. A hole in collision alone
cannot be named as a specific destination area.
[exit implementation](https://github.com/soarqin/d2mapapi_mod/blob/f61d05244f323409aa48b326033639326d7c285c/mapdata.cpp#L217),
[format and limitations](https://github.com/soarqin/d2mapapi_mod/blob/f61d05244f323409aa48b326033639326d7c285c/README.md).

## Direct rooms, automap and coverage

This installation already proves active-room collision allocation, neighbor
lookup and unloading. Logical rooms have rectangles and adjacency, but their
internal collision can be freed when inactive. A local complete graph is possible
without an external process; a complete unexplored level is **not established**.
Reading neighboring active rooms is sufficient for the local PoC. No room
creation or automap reveal call is needed. [Native lifetime evidence](sonar-radar.md).

The installed `AutoMapHD.json` is a rendering widget, not a walkability service.
Classic D2BS `AutomapCell` contains a drawable cell ID, pixel coordinates and
tree links; `AutomapLayer` holds floor/wall/object/extra collections. This is
useful rendering/annotation evidence, not body-valid movement adjacency.
[D2BS definitions](https://github.com/noah-/d2bs/blob/master/D2Structs.h#L42).

The **current D2R automap drawing/reveal function and cell-list layout have not
been derived in this investigation**. Classic names do not prove current RVAs,
pre-reveal availability or authoritative door semantics. Mapping that pipeline
would be additional RE; no evidence makes it superior to the collision grid for
the immediate objective. This remains an explicit gap, not an assumed API.

## Answers to the supplied document's fourteen questions

1. Collision comes from guarded copies of active D2R room grids.
2. Remaining clutter primarily concerns changing ray hits without passage
   identity; it is not established that the underlying flags are broadly wrong.
3. Yes, we already read the real grid; see `NativeWorld::ReadPlayer/ReadRoom`.
4. Room `+0x38`, header origins/dimensions and data `+0x20`; 16-bit collision
   subtiles, five per logical map tile. Runtime movement mask plus native body
   pattern determines destinations. Terrain, props, doors and units can overlap.
5. Seed generation is possible through Legacy engine wrappers; its parity with
   all current D2R content remains unverified and it is unnecessary locally.
6. D2RMH bridges runtime seed/position/level to a Legacy helper and draws results.
7. Reuse the concepts of local adjacency, reachability, coordinate contracts and
   caches; retain full subtile resolution and native body checks.
8. Complete loaded local coverage: yes. Entire unseen detailed level: unproven
   without generation/activation; logical rectangles are insufficient.
9. Subtract each grid's subtile origin. Do not divide our native positions by five.
10. Stitch known active-room grids in absolute coordinates; unknown seams block
    claims of reachability, not become announced walls.
11. Use lifecycle session generation and native level for current snapshots.
    A future cache also keys body pattern/mask and coverage/dynamic changes.
12. Follow full-body-valid neighboring destinations from the player. Preserve
    the native eight-direction trace behavior and the origin-overlap escape rule.
13. Local graph/flood fill is O(cells + edges), with at most eight edges per cell.
    Measure the PoC; do not run a whole-act BFS every frame or cache doors forever.
14. A local navigation layer over the existing native adapter is the smallest
    install-and-play option. No helper, new dependency or seed offset is required.

## Proposed PoC and acceptance boundary

The independent `Navigation::Grid` exposes body-valid cells, eight-direction
edges, shortest-route first steps, additional clearance and unknown frontiers.
Use original movement collision, not the furniture-filtered structural map, as
the reachability source. Keep world coordinates fixed. Output is an ASCII dump
and test diagnostics; it does not feed radar sounds or spoken classifications.

Test a front wall, offset doorway, disconnected region, narrow body gap, corner,
room seam, new session/level, native origin overlap and captured camp geometry.
Also test a destination across a wall reachable only by moving away first: this
is a counterexample to classifying sectors by reachable-cell count alone.

After this base is validated, evaluate stable connected openings/portals with
width and route-entry direction. Group one physical encounter across directions.
Consider an on-demand exploration readout and separately optional immediate
collision feedback. Keep automatic cues sparse by meaning, with confirmation
only after correct geometry. BFS does not label rooms or guarantee quieter audio.

Live validation in Blood Moor, Den of Evil, doors and transitions is still needed.
The PoC's synthetic cases and historical camp capture cannot replace it. No new
claim about online compatibility follows from this read-only design.

## PoC result and reproduction

Implemented in `src/navigation_grid.h/.cpp`, linked **only** into the new
`navigation-grid-tests` target. No runtime service or sound path calls it.
Build with `.\build.ps1`; run `build\navigation-grid-tests.exe`, or add `camp`
to print the processed actual camp capture. `ctest --test-dir build
--output-on-failure --timeout 20` runs the full suite.

- **36 new checks pass; 26 suites / 1284 checks pass overall.**
- A constructed 15x5 corridor has both coarse seam tests clear but a wall
  dividing the middle 5x5 node. The full-resolution graph correctly rejects
  that coarse graph's false connection.
- The offset-doorway case reaches the other side while the original straight
  sweep remains blocked. A solid partition keeps empty cells disconnected.
- A thin opening passes a point body but rejects the native cross. All six
  native body patterns and origin-overlap escape are covered.
- The detour counterexample retains legal shortest-route initial directions,
  rather than claiming the destination bearing is directly passable.
- The shared historical camp fixture yields **380 reachable body centres**;
  the post remains blocked and its verified detour is reachable. Unknown cells
  outside the capture remain unknown. Only classification, not original flags,
  survived in that capture.
- A 41x41 local graph build averaged about **0.14 ms** over 200 iterations on
  this machine. This excludes live snapshot cost and future passage extraction.
- The graph records current session/level/pattern/mask and rebuilds fresh local
  state. Persistent room caches and semantic doorway tracking are future work.
- Build and installed DLL hashes remain
  `DF188862EEAC1511A7D4FBA563A27ED682D8D61725DDD0C8E5F634EC69C18C2C`.
  The user's partially improved object-filtering radar is unchanged.

The PoC establishes that the present data source supports local connectivity.
It does **not** yet establish fewer audible cues or reliable room/corridor labels.
