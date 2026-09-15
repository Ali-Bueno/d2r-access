# First computer-use radar check — 2026-09-15

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

**Unresolved collision ownership:** the initial stationary (4463,4604) diagnostic
at 19:18:09 had `rawCell=0080 removedPresence=0`, a blocked cell immediately west
of `@` in the local grid, and a left-body obstruction carrying `flags=1000`.
After movement, sampled player cells were `1080` with `removedPresence=1`.
A mismatch between the reported position and the native presence stamp is a
candidate explanation, not a demonstrated cause. The initial left obstruction
was not tested directly before moving up. Derive the native stamp's actual
position before changing removal rules; never clear arbitrary NO_PATH cells.

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
