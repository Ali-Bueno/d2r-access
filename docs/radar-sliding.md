# Wall contours and sliding investigation

The later [short return visit correction](radar-revisits.md) adds remembered
turn locations and supersedes this document's current test/deployment status.

2026-09-16. The user reports a downward wall warning followed by a downward
opening while continuing to hold the stick down, and proposes preventing wall
sliding. This update corrects a reproduced radar defect and captures geometry
automatically. It does not change native movement or disable sliding.

## Evidence and correction

The latest session has 109 generated events, including 26 down events, and 282
opening-hold reason changes. It contains no full map snapshots. At event
433806484, `(4525,4681)` reports a down hit at `(4526,4682)` with zero clearance.
At 433806687, `(4525,4682)` reports a down opening with 12 subtiles of physical
clearance. These records establish a short displacement and changing ray
clearance, but do not identify the actual boundary or prove which event the
user heard. The user's continued downward input is direct user evidence.

An independent static-map fixture reproduces a false opening after retreat and
travel along a solid wall with a two-subtile raster jog. A curved solid face
also reproduces it while remaining inside the structural window. There is no
door or wall end in these fixtures. The former supporting-line comparison
mistook the changed face for travel beyond the remembered wall.

`WallMemory` now receives the current structural snapshot. When a supporting
line no longer matches, it can retain the actual contour between old and new
body hits. The bounded search follows blocked body centres with an open
neighbor on the approaching side. Its strip allows the combined half-cell
diagonal raster uncertainty of two hits. It cannot cross a free/unknown cell
or take an arbitrary route around a room to connect its walls. The original
full-body movement checks still decide whether an opening is usable.

This is a local continuity correction, not general room topology. A larger
recess or a boundary turning out of the loaded structural window can still
sound as widening. The tests deliberately distinguish an observed curved face
from a face beyond coverage. Suppressing every connected boundary would also
hide actual doors whose jambs connect to a back wall.

## Automatic collision records

Every cue-producing tick now calls `RadarTrace::Snapshot` after audio submission,
once for all simultaneous directions. It records the entire structural window
of original movement flags (87 by 87 cells for the current default cross body),
not just the previous 25 by 25 character picture. The radius is shared with
`StructuralWorld`. Unknown cells are encoded as the native blank bit, 0x20.

`[radar-collision]` records identify event time, session, level, map origin/size,
native body pattern, movement mask, fractional position and chunk number/count.
`data` is a row-major sequence of `hex_flags:decimal_run_length,` runs. `start`
is the first cell index of that chunk. Check every part and offset before replay;
the bounded asynchronous logger may drop diagnostics under disk pressure.
Payloads stay below its 4096-byte record limit. `[radar-reference]` records the
four reference points/normals, hits and required approach distances.

These are event snapshots, not an every-poll movement trace or a whole-level
map. They cannot reconstruct all intervening motion or collision beyond the
structural window. No extra key press is needed to capture a sounded event.

## Preventing sliding: investigation, not an installed feature

The SDK's `InputService` exposes action registration/bindings, not a movement
solver switch. `InputDirection` reads XInput for accessibility controls; it does
not filter the game's movement input. The reviewed extracted gameplay settings
did not expose a wall-sliding option.

Read-only Ghidra analysis of the existing `re/ghidra/D2R` / `D2R_live.exe` project:

| Native RVA | Finding |
|---|---|
| `0x3ACC60` | Derives a direction/endpoint using the native collision trace `0x3C5610`; shared by `0x12A180` and `0x429860`. |
| `0x429860` | Selects path type `0x11` on the directional branch and calls direction setter `0x342750`; the fallback uses default type 7. |
| `0x342750` | Normalizes the requested vector into `Path+0x22C/+0x230` and writes the associated float at `+0x234`. These fields have not been validated during the user's slide. |
| `0x340E70` | Dispatches path generation through the table at `0x1D000C0`, indexed by `Path+0x50`. |
| `0x3812D0` / `0x3822B0` | Shared path update and displacement calculation, including a type-17 branch. They are not established as player-only hook sites. |
| `0x381C60` / `0x3827A0` | Apply destination steps and native collision, updating collision results and path progress. Stopping here affects more than choosing a manual direction. |

The rebuilt file's path-dispatch table slots do not resolve to valid image
functions. Do not derive a hook from those reconstructed data bytes. The next
RE step is a guarded live read of the type-17 dispatch entry and desired/actual
vectors during a user-led slide, followed by verifying the local-player/manual
walk branch. The current investigation does not establish that a generic
collision hook, a coordinate rollback, or a zeroed XInput stick would safely
disable only wall sliding. No movement hook, new native dependency or game
memory write was added.

## Validation

31 suites / 1836 checks pass. The new contour suite has 277 checks covering four
fixed directions, 50/100/150/200 ms sampling, observed raster/curved faces,
body-valid sliding fixtures with continued directional intent, and a genuine
door connected to a back wall. The sliding fixture is not a native controller
emulator. The new 49-check snapshot suite round-trips raw flags, actors and
unknown coverage across numbered chunks and checks the actual log-size limit.
All former doorway/revisit/retreat, audio-delivery and UI regressions pass.

Build and deployment match SHA-256
`BBD91F823BF77D0838291D7C8E6966D88B0312FFC050C5DE0E568F08662A63DF`.
No live controller/listening validation of this update was performed.
