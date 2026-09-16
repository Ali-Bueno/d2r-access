# Computer-use radar checks — 2026-09-15

## Reproducible setup

Native plugin unchanged from `2ac87e3`; D2R 3.3.93787 / D2RLoader 1.2.1-beta,
offline Barbarian in Rogue Encampment, level 1. User put the game in the foreground
and left the controller idle. Python desktop tools used normal Windows input and
captured the 2560×1440 game client. No game-memory writes were used.

[Tool instructions and bindings](../tools/computer_use/README.md).
Game log session began at **18:59:56 local**. Times below are local (UTC−3);
`actions.jsonl` and PNG names use UTC. The game stayed in camp throughout.

## Observed movement

Each input was followed by a 500 ms wait, Ctrl+T and a screenshot. Coordinates
are native world X/Y, not screen pixels. Directions refer to fixed screen axes.

| Diagnostic time | Input and hold | Before → after | Radar before input |
|---|---|---|---|
| 19:18:26 | Up, 300 ms | (4463,4604) → (4460,4604) | Up blocked, passage 0 |
| 19:18:44 | Left, 350 ms | (4460,4604) → (4458,4606) | Left open, passage 34.5 |
| 19:19:27 | Right, 700 ms | (4458,4606) → (4465,4601) | Right passage 3.5, no usable opening |
| 19:19:47 | Down, 800 ms | (4465,4601) → (4469,4608) | Down passage 6.0, no usable opening |
| 19:20:07 | Right, 300 ms | (4469,4608) → (4469,4606) | Right blocked, passage 0 |
| 19:20:28 | Right + down, 400 ms | (4469,4606) → (4469,4607) | Both directions blocked, passage 0 |
| 19:23:57 | Left (final F13 binding), 350 ms | (4469,4607) → (4467,4609) | Left open, passage 16.0 |

Screenshots and native coordinate changes confirm that all four bound movement
inputs work. The final F13 binding avoids Steam's F12 screenshot action. Menu
clicking, scrolling, Escape and Ctrl+T also worked with NVDA active.

## Findings and confidence

**Observed:** at the rock, an up input moved only world X. At the tent, a right
input moved only world Y while X stayed at 4469. Both began with radar passage 0.
This is consistent with the game sliding the character along an obstruction.
A straight full-body sweep does not describe all movement obtainable by holding
a direction against an edge. Do not label either case as proof that a wall is
passable; the character did not move along the originally requested world vector.
Conversely, do not interpret zero sweep clearance as proof of total immobility.

**Observed:** the opening classification requires at least eight subtiles while
stationary. Several tested positions had 3.5–7.5 subtiles of clearance, yet no
usable opening. This explains silence at those particular stationary positions;
it does not establish that this threshold explains all of the user's report that
the latest radar detects less. These snapshots had zero measured velocity and do
not validate the 1.28-second moving forecast.

**Initial collision-ownership question:** the stationary (4463,4604) diagnostic
at 19:18:09 had `rawCell=0080 removedPresence=0`, a blocked cell immediately west
of `@` in the local grid, and a left-body obstruction carrying `flags=1000`.
After movement, sampled player cells were `1080` with `removedPresence=1`.
A mismatch between the reported position and the native presence stamp is a
candidate explanation, not a demonstrated cause. The initial left obstruction
was not tested directly before moving up. The later controlled reproduction
below establishes a displaced-stamp failure in another position; it does not
retroactively establish ownership of that untested first marker.

## Next investigation

Capture short movement traces around the rock/tent with input direction, actual
displacement, collision ownership, raw sweep and emitted cue state together.
Compare the native directional movement/slide rule with the radar's strict ray.
Evaluate whether short but navigable spaces need separate feedback from a useful
through-passage. Then test moving reaction forecasts and turns at known openings.
Avoid tuning only the sound cooldown or weakening all collision checks.

This session validates the computer-use setup, **not full radar accuracy**.
No audio was captured; emitted log events do not prove that every cue was audible
or distinguishable. Controller analog motion and combat remain untested here.
Final state: character at (4467,4609), camp, pause menu open at 19:24:52.

Evidence is kept locally in `%LOCALAPPDATA%\D2RAccess\computer-use` (PNG and
`actions.jsonl`) and the game's `d2rloader/logs/d2r-access.log`.

## Controlled dinner-session investigation

The user confirmed also playing during the 20:32–20:38 checks. Those mixed-input
movements are excluded from causal claims about input latency or precision.
They then authorized independent testing, saving/exiting and restarting while
away for dinner. The following short keyboard inputs were issued individually,
with screenshots and Ctrl+T snapshots, all inside camp. This does not establish
analog-controller feel or combat behavior.

