# Remembered wall references

2026-09-16: the user confirms that connected passages improve the radar, but
reports that walking away from a known left wall announces an opening toward
that same wall. This correction adds a world-space reference to each direction;
it does not replace the body graph or turn the radar into a complete map.

## Evidence and changed contract

The latest loaded session contains 322 generated radar events. One left-side
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
the reference. The player's position never moves the remembered plane.

To qualify as a new exit, the structural sweep must show a body span of free
travel beyond that plane, with at least the configured eight-subtile useful
depth. Two current observations confirm this geometry independently of the
already-open proximity state. The original physical body sweep must also clear
that distance. Thus an actor farther than eight subtiles away but still before
a distant doorway prevents announcing its approach as clear.

Retreat leaves the existing opening transition unannounced while the known
wall remains. It invalidates any stale queued opening. Returning to the same
already-reported wall does not produce another closure. Moving along the wall
to a real doorway or past its end can release a new opening normally.

Missing coverage before the old plane cancels exit confirmation but retains
the wall reference. A wall leaving the structural query window does not become
an exit. Engine resets (session/area, teleport, pause or prolonged sampling gap)
still clear references and establish fresh silent baselines. This is four local
wall references, not persistent room/door IDs or a saved exploration map.

Ctrl+T adds `[radar-wall]` records with the known reference and exit evidence.
Generated events include `new_exit` and `exit_depth`; the latter is the required
physical approach depth when evaluating a new exit. Logging remains event-only
or on explicit queries. Spoken physical clearance remains separate from a new
automatic opening.

## Validation

**28 suites / 1435 checks pass**, including 77 new wall-memory checks:

- All four directions warn on approach, then stay silent across two retreats,
  stops and returns, including leaving the structural query window.
- An inferred horizontal-wall fixture reproduces the logged left hits at 4.5
  and 9.0 subtiles; retreat and stopping no longer produce an opening.
- Real doors and wall ends remain detectable after retreat in all directions.
- Unknown coverage, an unfitted wall, interrupted geometric confirmation,
  physical occupancy on a distant approach and session reset are covered.
- Existing running junction, finite opening, queue completion, delivery retry,
  object filtering, body-footprint and UI suites remain green.

The 35-room engine timing sample measured 0.88 ms median and 2.06 ms maximum on
this machine, excluding native snapshot reading. The DLL is compiled and
installed. The inferred event fixture is not a replay of the complete live map;
controller/audio validation of this correction remains pending. Curved or
ambiguous boundaries retain the local surface-estimation limits, and this does
not establish complete navigation accuracy across every D2R level.
