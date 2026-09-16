# Radar surface tracking after the running feedback

Investigated 2026-09-16. The user rejected the 07:49 build after hearing repeated
openings followed by closing on the same side while running. This supersedes the
interpolated-path approach in [radar-precision.md](radar-precision.md).

**Follow-up:** the user rejected this build's audible result too. The 08:26
session had no retries but retained geometry chatter. See
[structural object filtering](radar-object-filtering.md) for the subsequent fix
and current validation; the results below describe this historical revision.

## Evidence from the game

The 08:00:57 load session recorded 148 generated cues, 139 submissions, 122
completed buffers, 17 interrupted buffers and nine drops. Six generated cues
were retries. These are software events, not an audio recording.

At 08:01:13, three left openings were generated at monotonic times 421168546,
421168609 and 421168796. Their clearances were 53, 9 and 64 collision subtiles.
The first two were only 63 ms apart; all three buffers completed. Other examples
included two up openings at the same reported cell, 78 ms apart. Interrupted
chirps were also retried, sometimes after most of the 120 ms cue had played.

The earlier **actual camp grid**, saved at 2026-09-15 21:06:53 around (4472,4622),
contains an isolated blocking cell at (4466,4629). With the native five-cell body:

- At (4473,4621), the left ray rejects body centre (4466,4628), leaving exactly
  **9 subtiles** of clearance. This matches the 08:01:13.708 event's hit/depth.
- That centre cell is itself empty: an adjacent part of the player's body hits
  the object. A point ray would incorrectly pass through it.
- One cell over, at (4472,4621), the same ray bypasses the object. Thus the map
  really can alternate between a distant boundary and a nearby tiny object as
  the character crosses individual subtiles.

`RecordedCampPost` retains that 25x25 classification grid verbatim in
`tests/radar_surface_tests.cpp`. Outside it remains unknown; original bit values
and the complete 08:01 movement trajectory were not recorded. This verifies the
local obstruction, **not a replay of the whole latest session**.

## A Hero's Call: source inspected

Read the recovered C# directly, including
`D:/code/c#/a-heros-call/decompiled/src/Sound/Ian/ReactiveRadar.cs`, `Line.cs`, and
the collision/movement reference checked against those sources.

- `ReactiveRadar.cs:162–232` tracks the line through successive ray hit points.
  Collinear hits on one wall remain silent despite movement along that wall.
  A hit moving across the old supporting line signals widening/narrowing.
- `Line.cs:102–116` supplies the side test and sign-independent slope comparison.
  The slope-only equality works there because successive fits share a point.
  Matching normals without checking offsets would lose parallel recesses.
- A cue consumes the tracked line and forces a fresh baseline. Replaying every
  small change in one corner is not equivalent to that behavior.
- Three facing-relative beams and a global 100 ms post-cue gate are separate from
  the approach detector (three distance buckets under three AHC tiles). AHC's
  exact floating-point movement-direction comparisons are not suitable for an
  analog controller port.

The port uses the supporting-surface and consume-on-event ideas. It does not
claim to reproduce AHC's point-body movement, sound assets or approach pitches.
Four fixed screen directions remain, including down/rear, as requested for D2R.

## D2 collision and movement constraints

Re-read this installation's native decompilation of `0x364700` (destination-cell
trace), `0x364C70`/`0x366D40` (body pattern), `0x3C5610` (runtime movement mask),
and the previously recovered `0x340E70`/`0x343920` alternate collision-tested
movement candidates. These are native C++ functions, not Unity physics.

The classic [D2MOO collision implementation](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/src/D2Collision.cpp)
and [collision definitions](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/include/D2Collision.h)
corroborate the grid/footprint model; D2R offsets still come from the installed
binary, not the classic reconstruction.

Collision uses integer subtiles and the current body pattern/movement mask;
fractional 16.16 motion and screen projection are separate. Equal-axis traces
visit diagonal destination centres and skip the origin. Controller collision
response can redirect movement around an obstacle. It does not prove the player
traversed a straight interpolation between two observed positions.

## Change

1. **Fit a supported local wall line.** Two neighboring parallel full-body rays
   accompany the existing terrain ray. Their hit line must include the centre
   hit within half a grid diagonal. Every intervening point must be a real
   first-blocking boundary: blocked there, clear one ray step before it. This
   rejects lines drawn through a doorway or the interior of a corner.
2. **Follow the surface, including its offset.** Travel along that verified line
   is removed from boundary displacement, even after a delayed poll or sliding
   around an object. Either observation can supply an unambiguous line; when
   neither can, the existing small-grid-step tolerance remains.
3. **Only real observations advance confirmation.** Removed interpolation of
   hypothetical player origins. It could invent intermediate edges, and its
   clipped sweep also reported an open result before covering the full release
   range. Two actual observations still confirm a change.
4. **Consume adjacent facets once.** Repeated same-kind changes within the
   native body's span are coalesced. A confirmed opposite event immediately
   rearms that direction; it is not delayed by a general cooldown. Current body
   and occupancy checks continue to gate every opening.
5. **Do not replay started chirps.** Cancellation feedback distinguishes a cue
   that never started from one already submitted to the mixer. Only the former
   may retry. New geometric events remain eligible after either outcome.

The normal fit is an interpretation of the collision grid, not a newly
discovered game surface identifier. Half a grid diagonal is a quantization
tolerance; body span comes from the existing native pattern. No collision flags,
game movement, dependencies, direction convention or 120/50 ms sound timing changed.

## Validation and limits

- New `radar-surface` suite: **23 checks**, covering rasterized slopes, range-edge
  travel, a solid corner, collision sliding past an object, adjacent facets,
  started-cue cancellation, rear reopening, and the captured camp obstruction.
- The sliding-past-an-object case failed on the previous implementation, then
  passed after surface tracking. Existing finite-recess tests caught a fit
  spanning a jamb; boundary continuity checks fix it without losing either
  direction of the running return trip at 50/150/200 ms polling.
- **24 suites / 1164 checks pass**, including the UI/controller suites and the
  engine/serialized-audio integration tests. Radar/footsteps: 729 checks.
- Normal build deployed successfully; build and installed DLL SHA-256:
  `5295ECCAFFDB4C964CC6BEEB99BB435B0ED17A5EEEE1DC2554E0A98580B0F40E`.

This is still experimental. The latest session lacks full per-poll world
snapshots, so no measured before/after audible cue-rate reduction is claimed.
The new build needs controller/listening validation. Dense genuinely separate
obstacles can still yield real openings and closures; loaded-room coverage,
actor/terrain ownership ambiguity and the existing chosen D2 query scale remain
limitations. The user controls the next live movement test.
