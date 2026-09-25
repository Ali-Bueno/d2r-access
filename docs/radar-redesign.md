# Radar redesign (2026-09-16): confirm-by-travel state machine per direction

Decided after a deep review of the four-direction/geometry-forecast radar
(`docs/sonar-radar.md` and its follow-ups below). **Three review items were
tried live and reverted at the user's request after listening**: eight
directions (item 1), speed-scaled warning depth (item 5), and a new sound
design (item 6). The kept part of the redesign is the classifier/delivery
engine — installed; live listening validation of that part is the next step
(see checklist).

The user's rule, verbatim, is the spec this engine follows: *"si hay pared
suena pared, si hay salida suena salida; solo cambios; da igual la
velocidad; da igual si vuelvo al mismo sitio."*

## The model

1. **Four directions, unchanged.** (Since 2026-09-25 the rays follow the stick
   diagonals by default, `docs/radar-readings.md`; the cardinals below stay
   selectable.) Screen cardinals up/left/right/down —
   the user's explicit choice after listening to both the 4- and 8-direction
   builds: four sounds are enough information, and the review's 8-direction
   attempt (screen cardinals + diagonals sensing the world axes) was tried
   and reverted. Native projection `0x334DB0` (screen X = 80·(worldX−worldY),
   screen Y = 40·(worldX+worldY)) still means each screen cardinal is a
   **world diagonal**, so these four rays still meet D2's axis-aligned walls
   at 45 degrees. That grazing geometry is therefore **not eliminated** by
   this redesign — it is absorbed by item 3 (confirmation by travel), which
   does not need a clean ray/wall angle to work.
2. **One state machine per direction** (`Sensor` in `src/radar.h`) replaces
   `PassageTracker`, `OpeningMemory`/`OpeningVisit`, `Guidance` and
   `BoundaryTracker`, and their retry logic. `WallMemory`'s purpose — a wall
   the player was warned about should not be reported as an opening just
   because they backed away from it — is not dropped: it survives as the
   `Frontier` rule in item 4 below, scoped to one remembered wall per
   direction instead of a standing memory structure. The only classifier is
   the connected-passage BFS (`ScanPassage`, `src/radar_passage.cpp`) over
   the structural grid built by `StructuralWorld` (terrain layer with
   bypassable clutter removed — `src/radar_structure.cpp`). The route search
   and the clutter filter were both narrowed after the 2026-09-16 18:10 live
   analysis (see that section below) — the BFS keeps only one native cell of
   lateral tolerance, and the filter caps removable clutter at one map tile.
   Every `Reset()` (session/level change, teleport, poll gap) starts a silent
   baseline: the first observation after a reset never sounds.
3. **Confirmation by travel, not time.** A changed verdict must persist
   while the player moves `confirmDistance` (one native cell, measured with
   the 16.16 subtile fraction) before it is confirmed; a stationary player
   confirms after two polls (100 ms). Still polled with `GetTickCount64` at
   `PollMs`, but elapsed time no longer decides the verdict by itself. This
   is what actually absorbs the 45-degree raster steps against the world
   axes now that the 8-direction attempt has been reverted.
