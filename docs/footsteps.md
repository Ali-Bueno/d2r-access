# Stalled local footsteps

Requested after the user reported that footsteps continued while trying to
move into an obstacle. This feature filters the native HD footfall event when
the local walking/running character repeatedly produces footsteps without any
change in its actual position. It does not use radar clearance as proof of a
collision: the game can slide along a wall, which is still real movement.

## Native evidence: D2R 3.3.93787

Read-only inspection of the running game and its recovered code established:

| RVA | Evidence |
|---|---|
| `0x7D9800` | Animation sound-event dispatcher. Event value 1 calls `0x7D9390` with the ECS entity ID in ECX. Values 2 and 3 use separate functions. After the call, it checks the returned sound handle for null before applying volume. |
| `0x7D9390` | Resolves the entity's unit, constructs the armor-specific `footstep_foley` sound, selects the floor footstep via `0x105510`, and plays it through `0x1A0780`. Returns a null handle if the unit is unavailable. |
| `0x769B50` | Validates the full ECS entity generation against the entity array, obtains its unit component, and returns the unit pointer at component+8. |
| `0x7D96D0` | Reads player animation mode at Unit+0xC; mode 3 selects the running footstep variant. |
| `0x3818E0`, `0x382AA0` | Previously verified native 16.16 position at Path+0, reached through Unit+0x38. The filter compares the entire packed X/Y value, preserving subcell motion. |

Complete function fingerprints, checked before hook installation:

| Function | Bytes | FNV-1a |
|---|---:|---|
| HD footfall `0x7D9390` | 926 | `23C50C43C505B306` |
| Entity resolver `0x769B50` | 90 | `E8700C4429FA18D7` |
| Event caller `0x7D9800` | 297 | `ECB8CBDF6CE9625A` |

All three code ranges matched the running image during this investigation.
The current game's string `footstep_foley` is at RVA `0x1D66E70`. The historical
rebuilt file's `.rdata` is not reliable for this session: at the old inferred
address, live memory contains unrelated renderer text. Current read-only data
was used to find the correct string references; the recovered `.text` matched.

Walk 2, run 3 and town-walk 6 follow the classic
[D2C_PlayerModes declaration](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/include/Units/Player.h).
The D2R footfall function independently confirms run 3. Other animation modes
retain stock sound. This does not assume the classic x86 unit layout: the D2R
mode/path offsets come from the installed native code.

## Behavior and controls

- Enabled on startup when the verified hook can be installed. **Ctrl+F** toggles
  the filter with screen-reader confirmation; `access-footsteps on|off|status`
  provides console control and counters.
- Only the native local player is eligible, using the same verified lookup as
  the radar. NPCs, monsters and other players retain their footsteps.
- The first footfall remains audible. Repeated unchanged positions become
  silent after a 150 ms observation window; any fractional displacement restores
  the next footfall immediately. These timings are design guards, not offsets.
- A one-second break, local entity-generation change, clock discontinuity,
  non-walking animation or unavailable local position clears the baseline. An
  unresolved event from a different full ECS entity ID cannot reset the local
  sequence. Turning the filter
  off restores the normal footfall function.
- The hook skips the floor and equipment-footstep event only. It does not change
  movement, animation, terrain, global mixer volume or other sound categories.
  A sound already playing may finish its short tail.
- `[footsteps]` logs the first local event and audible/stalled transitions, with
  cumulative allowed/suppressed counts, native mode, decision reason, stationary
  time and fractional displacement. Active-event summaries run at most once per
  five seconds; toggle/status/shutdown flush the pending summary. There are no
  per-frame or per-footfall log writes.

## Follow-up: steps still audible in some cases

The user cannot distinguish a full stall from wall sliding by sound alone. The
22:54:32 load installed the hook, but its old transition-only log contained no
suppressed local event. At the last wall snapshot (22:57:28), the radar measured
velocity `(5.1, approximately 0)` subtiles/s: there was still lateral displacement
beside the obstacle. This does not establish the cause of every reported case.

Inspection also found a reproducible policy defect: an unresolved **foreign**
entity event reset the local baseline. Interleaving such events with stationary
local footfalls could keep allowing the first sound indefinitely. The correction
ignores foreign read failures but still fails audible and resets on an unreadable
event belonging to the local full ECS identity. Tests cover both cases and
generation-distinct IDs.

The new summary records counts for `baseline`, `moved`, `settling`, `stalled`,
`animation`, `unavailable`, `unrelated`, and unresolved foreign events. `mode` is
the last native local mode; `travel` and `net` accumulate only comparable samples
accepted by the walking policy, not movement across gaps or excluded animations.
`max_stationary_ms` reports the longest observed unchanged sequence in that
summary. These distinguish actual movement from a mode/read/reset problem without
asking the player to see the character. Other causes remain under investigation;
do not infer a confirmed physical collision from a sound event alone.

