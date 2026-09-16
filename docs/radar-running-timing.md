# Running radar: playback-aware reaction forecast

The user confirmed improved collision precision after the fractional-motion and
displaced-presence fixes, but still found orientation difficult while running.
The previous forecast always charged four sounds and gaps (680 ms) before the
600 ms reaction allowance, even when the audio queue was empty.

## Revised budget

Every direction reserves its own 120 ms cue plus a 50 ms scheduling/driver guard.
Each other pending or possible same-poll direction adds a 220 ms slot: 120 ms
sound, the existing 50 ms silence, and its own 50 ms guard. A busy worker reserves
one additional full slot, covering playback, the silent gap and a pending cue
being taken concurrently. Pending replacement of the same direction counts once.
The calculation conservatively counts all other directions regardless of the
fair queue's order. It never assumes a new opening will jump ahead in the queue.

| Audio state | Playback budget | Forecast including reaction |
|---|---:|---:|
| One event, idle queue | 170 ms | 770 ms |
| Two simultaneous events, idle queue | 390 ms | 990 ms |
| Four simultaneous events, idle queue | 830 ms | 1430 ms |
| Four events and active playback | 1050 ms | 1650 ms |

The 600 ms reaction allowance and 50 ms audible gaps are unchanged. A quiet queue
can reveal a running exit previously rejected by the unconditional budget; a
busy queue can require more time than the old estimate. These are bounded design
allowances, not a guarantee of operating-system or speaker latency.

## Same-poll consistency

The engine copies/scans each direction once. Cheap classification at the minimum
and maximum playback budgets finds every direction that could change state.
It reserves their slots before the final full-body forecast and event confirmation.
Thus simultaneous candidates are included before either is committed. Reserving
an ultimately rejected candidate may make a cue conservative, but cannot make
its estimated delivery earlier than the modeled batch.

Pending audio is revalidated every radar poll using the current snapshot. The
existing serialization, cancellation, confirmation, boundary filtering and
600 ms turn-depth checks remain in force. Every intermediate half-subtile in the
continued-motion forecast must still fit the complete body. Unknown coverage
and occupied passages cannot become promised openings.

`[radar-guidance]` and generated `[radar]` events include `playback_ms`; this makes
the forecast's timing visible beside speed, clearance and warning distance.

## Tests and remaining limits

The running regression uses a modest junction at velocity (12,-12) subtiles/s.
It permits a safe turn with two idle-queue cues, rejects the old 680 ms budget,
rejects an actually busy queue, retains intermediate collisions and closes the
window near the far edge. An engine-level trace must announce the exit once
while still moving and reserve both simultaneous sides. Existing speed, turn,
occupancy, narrow-gap and stationary-confirmation tests remain required.

The prediction still assumes continued motion. It does not model the game's
wall sliding, future enemies or later controller inputs. Four local directional
cues cannot identify distant landmarks or reconstruct a winding route. Live
controller timing and navigation validation are recorded separately in `STATUS.md`.