### Fractional motion

Commit `f0dd74f` replaced integer-only motion measurements with the native 16.16
position. Collision still uses the native integer cells. Complete fingerprints
for the fixed-point constructor/setter increased the reader checks to 19.
The 20:43:49 load succeeded; Ctrl+T recorded fractional positions and navigation
events. Thirteen suites / 818 checks passed at that revision. The detailed
derivation is in [the sonar document](sonar-radar.md#fractional-motion-source).

### False wall from the player's displaced stamp

At 20:47:05, an up input (600 ms) followed an announced opening from
(4470,4620) to (4463,4614). The stopped precise position was
(4463.82536,4614.66194). At 20:47:06 the old adapter generated a **down blocked**
cue, `event_ms=380721140`, passage 0, flags `1000`; right also had zero body
clearance. Diagnostics showed `rawCell=0080 removedPresence=0`.

Read-only memory inspection at the unchanged position found this raw cross:

| Native Y / X | 4463 | 4464 | 4465 |
|---|---|---|---|
| 4613 | 0000 | 0080 | 0000 |
| 4614 | 0080 | 1080 | 0080 |
| 4615 | 0010 | 0090 | 0000 |

Its center was (4464,4614), one cell east of the reported player cell. The
`0010` bit is nonblocking ground; the center's `1000` is the native added
presence marker. At **20:52:12**, a 250 ms down input reached (4466,4617),
precise (4466.93225,4617.81810). Both X and Y increased, matching screen-down
motion rather than a slide along one world axis. This demonstrates the false
block. A regression using that translated shape failed before the fix.

Path+0x10/+0x12 happened to match the cross center in this snapshot, but static
inspection established those are path targets. They were not adopted as a
collision anchor. The fix instead requires one complete overlapping native
player shape before shifting copied-footprint removal. It preserves other
blocking flags and rejects ambiguous, incomplete, unknown and actor-overlapped
shapes. This inference cannot recover ownership from coincident bitfields.

### Installed correction: 20:58:00 load, 21:02–21:07 check

The game was saved/exited and fully closed for the build. Thirteen CTest suites
passed, **836 checks**, including 65 native-world/config checks. Built and
installed DLL SHA-256 matched:
`8002104D4A7C3C5788354302C280F3BD561312575E71F05AE529CB783B1DF1D2`.
The reader verified successfully and acquired 35 loaded rooms in camp.

The initial repeated route (up 800 ms, right 300 ms, up 600 ms) reached
(4464,4614). Short left/up adjustments then exercised a displaced stamp:

| Diagnostic time | Position | Presence / observation | Following input and result |
|---|---|---|---|
| 21:04:46 | (4462,4613), precise (4462.48604,4613.98695) | raw `0080`, removed 1, offset (0,1); down clearance 44.5 | Down 250 ms → (4465,4616), precise (4465.42200,4616.63118) |
| 21:05:37 | (4461,4618) | Actual stone wall: left/down clearance 0, flags `0015` | Down 250 ms → (4462,4618); Y stayed at the wall, consistent with an edge slide |
| 21:06:29 | (4465,4615) | raw `0080`, removed 1, offset (-1,0); down clearance 41.5 | Down opening event `381883671`; down 700 ms → (4472,4622) |
| 21:06:53 | (4472,4622) | raw `0080`, removed 1, offset (-1,0); down clearance 31.5 | Paused in camp at 21:07:12 |

The changed removal activated on two different offsets and stopped counting the
player's marker as an obstruction. The wall's blocking flags and body checks
remained present. The down opening at 21:06:28 was generated, queued, submitted
and completed before the matching movement test.

All **eight** generated navigation events in this load session have complete
delivery chains. Submission delays were 0–16 ms; worker-observed buffer times
were 109–141 ms. One right warning received a cancellation request as its
buffer completed. There were no device failures or missing terminal records.
No audio capture was used; these are software-delivery observations.

### Remaining navigation limits

The fixed 600 ms reaction allowance plus 680 ms audio budget still forecasts
1.28 seconds of continued movement. Earlier controlled movement showed some
side openings becoming eligible only after stopping. The fractional-position
and displaced-stamp corrections do not establish that this forecast is tuned
well for running, turning, shorter passages or combat. The strict straight sweep
also does not simulate the game's edge sliding. Those require distinct checks;
do not remove obstacle checks or shorten timing merely to produce more sounds.
