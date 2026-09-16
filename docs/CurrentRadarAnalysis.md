# Current radar analysis

> Superseded by the 2026-09-16 redesign (docs/radar-redesign.md); retained as history.

> Follow-up: the local graph now drives the installed passage radar. See
> [connected passage implementation](radar-connected-passages.md). The audit
> and isolated PoC results below describe the earlier research stage.

Audit of commit `a4e54b8`, 2026-09-16, prompted by
`../D2R_Navigation_Radar_Research.md`. The user confirms object filtering helps,
but still finds the radar cluttered. This audit precedes the navigation-grid PoC.

## Finding

**We already read D2R's real room collision grids.** Replacing the data source
with a Legacy map generator would not address the main remaining limitation:
automatic sounds describe changing ray hits, not persistent passages or regions.
The existing scans test intervening full-body positions; they do not jump through
a wall to sample an empty endpoint. Some premises in the supplied document apply
to a simpler probe implementation, not this revision.

## Data flow

```text
RadarService::Tick (50 ms poll, gameplay/UI/lifecycle gates)
  NativeWorld::Read -> ReadPlayer -> ReadRoom (active room + loaded neighbors)
    World: native position, session, level, body pattern, movement mask, grids
  Radar::Engine::Tick
    physical Scan + actor-filtered terrain Scan
    StructuralWorld -> full-body Scan -> BoundaryTracker (four fixed axes)
    current physical clearance validates each opening
  RadarAudio::Validate / Play -> serialized flat stereo cues
```

| Responsibility | Existing implementation | Meaning |
|---|---|---|
| Local player | `src/native_world.cpp:179`, `ReadPlayer`; loader-resolved lookup at RVA `0x9A480` | Verified player identity, not the opaque inventory SDK handle |
| Position | `Unit+0x38 -> Path`; packed X/Y at `Path+0/+4` | High unsigned words are collision subtiles; low words retain 16.16 motion |
| Collision | `ReadRoom`, `src/native_world.cpp:76` | `Path+0x20 -> ActiveRoom`, grid pointer at room `+0x38` |
| Grid layout | Header X/Y/W/H at `+0/+4/+8/+C`, 16-bit buffer at `+0x20` | Absolute subtile origin, row-major cells |
| Coverage | `ReadPlayer` active-neighbor traversal | Neighbor array `Room+0`, count `+0x40`; 152-subtile snapshot radius, bounded room/cell copies |
| Level | ActiveRoom `+0x18 -> RoomEx`, `+0x90 -> Level`, ID `+0x1F8` | Checked against lifecycle area identity |
| Footprint | `World::Body`, `src/radar_grid.cpp`; pattern from `Path+0x5C` | Point, five-cell cross or 3x3 square; native `0x364C70/0x366D40` |
| Movement restriction | `Path+0x64`, native wrapper `0x3C5610` | Runtime mask, normally `0x1C09` in recorded walking sessions |
| Player occupancy | `RemovePlayerPresence` | Removes the verified local stamp in our copies; does not alter game memory |
| Direction | `src/radar.cpp`, fixed `Axes` | Screen/stick up, left, right, down; never heading-relative |
| Distance | `SweepBody`, `Scan`, `src/radar_grid.cpp` | Half-subtile sweep samples, full-body first hit plus three point lanes |
| Geometry sounds | `BoundaryTracker`, `src/radar_geometry.cpp` | Hit displacement/surface changes inside a 20-subtile local range |
| Physical opening guard | `EvaluateGuidance`, `src/radar_guidance.cpp` | Original body/median clearance must reach eight subtiles |
| Presentation | `src/radar_audio.cpp`, `src/radar_synth.cpp` | 120 ms cues, 50 ms gaps; only unstarted failures can retry |
| Manual inspection | `src/radar_readout.cpp`, service `Dump` | Ctrl+T reports original physical clearance and captures a local map |

Offsets above were already derived from this installed D2R 3.3.93787 binary and
guarded with 19 function fingerprints. See [native evidence](sonar-radar.md).
No new offsets were inferred from historical repositories during this audit.

## Coordinate contract

For a world cell `(x,y)` in a room grid, subtract the grid's own subtile origin;
index is `(y-originY)*width + x-originX`. There is **no division by five** on this
path. Logical map tiles are five subtiles wide; native grid header coordinates
and the player's integer position are already in subtiles.

Native projection `0x334DB0` uses screen deltas proportional to
`(80*(dx-dy), 40*(dx+dy))`. Consequently up/left/right/down are world diagonals
`(-X,-Y) / (-X,+Y) / (+X,-Y) / (+X,+Y)`. The supplied document's world-cardinal
bits must not be mistaken for these four screen directions. The user previously
requested fixed stick directions; new connectivity must preserve that convention.

## Why the result can still be confusing

1. **A changed hit is not necessarily a new passage.** `Difference` compares
   projected hit positions with a 1.5-subtile threshold. Surface tracking and
   small-facet coalescing reduce this, but irregular connected terrain retains
   many real changes that may be irrelevant to exploration.
2. **Four independent histories can describe one encounter.** The queue keeps
   cues distinguishable in time; it does not identify two directional events as
   the same doorway/corner. A burst of correct local events can remain cluttered.
3. **Straight obstruction is different from reachability.** A side opening can
   provide a route around the central hit. The current ray cannot describe that
   route. Conversely a reachable region may require initially moving away from
   it; reporting its final compass bearing as an immediately open direction would
   introduce a new false interpretation.
4. **Clutter filtering is conservative.** `StructuralWorld` removes bounded
   components with verified detours, but retains large/attached components and
   unknown detours. It runs connectivity around each candidate object, not from
   the player across the navigation space. It provides no passage identity.
5. **Unknown is a coverage limit.** Missing rooms remain unknown, not blocked or
   open. Streaming can reset boundary history; the code does not possess detailed
   collision for every room in the whole level. Guarded reads are not an atomic
   snapshot of all client state, so dynamic mismatches remain possible.
6. **Geometry and occupancy have different lifetimes.** Actors affect immediate
   physical movement, but should not repeatedly split semantic rooms. Shared
   collision bits do not prove independent ownership of overlapping stamps.

There is no evidence in the latest log of a new five-to-one coordinate error,
audio retry loop, or rays skipping through walls. It would be premature to label
all remaining events false collision. The demonstrated gap is spatial meaning.

## Latest live evidence

The 08:47:20 load session generated **120 cues, zero retries**, with no Ctrl+T
snapshots. Structural hit flags are mainly wall-related; the new `structure`
diagnostic confirms objects can be skipped while physical clearance still sees
them. This is not comparable as a rate to earlier runs: routes and durations vary.

Example: down opened at position (4473,4683), hit (4489,4699), depth 21.5;
281 ms later (monotonic event times) it closed at (4476,4683), hit (4489,4696),
depth 17.5. This documents
a rapid geometry transition, **not proof of an incorrect wall**. Full per-poll
map snapshots were not saved, so a faithful replay of this movement is unavailable.

## Concrete next experiment

Build an isolated full-resolution navigation grid from the existing `World`:
native body-valid destination cells, eight-direction edges, local reachability,
first steps of shortest routes, known coverage and clearance. Keep it separate
from automatic audio. Tests must include offset doorways, disconnected space,
body-sized gaps, room seams, origin overlap and a reachable destination requiring
an initial move away from it. The captured camp grid supplies a real local fixture.

A later radar can group stable passages and arbitrate one encounter across
directions, while retaining immediate straight collision. A successful flood fill
alone is **not** evidence that the final sound design is solved.
