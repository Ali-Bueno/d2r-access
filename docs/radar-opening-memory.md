# Keeping announced opening regions

2026-09-16. The user still reports forgotten openings and a wall followed by an
opening while moving. This update retains multiple announced approaches in an
area and groups approaches that demonstrably belong to the same clear space.

## Evidence and limits

The 12:45:21–12:53:52 session contains 306 generated events, 305 submissions,
296 completions and nine interruptions. Eighteen events are remembered returns.
Its 441 collision chunks decode into 279 complete maps, each with four reference
records. These snapshots describe cue-producing ticks, not the whole trajectory
or the player's held stick direction.

Two recorded wall/open pairs have genuinely different collision destinations:

- Down, events `438357406` / `438357625`: `(4548,4668)` to `(4549,4668)`,
  219 ms apart. The later diagonal body sweep clears the end of the barrier at
  native rows 4670–4671; the earlier sweep hits rejected centre `(4549,4669)`.
- Left, events `438480625` / `438480750`: `(4663,4593)` to `(4663,4594)`,
  125 ms apart. The later sweep passes the lower corner of the obstacle; the
  earlier sweep hits `(4662,4594)`.

Consequently, a wall/open sequence alone is not proof of false collision or
proof of native sliding. Automatically suppressing every such sequence would
hide real usable corners. This update does not change native movement or the
wall/contour detection rules. The no-slide investigation and remaining geometry
limits are in [radar-sliding.md](radar-sliding.md).

An independent reproducible memory failure uses the existing captured Blood
Moor fixture. Announce the upward turn at `(4784,4582)`, leave to `(4788,4586)`,
then return across a 500 ms sampling gap. The previous engine produces one
initial cue and zero return cues. The corrected engine produces one on each
visit, with fresh clearance confirmation on return. This controlled route is
not claimed to be the exact user trajectory.

## Behavior

`OpeningMemory` retains multiple generated opening witnesses per direction in
the current session and area. Discovering another opening no longer overwrites
the preceding one. Same-footprint observations retain their original anchor.

When another approach is announced, `OpeningVisit::Join` checks the native body
destinations throughout the strip connecting both approaches and their proved
targets. Only a fully known, clear structural strip can join them. A wall or
missing cells between two approaches keeps them separate; reachability by a
detour is insufficient. This prevents a wide opening's second edge from
immediately recalling the first edge as another exit.

Each joined region retains the segments that were actually proved. Departure
and re-entry use distance to those segments, with the existing native body-span
hysteresis. A moved native cell losing the approach still permits a narrow
mouth return. That movement is measured from the last reported visit's actual
position, so stationary geometry fluctuations inside a wide region do not
create a new visit. Elapsed time alone never repeats a cue.

Every reminder still requires current structural and original physical body
clearance to the remembered/interpolated approach target. Occupancy, missing
coverage or blocked forward travel can invalidate it. A pending older cue does
not gain validity from an unrelated ready opening.

Pauses, temporary unavailable readings, polling gaps and discontinuous movement
discard live observations and pending audio but retain witnesses in the same
area. Confirmation must restart. Session/area changes and explicit resets clear
them. Resume at an unchanged visit remains silent. This is local approach
memory, not a complete saved map or a route planner.

`[radar-reference] visits=N` records the number of remembered regions in that
direction. Existing `revisit`, visit/target, wall and collision records remain.

## Validation

The revisit suite covers captured geometry in all four directions, polling
cadences, short returns, older approaches after newer ones, pause/gap recovery,
separate and joined openings, stationary changes inside a joined region,
occupancy, unknown coverage, area reset and cue delivery. Existing running
passage, wall/contour, paired-audio and ground-item tests remain part of the
shared build. Final counts and installed artifact are recorded in `STATUS.md`.

Controller listening validation remains user-led. Do not mix desktop input with
the user's movement or equate software audio submission with audible accuracy.
