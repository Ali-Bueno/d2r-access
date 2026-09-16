# Remembered wall references

> Superseded by the 2026-09-16 redesign (docs/radar-redesign.md); retained as history.

The subsequent [contour/sliding correction](radar-sliding.md) follows local
continuous boundaries and captures collision automatically at sounded events.
It supersedes this document's test counts and latest-session description.

2026-09-16: the user confirms that connected passages improve the radar, but
reports that walking away from a known left wall announces an opening toward
that same wall. This correction adds a world-space reference to each direction;
it does not replace the body graph or turn the radar into a complete map.

Follow-up: the user confirms improvement but reports missed first-pass exits,
return-only detection, and wall/opening changes on approach. The current version
refreshes occluded references, rearms confirmed revisits independently of near
warnings, and removes an extra body inflation from beyond-wall proof.

## Evidence and changed contract

The session reviewed for the retreat correction contains 322 generated radar events. One left-side
pair has position `(4819,4770)`, hit `(4815,4774)`, clearance 4.5 at event
`427832640`, followed by position `(4821,4767)`, hit `(4814,4774)`, clearance 9.0
at `427833609`. Both hits lie on the same world row. This supports the reported
retreat problem; the log does not include enough collision data to reconstruct
the entire intervening map.

The old passage state correctly detected more free floor but incorrectly used
the opening sound for it. Some existing tests explicitly required an opening
when retreating from the same wall; those expectations are now changed.
An automatic opening must prove travel past the remembered barrier. Free floor
between the player and that barrier is insufficient.

## Implementation

`radar_wall_memory.cpp` stores a rejected body centre and its fitted supporting
wall normal. These come from the existing structural scan and its neighboring
full-body rays. If no normal is established, the hit's projection on the fixed
direction is the fallback. No new game offset, native call or dependency is used.

The reference follows fresh hits on the same supporting wall, including
tangential hit movement during diagonal retreat. Matching uses the existing
half-cell-diagonal raster tolerance. A newly encountered near barrier replaces
the reference. A nearer first obstruction also replaces it as soon as it is
observed, even outside the proximity-warning distance. The old extrapolated
plane must not hide entrances in a newer wall standing in front of it. The
player's position never moves the remembered plane.

To qualify as a new exit, the structural sweep must show one native destination
step of free travel beyond that plane, with at least the configured eight-subtile useful
depth. The reference and sweep already include the entire body. Adding another
three-cell body span could delay a valid opening until the player moved closer;
the additional step is the axis projection of a native neighboring destination
(one cell per changing world coordinate), not a second footprint inflation.
Two current observations confirm this geometry independently of the
already-open proximity state. The original physical body sweep must also clear
that distance. Thus an actor farther than eight subtiles away but still before
a distant doorway prevents announcing its approach as clear.

Retreat leaves the existing opening transition unannounced while the known
wall remains. It invalidates any stale queued opening. Returning to the same
already-reported wall does not produce another closure. Moving along the wall
to a real doorway or past its end can release a new opening normally.

A confirmed wall return now rearms a previously announced opening even while
current free floor remains beyond the near-warning range. Two wall observations
are required. A one-poll flicker cannot rearm it, and a silent initial open
baseline never counts as an announced exit. This allows the same doorway to
sound on a return trip without requiring a close approach between visits.

Missing coverage before the old plane cancels exit confirmation but retains
the wall reference. A wall leaving the structural query window does not become
an exit. Engine resets (session/area, teleport, pause or prolonged sampling gap)
still clear references and establish fresh silent baselines. This is four local
wall references, not persistent room/door IDs or a saved exploration map.

Ctrl+T adds `[radar-wall]` records with the known reference and exit evidence.
Generated events include `new_exit` and `exit_depth`; the latter is the required
physical approach depth when evaluating a new exit. Logging remains event-only
or on explicit queries. `[radar-opening-hold]` records changes in the reason an opening waits:
physical clearance, remembered-wall evidence, the full approach, or confirmation.
It includes position, the reference, the current hit and required distance. It
is not logged every frame. Spoken physical clearance remains separate from a
new automatic opening.

## Validation

**29 suites / 1510 checks pass**, including the original 77 wall-memory checks:

- All four directions warn on approach, then stay silent across two retreats,
  stops and returns, including leaving the structural query window.
- An inferred horizontal-wall fixture reproduces the logged left hits at 4.5
  and 9.0 subtiles; retreat and stopping no longer produce an opening.
- Real doors and wall ends remain detectable after retreat in all directions.
- Unknown coverage, an unfitted wall, interrupted geometric confirmation,
  physical occupancy on a distant approach and session reset are covered.
- Existing running junction, finite opening, queue completion, delivery retry,
  object filtering, body-footprint and UI suites remain green.

The follow-up adds **75 memory-update checks**. Before correction, the new
fixtures failed in three independent ways: no doorway on either pass when an
older plane hid a nearer wall; one outward opening but none on return while
staying far from the wall; and a valid opening rejected by the extra body span.
After correction, all four screen directions find the nearer wall's doorway
and repeated visits at 50/100/150/200 ms polls. The physical body guards remain
in force. A one-poll wall flicker cannot rearm a cue, while a confirmed returning
wall does so without inventing a proximity closure.

The latest live session before this follow-up has 44 generated/queued/submitted
cues, 41 completed and three interrupted tails. It has no Ctrl+T wall snapshots;
these counters do not reveal every withheld opening or prove the exact reported
route. The new fixtures establish independent logic defects, not a complete
replay of that session. All existing retreat, far-recess silence, running cue
delivery, footprint and UI checks remain green.

The DLL is compiled and installed. Controller/audio validation of this update
remains pending. Curved or ambiguous boundaries retain local surface-estimation
limits, and brief openings can still fall between observations. This does not
establish complete navigation accuracy across every D2R level.