4. **World-anchored wall memory (`Frontier`, `src/radar.h`), replacing the
   "repeat" hold.** The first cut of this engine held back an opening cue
   only when it repeated the sensor's last confirmed cue. The user found
   this wrong in the field: walking into a wall sounds the wall cue;
   backing away makes the proved route ahead reach `openDepth` again, but
   nothing actually opened — the old engine reused the "open" cue there
   regardless, which is exactly the case the original `WallMemory` existed
   for. When a wall is confirmed, its `Sensor` now stores a `Frontier`: the
   wall's position projected onto the direction axis (`player projection +
   proved route depth`), and where the player stood when it was announced.
   A confirmed opening is then held (hold reason `"retreat"`) while the
   player is within `boundaryRange` (20 cells) laterally of that spot *and*
   the proved route does not reach past the stored wall by at least half
   the hysteresis band, `(openDepth − closeDepth)/2` (1.5 cells at the
   default depths). A doorway beside the wall, or its far end, proves reach
   past that margin and sounds; retreating straight back never does;
   approaching the same wall again still sounds the wall cue (it is a
   proximity warning, not memory-gated). Once the player leaves the
   frontier's lateral neighbourhood the gate no longer applies. A confirmed
   opening also waits until `cueIntervalMs` since the last cue in that
   direction before it can sound (hold reason `"interval"`); **since
   2026-09-25 only since the last opening**, and a passable change equal to
   the last cue is dropped (hold `"unchanged"`), `docs/radar-readings.md`;
   wall cues are never delayed. There are no retries: a queued cue that no longer matches
   the confirmed state is dropped (`RadarAudio::Retain`), and a cue already
   sounding is not cut by a new observation — only by suspension
   (dialogue/pause/focus loss).
5. **Fixed thresholds: back to no speed scaling.** A first cut of this
   engine grew the closing depth with measured speed (`reaction =
   blockedDepth + confirmDistance + speed·(PollMs + CueDurationMs +
   CueGapMs)/1000`), on top of a standing distance. The user rejected this:
   walking fast or slow must make no difference to the verdict — only real
   collision changes should sound, never how quickly the player got there.
   Thresholds are fixed again: `open = openDepth`; `close =
   max(blockedDepth, openDepth − bodySpan)` (`Engine::CloseDepth`, now takes
   no speed). `openDepth` was raised to 10 cells by the 2026-09-16 18:10 live
   analysis (see that section below), so `close` is 7 for the three-cell
   bodies, 9 for the point body. `Engine::OpenDepth` no longer exists.
   Velocity is still measured (`UpdateMotion`), but only feeds the Ctrl+T
   "moving towards" readout, not any threshold.
6. **Audio: back to the previous sound design** (`src/radar_synth.cpp`).
   The review's new sound design (80 ms cue, pitch glide, ±0.64 diagonal
   pan) was tried and reverted after listening. Current cues: 120 ms
   duration, 50 ms gap; the old tin/tock registers (open 1100 Hz bright,
   blocked 260 Hz dull); Down is the same tone 14% lower (`DownPitch` 0.86)
   instead of a pitch glide; stereo pan is fixed left/right ±0.9 (only two
   of the four directions pan; up/down are centred); peak level
   0.15 × volume. `volume = 0..100` remains a key under `[radar]` in
   `radar.toml` (`src/radar_config.h`). The output device still reopens
   automatically every 2 s after a failure (`ReopenDelay`,
   `src/radar_audio.cpp`) instead of disabling the radar for the rest of
   the session. `access-radar test` auditions 8 chirps: up, left, right,
   down opening, then the same order blocked.
7. **Diagnostics**: collision snapshots are rate-limited to one per second.
   New lines `[radar-direction]` (inside snapshots) and `[radar-state]`
   (Ctrl+T / `access-radar dump`) report state/candidate/depth/open/close/
   available/deferred/last_cue/hold per direction, each now ending with
   `frontier=<known>,<projection>` (whether a wall is remembered for that
   direction, and its stored axis projection). The `[radar]` generated line
   carries depth/open/close/speed and `direction_name`/`state_name`
   (`direction_name` ∈ {up, left, right, down} — unchanged by the revert),
   and now ends with `stick=<x>,<y>` (the left thumbstick, screen
   convention, `InputDirection::LeftStick`) so a future log shows where the
   player was steering. The old `[radar-opening-hold]`, `[radar-wall]`,
   `[radar-guidance]` and `[radar-reference]` lines are gone with the
   classes that wrote them.
8. **Explicitly out of scope**: item 7 of the review (a continuous/
   on-demand scan mode). Not implemented, not planned in this pass.

## Parameters (`Radar::Settings`, `src/radar.h` unless noted)

| Value | Origin |
|---|---|
| 4 directions (Up/Left/Right/Down), `Axes`/`ScreenAxes` | Native projection `0x334DB0`; kept as-is after the 8-direction attempt was reverted |
| `PollMs` = 50 | Existing poll cadence, unchanged |
| `CueDurationMs` = 120, `CueGapMs` = 50 | Restored to the pre-redesign cue length after the shorter 80 ms cue was reverted |
| `confirmDistance` = 1 | One native collision cell |
| `stationaryConfirmMs` = 2×`PollMs` = 100 | Two polls, for a stationary player |
| `cueIntervalMs` = 500 | Chosen anti-spam interval for repeated openings |
| `blockedDepth` = 3, `openDepth` = 10 | Fixed thresholds, no speed term: `open = openDepth`; `close = max(blockedDepth, openDepth − bodySpan)` = 7 cells (three-cell bodies) or 9 (point body). Speed-scaling was tried and reverted — the user requires the same verdict at any speed. `openDepth` raised from 8 to 10 (two map tiles) by the 18:10 live analysis |
| lateral tolerance = 1 native cell (`radar_passage.cpp`) | 18:10 live analysis: the prior 3-cell (one body span) lateral slack let the BFS count routes beside the player as "ahead", inflating depth against 45-degree walls until contact. Reduced to the unavoidable 0.707 staircase wobble of a diagonal line — the first step may be the diagonal or either axis component, matching native movement |
| clutter object span = 5 cells (`SubtilesPerTile`, `radar_structure.cpp`) | 18:10 live analysis: props up to 20 cells wide were being erased from the ray. Capped at one native map tile (5 subtiles) for both `WALL` and `OBJECT` flags |
| `reachDepth` = 20 | New. Origin: the existing `boundaryRange` (structural neighbourhood). The passage scan proves the route out to this distance (not just `openDepth`) so a doorway past a remembered wall is measurable; unknown cells beyond `openDepth` now only cap the reach instead of invalidating the verdict |
| retreat margin = `(openDepth − closeDepth)/2` (1.5 cells at default depths) | Derived, not a separate constant: half the hysteresis band between open and close, so the `Frontier` check absorbs the same raster/confirmation slack as the rest of the engine |
| `boundaryRange` = 20 | Reused (was structural-neighbourhood only) as the lateral radius within which a remembered `Frontier` still gates opening cues |
| `teleportDistance`, `maxGapMs`, `motionWindowMs`, `stoppedMs`, `recentMotionMs`, `farRange` | Unchanged from the prior engine (session/rebase and velocity smoothing) |
| `PanWidth` = 0.9, `OpenHz`/`BlockedHz` = 1100/260, `DownPitch` = 0.86, `Level` = 0.15 | `src/radar_synth.cpp`: restored pre-redesign sound design (fixed left/right pan, tin/tock registers, Down 14% lower, listening-validated level) |
| `volume` 0-100 | `radar.toml` `[radar]` key, `src/radar_config.h` |
| `ReopenDelay` = 2 s | `src/radar_audio.cpp`, output-device retry interval |

## Log format (current)

| Prefix | When | Content |
|---|---|---|
| `[radar]` | lifecycle, status, generated cue | `direction_name`, `state_name`, position, depth/open/close/speed, session/level/velocity, `stick=<x>,<y>` (left thumbstick), `stage=generated` |
| `[radar-direction]` | inside a rate-limited snapshot | per-direction state/candidate/depth/open/close/available/deferred/last_cue/hold, ending with `frontier=<known>,<projection>` |
| `[radar-state]` | Ctrl+T / `access-radar dump` | same fields plus `candidate_age`/`last_cue_age`, also ending with `frontier=<known>,<projection>` |
| `[radar-motion]` | every snapshot/dump | velocity, speed, precise position |
| `[radar-map]` | Ctrl+T / dump | local ASCII grid (`#` blocked, `A` occupied/actor, `.` open, `?` unknown, `@` player) |
| `[radar-collision]` | rate-limited (1/s) | run-length-encoded raw flags for replay |
| `[radar-audio]` | queued/submitted/completed/cancelled/dropped | stage, reason, timings, source |
| removed | — | `[radar-opening-hold]`, `[radar-wall]`, `[radar-guidance]`, `[radar-reference]` |

