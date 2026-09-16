# Radar cue event log

> Superseded by the 2026-09-16 redesign (docs/radar-redesign.md); retained as history.

Radar cue delivery is recorded automatically in `d2rloader/logs/d2r-access.log`.
No console switch or audio recording is required. Records are emitted for cue
lifecycle changes, not for each 50 ms radar poll. They use the existing bounded
asynchronous logger, keeping SDK disk flushes off the UI and audio threads.

## Follow one cue

Within the latest plugin-load session, join **`event_ms` + direction**. A poll can
generate multiple directions; each has its own queue/playback history. Screen
directions are `up`, `left`, `right`, `down`; states are `open` (tin) and `blocked`
(tock). The old numeric fields remain in generated records for existing searches.

| Stage | Meaning |
|---|---|
| `generated` | The navigation engine confirmed a changed state or retried a cancelled, currently valid cue. `[radar]` includes position, level/session, raw obstruction flags, physical/terrain clearance, velocity, speed and opening/warning thresholds. |
| `queued` | Audio accepted the event into its bounded direction queue. |
| `submitted` | `waveOutWrite` accepted the PCM buffer. This is the software submission time, not a measurement at the speaker. |
| `completed` | The worker observed that Windows returned the buffer without needing a cancellation reset. |
| `dropped` | A queued event was replaced (`superseded`), became invalid (`observation-changed`), or was cleared by pause, lost focus, rebasing, shutdown or an unavailable/failed device. `cancelled-before-submit` means the worker had taken it but had not submitted audio yet. |
| `cancel-requested` | Current playback was invalidated; `reason` records the cause. The worker separately reports its eventual outcome. |
| `interrupted` | Playback was reset before the buffer completed. |
| `failed` | Header preparation or buffer submission failed; `result` contains the Windows multimedia error code. |

`source=navigation` identifies actual radar events. `source=audition` identifies
the synthetic `access-radar test` sequence; those cues start at `queued`, bypass
navigation, and have no measured player origin.

`origin` in audio records is the position **when the event was generated**.
Each queued direction retains its own origin and event ID, even when later
directions arrive or another cue replaces an obsolete one.

## Timing

- `event_ms`: engine tick that generated the event.
- `at_ms`: monotonic Windows time at the audio lifecycle event.
- `elapsed_ms`: time since generation; at `submitted`, this measures software
  processing/queue delay.
- `audio_ms`: worker-observed time from submission to completion/interruption.

These values use `GetTickCount64` and its system-clock resolution. They are not
sample-accurate. The SDK's bracketed wall-clock prefix is written later by the
logging worker, so use the numeric fields for queue/cue intervals. The 120 ms PCM
duration and 50 ms scheduled gap are unchanged; driver completion can arrive later.

## Verification on 2026-09-15

Build/deployment succeeded; installed and built DLL SHA-256 matched:
`884228AADA92E766A897EC02E61BC1C115ED2A9578881CF465C198DCF694A5EB`.
All thirteen CTest suites passed, 811 checks, including eleven additional queue
source checks for serialization, replacement, invalidation, reset and audition.

The 20:18:06 local live session loaded successfully; its front end displayed
D2RLoader **1.3.0-beta**. Short F9 movements in Rogue Encampment produced complete
generated → queued → submitted → completed chains for four navigation cues:

| Event ID / direction | State | Origin | Submission delay | Observed buffer time |
|---|---|---|---|---|
| 379091281 / up | blocked | 4471,4626 | 0 ms | 140 ms |
| 379122953 / right | blocked | 4469,4621 | 0 ms | 125 ms |
| 379123140 / left | blocked | 4469,4620 | 0 ms | 125 ms |
| 379123218 / up | open | 4468,4619 | 94 ms | 125 ms |

The final opening waited behind another direction and preserved its own event
ID and origin. The character remained in camp and was left at the pause menu.
No cancellation/device-error branch was triggered in this live smoke test;
queue invalidation/replacement source handling is covered by automated tests.

## Interpreting silence

The generated-event log already existed before this change. It could not establish
whether an event was subsequently discarded or submitted. These records fill
that gap. Collision queries, thresholds, confirmation rules and cue design are
unchanged by this diagnostic addition.

If no event was generated, use **Ctrl+T at the relevant position** to inspect the
existing physical/guidance snapshots and local collision grid. An unchanged
baseline or filtered transition can intentionally remain silent.

The [state/delivery revision](radar-state-delivery.md) feeds cancellations back
to the engine by original event ID. A later safe retry has a new `event_ms` and
may repeat the same direction/state after an earlier `dropped`/`interrupted`
record. Returning through the same passage also creates fresh state transitions.

The existing logger may discard old pending messages if its bounded queue fills,
or at shutdown. A missing line alone is not proof of missing audio. Software
submission/completion also cannot establish speaker volume, output routing or
perceptual clarity; those remain separate checks if needed.


## Geometry revision (2026-09-16)

Generated events now include `trigger=boundary|clearance` and `hit=X,Y`. Boundary
cues can describe a finite wall receding/returning even while both distances are
physically open; clearance cues include speed-based approach warnings. Ctrl+T's
`[radar-guidance]` lines add `geometry` and `boundary_pending`. Current passage
checks replace the former future-motion opening veto. See
[map fidelity](radar-map-fidelity.md) for the distinction and live limits.
