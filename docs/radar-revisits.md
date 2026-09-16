# Remembering short return visits

2026-09-16. The user confirms improvement but reports that an opening does not
always sound again after moving a little away and returning. This update adds
a remembered visit to the existing wall/contour logic.

## Recorded evidence

The 11:54:15–12:04:03 session contains 328 generated/queued events, 327 submitted,
308 completed, 19 interrupted and one dropped before submission. Its 438
collision chunks decode into **302 complete snapshots**, each with all four wall
references. No snapshot is missing a chunk. These are event snapshots, not an
every-poll record; the exact reported return route cannot be identified from
these counters alone.

The raw Blood Moor snapshot at event **435716875**, position `(4784,4582)`, is
preserved in `tests/recorded_return_grid.h`. It contains an 87 by 87 grid at
`(4741,4539)`, native body pattern 1 and movement mask `0x1C09`.

On this captured map, a body-valid route from `(4781,4583)` through
`(4782,4583)` and `(4783,4582)` to `(4784,4582)` reproduces an upward opening.
Walking to `(4788,4586)` and returning leaves that direction physically open
throughout. Before correction it produces one outward announcement and none
on return. After correction it produces one on each visit. This is a new
controlled route on an exact captured map, not a replay of all native movement.

## Changed behavior

`OpeningVisit` stores the **last announced turn per direction**, its position
and the end of the approach proved when it sounded. It only records a generated
opening; an initially open silent baseline cannot create a reminder.

Departure normally requires moving one native body span away. Re-entry occurs
within half that span, with two fresh clear observations. A changed native cell
that loses the structural approach can also establish departure from a narrow
mouth. Fractional motion, a wobble inside the open footprint, stationary wall
flicker, or elapsed time alone cannot cause a repeat.

On return, the current structural and original physical body sweeps must still
clear the remembered approach, with the ordinary connected-passage/alignment
guards. Seeing a back wall in between does not erase that turn. Actors, newly
closed geometry and missing approach coverage can prevent the reminder. A
temporary observation gap can be recovered from fresh real geometry; engine
resets for session/area, pause, teleport or prolonged polling gaps clear visits.

The return uses the existing opening sound and audio queue. Consuming a visit
does not invalidate its queued cue; normal cancellation and failed-submission
retry still apply. Generated events include `revisit=1` for these reminders.
`[radar-reference]` adds the visit position, target and departure state.

This is four recent turn memories, not a catalogue of all explored doors. A
newly announced turn replaces the previous one for that direction. New openings
continue to use the wall/contour rules; their known limitations remain in
`radar-sliding.md`. Native wall sliding is still active.

## Validation

**32 suites / 2022 checks pass.** The new 186-check suite covers:

- The recorded map and its four rotated directions at 50/100/150/200 ms polling,
  three repeated short trips, and silence while leaving or standing still.
- Small positional/fractional wobbles, silent baselines and session resets.
- Remembered visits surviving a new back-wall reference or brief missing data,
  while requiring fresh real clearance after coverage returns.
- Actors beyond the initial eight-subtile range, narrow-mouth departure,
  stationary collision flicker, queued-cue cancellation and delivery retry.

All earlier retreat, contour, doorway/revisit, audio and UI regressions pass.
The new suite also passes after removing a fixture-only compiler conversion
warning. Build and installed DLLs match SHA-256
`BE1A3B6293D42AB22E67D3908F37E12039EF9671ED4AB03112792ABA018E82E9`.
User-led controller/listening validation of this revision remains pending.