## What was tried, and what was kept or reverted

| Change | Outcome |
|---|---|
| Item 1: eight directions (screen cardinals + diagonals sensing the world axes) | **Reverted.** The user listened to both builds and asked to keep four directions; the 45-degree ray/wall angle this was meant to fix is instead absorbed by confirm-by-travel (item 3 above) |
| Item 5: speed-scaled warning/closing depth | **Reverted.** The user's rule (see spec line above) requires the same verdict whether walking or running; thresholds are fixed again (`open = openDepth`, `close = max(blockedDepth, openDepth − bodySpan)`), `Engine::OpenDepth` is gone, and `Engine::CloseDepth` takes no speed. Velocity is still measured for the Ctrl+T "moving towards" readout only |
| `PassageTracker`, `OpeningMemory`/`OpeningVisit` | **Kept removed.** Addressed forgotten/duplicated openings across approaches (`radar-opening-memory.md`, `radar-revisits.md`) — replaced by one confirmed `Sensor` state per direction, no memory of past positions needed |
| `WallMemory` | **Purpose survives, structure doesn't.** Its job — a warned-about wall must not be reported open just by backing away — is exactly what `Frontier` (item 4) does now, scoped to one remembered wall per direction (`boundaryRange` neighbourhood, half-hysteresis margin) instead of a standing memory structure (`radar-wall-memory.md`) |
| `Guidance`, `radar_guidance.cpp` (reaction/queue forecast) | **Kept removed.** Addressed cue spam driven by a continued-motion forecast and queue-timing charges (`radar-running-timing.md`, `radar-state-delivery.md`, `kirandur-radar-comparison.md`) — replaced by confirm-by-travel + cue-interval gating |
| `BoundaryTracker`, `radar_geometry.cpp` (surface-fit/facet classifier) | **Kept removed.** Addressed unchanged-cell open/closed flip-flops (`radar-precision.md`, `radar-surface-tracking.md`) — replaced by `ScanPassage`'s BFS plus confirm-by-travel, without needing eight directions |
| Ray scans `SweepBody`/`Scan`/`Probe` | **Kept removed.** Addressed ray-hit classification missing connected passages / conflating bypassable clutter with walls (`radar-connected-passages.md`, `CurrentRadarAnalysis.md`) — replaced by `ScanPassage`'s BFS over the structural grid |
| Retry logic (`radar-event-log.md` retry/cancellation churn) | **Kept removed.** Retried cues racing confirmed state — replaced by drop-on-mismatch (`RadarAudio::Retain`), no retries |
| Item 6: new sound design (80 ms cue, pitch glide, diagonal pan) | **Reverted.** Restored the 120 ms tin/tock, fixed pan and level (see Parameters) after the user found the new design worse |
| `radar_structure.cpp` (object filtering) | **Kept, narrowed 2026-09-16 18:10.** Still the only classifier's input (`ScanPassage`), but the removable-clutter span is now capped at 5 cells (was effectively unbounded — up to 20-cell props were erased); `docs/radar-object-filtering.md` describes the pre-cap behavior |
| Route-search lateral tolerance (`ScanPassage`) | **Narrowed 2026-09-16 18:10.** 3 cells (one body span) of slack let side routes count as "ahead", hiding walls until contact (76% of that session's wall cues fired at depth 0); reduced to 1 native cell, the unavoidable diagonal staircase wobble |

## Live analysis (2026-09-16 18:10)

Forensic review of the 18:10–18:24 session: 377 cues, 207 collision snapshots
independently re-derived with zero discrepancies — the imprecision was in the
model, not the code. Findings:

- **76% of wall cues** fired at depth 0 (on contact): the BFS's 3-cell
  lateral tolerance (one body span) let it count routes beside the player as
  "ahead" against 45-degree walls, so proved depth stayed near `openDepth`
  until the very last cell (4.9 → 0 in one step) instead of degrading early.
- **22% of wall cues** were walls beside the player that a forced diagonal
  first step hit, while the game itself let the player slide past and keep
  moving.
- **3 wrongly-fired "exit" cues** came from the clutter filter deleting
  props up to 20 cells wide sitting directly on the ray.

Changes installed:

1. `ScanPassage` now measures the straight route: lateral tolerance cut from
   3 cells to 1 native cell (the diagonal's own 0.707 staircase wobble); the
   first step may be the diagonal or either axis component, matching native
   movement.
2. `openDepth` raised to 10 cells (two map tiles); closing depth
   `openDepth − bodySpan` is now 7 for three-cell bodies (was 5).
3. The clutter filter (`radar_structure.cpp`) removes components up to one
   map tile (5 cells) only, for both `WALL` and `OBJECT` flags — not 20.

Also new: every `[radar]` generated line ends with `stick=<x>,<y>` (left
thumbstick, screen convention) so future logs show what the player was
steering toward.

## Live analysis (2026-09-20)

1. **Clutter must stand alone** (`radar_structure.cpp`): besides the proved detour, a removable
   component needs no other blocking cell within one body reach (`bodyRadius+1`) of its bounding box —
   a long way round inside the window is not a bypass. Rogue Encampment alcove walls were erased, so
   screen-down read `open 33.9` where the game refused three inputs (`tests/recorded_alcove_grid.h`).
2. **The silent baseline covers the dead band** (`radar.cpp`): it is taken on the first observation
   after a reset, `Unknown` included, so a wall met from the dead band (up 7.8 -> 0.0) cues normally.

## Validation checklist (not done yet)

1. `access-radar test` first, to learn the 8 chirps (4 directions × open/blocked).
2. Camp + Blood Moor walking with NVDA and the log open.
3. Check cue rate per minute (spam regression).
4. Check for same-direction open/blocked flips under 1 second.
5. Check `hold` reasons in `[radar-state]` correlate with what was actually
   heard (or correctly not heard) — including `retreat`: back away from a
   wall and confirm no opening cue sounds until a real doorway or the
   wall's end is reached, per `frontier=` in the log.

## Tests

Radar test counts: seven suites / 1480 checks (`radar-passage` 962, `radar-engine`
257, `radar-structure` 117, `radar-snapshot` 59, `radar-readout` 38,
`radar-audio` 35, `radar-ui` 12); whole tree 45 suites, all green.