## Reproduction at 23:19–23:22: wall sliding confirmed

The user held stick-up while repeatedly requesting Ctrl+T and reported that up
remained blocked. In the 23:19:29 session, both the radar snapshots and footfall
positions show sustained displacement along the wall:

| Local time | Native position X,Y | Result |
|---|---|---|
| 23:21:44.655 | 4497.88356,4592.50232 | Up body clearance 0; velocity approximately (-5.1,0) |
| 23:21:56.970 | 4445.58965,4592.50232 | Up still blocked; X continues decreasing |
| 23:22:04.203 | 4412.81310,4592.50009 | Velocity zero; up/left/right blocked |
| 23:22:06.394 | 4412.81310,4592.50009 | Position remains exactly unchanged |

Decreasing world X with fixed Y projects up-left on screen (the native projection
is documented in `sonar-radar.md`). Together with the user's held-up report, this
establishes wall sliding. It is substantial motion, not fractional jitter. The
radar's straight-up body sweep correctly remained closed throughout.

The session recorded 328 allowed local footfalls: 288 changed-position samples,
19 sequence baselines, and 21 excluded animation-mode samples. There were zero
settling/stalled decisions, local read failures or unresolved foreign events.
The continuous final wall segment was mode 6 with moved decisions, no resets,
and approximately 17–24 subtiles of travel per five-second group. Once the final
position stopped changing, no further local footfalls were recorded; the last
summary flushed the earlier moving events, ending at allowed=328/suppressed=0.

This reproduction validates preserving footsteps while sliding. It does **not**
validate suppression of repeated stationary footfalls, nor establish whether any
of the 21 excluded animation events elsewhere happened during a stall. No audio
capture was made. The foreign-reset correction was not exercised in this run.
An orientation limitation remains: Ctrl+T reports straight directional clearance
without describing the character's actual deflected movement.

## 2026-10-12: treading into a wall

A tester still heard steps against walls. The stall rule mutes only a footfall at the exact same
position for 150 ms, so a walk restarted between retries (a non-walking footfall reset the
baseline) or a creep along a ragged border stayed audible. Now a footfall the game made while
refusing a stick push (the wall-slide hook's attempt count rose since the last local footfall)
is muted at once when it advanced under a quarter subtile from the previous local footfall of
any animation (`Reason::Pushing`, `PushStepMinimum`); a real slide along the wall still sounds.
The `[footsteps] summary` counts it as `pushing=`. Unverified live.

## Verification scope

Thirty-seven pure policy checks cover a sustained stall, immediate subcell resumption, slow
movement, NPC interleaving, action animations, missing data, identity changes,
long gaps and reset. Build and live verification are recorded in `STATUS.md`.
The native hook targets the HD sound event; classic graphics/audio coverage is
not established. It detects stationary walking footfalls, not the identity of
the obstacle. It does not provide a map landmark or route to a destination.


## 2026-09-16: diagonal movement with two straight rays blocked

The latest user report is backed by the 23:52:33/40 Ctrl+T grids: up/right hit
world Y <= 4546, while the player moved along X at Y about 4548.5. Footstep
summaries recorded net +11.008728 X and then -9.091965 X in separate five-second
intervals, with no stalled decisions. This establishes real boundary-parallel
motion, not repeated stationary footsteps. The exact analog stick angle was not
recorded. The filter remains unchanged; Ctrl+T now describes actual recent
screen movement before its straight-ray readings. See [the map analysis and
replayed regression](radar-map-fidelity.md). Live stationary suppression remains
unverified.

## Footstep volume (2026-10-13)

A tester asked for the player's own footsteps louder or quieter than the game's
sounds. `footstep-volume` (Ctrl+S, Sounds) scales them from 25 to 300 percent.
A sound instance keeps its volume at `+0x24` (0-255, 0xFF at creation by
0x1A0780); the event caller 0x7D9800 sets the footfall's after 0x7D9390 returns
(SetSoundVolume 0x19F900, GetSoundVolume 0x19E9D0, both fingerprinted), and the
game's sliders multiply later. So the detour keeps the local player's handle
and the next `FootstepService::Tick` scales it, clamped to 255: a louder step is
possible only while the game's own value leaves room. Only the floor step is
scaled, not the armour's `footstep_foley`. Static RE only; live check:
`[footsteps] volume stock=... percent=... set=... read_back=...` (logged for
the first three steps after a change); if stock is always 255, only lowering works.
