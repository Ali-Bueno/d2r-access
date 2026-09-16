# Radar state changes and delivery

The user reported hearing one cue when both sides opened, and missing the wall
cue when returning through a previously visited passage. They requested prompt
state updates, including returning to the same wall.

## Evidence and causes

The event log demonstrates that generation did not guarantee playback:

- Event `386113390`: up and left opened together; up completed, but left was
  dropped as `observation-changed` after 78 ms, before submission.
- Event `386530953`: three openings were queued; up and left were dropped after
  62 ms and down completed.
- In the 23:19 session, event `389914921` generated up/right warnings; up completed
  and right was dropped after 79 ms. Other warnings were interrupted mid-tone.

These records alone do not prove that every cancellation was wrong. Inspection
found two independent problems: the engine committed an emitted state even when
its sound was subsequently discarded, and each poll could charge an already
queued cue for a fresh complete playback wait. The second side could therefore
invalidate its own forecast. Existing separate engine and queue tests did not
exercise them together through playback.

The previous world-boundary filter also deliberately suppressed reopening while
retreating from the same wall, and could retain its warning history on returning.
That behavior conflicts with the user's requested state updates.

## Revised behavior

- Confirm a changed state on two consecutive radar observations, normally 50 ms
  apart. A change between polls takes about 50–100 ms to confirm, plus audio queue
  time. There is no additional travel requirement or stationary 250 ms wait.
- Every confirmed open/blocked transition can sound again, including backtracking
  along the same wall. An opening means usable clearance; it need not be a newly
  discovered door. Unchanged states and one-poll flicker remain silent.
- Preserve the full-body collision, occupancy and continued-motion reaction
  checks. An unsafe opening is still rejected. Missing terrain silently rebases.
- Each queued direction retains its place in the actual round-robin order. Its
  forecast uses remaining playback, the actual remaining silent gap and earlier
  queued directions. New possible directions reserve their own slots. A second
  side is not evaluated as though it had just arrived behind a fresh batch.
- Cancellation reports its original direction, state and event timestamp back
  to the engine. If that exact state becomes valid again, it can retry, with a
  250 ms minimum between attempts. Old failures cannot overwrite a newer event or
  cross a session reset. Opposite state changes never wait for this retry timer.
- Keep a confirmed wall cue valid through the limited-clearance band, so speed
  or queue changes alone do not chop it. Openings and missing coverage invalidate
  it as before. A started opening still stops on a physical obstruction.
- Sounds remain 120 ms long with at least 50 ms silence between them. Both sides
  are serialized; playback is not instantaneous or simultaneous.

## Verification

`radar-delivery-tests` has 34 checks. Its integration trace advances the actual
engine and queue policies at a simulated 20 Hz radar / 10 ms audio cadence,
including sound completion and cancellation, without an audio device. It verifies
both up/down and left/right openings **and closures** on outbound and return trips,
with every tone completing once and no self-invalidated queued side. Other cases
cover repeated approaches to the same wall, actual queue time, late device guards,
an occupied passage, safe retry, obsolete failure IDs and session rebasing.

Existing tests now require prompt stationary door changes and reopening when
retreating from a wall. Actor oscillation, narrow/occupied exits, intermediate
collisions, unknown terrain, acceleration and full-body guards still pass.

Build/deploy and all 19 shared-workspace suites / 996 checks passed. Build and
installed DLL SHA-256 matched:
`A36A81179A0169A57C059C078ABF593E65E06712CB3DC09DC414506B69E51B4C`.
The latest UI work is included in that shared build but was committed separately.
Live controller/perceptual validation of this revision remains pending. Driver
latency, future enemy movement and a later player turn are not guaranteed by a
forecast; stale unsafe openings must still be cancelled.
