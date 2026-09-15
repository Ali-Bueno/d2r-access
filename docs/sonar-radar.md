# Sonar radar: first prototype and native map evidence

Implemented 2026-09-15 for installed D2R 3.3.93787 / D2RCore 1.2.1-beta.
**Live camp and Blood Moor snapshots work. The user reports improved fixed-axis
navigation but excessive cues. Speed/reaction forecasts and occupancy filtering
are now deployed, pending controller/headphone validation.** See the
[original Kirandur code comparison](kirandur-radar-comparison.md) for the research,
reference-document correction, reaction model and new regression coverage.
The user's `../D2R_SONAR_RADAR_IMPLEMENTATION_SPEC.md` v2 is the design source.
Its referenced `RADAR_ANTISPAM_STABILITY_SPEC.md` was not present in this workspace;
the initial state/movement rules came from v2. **The user explicitly replaced
movement-relative directions with fixed screen/left-stick directions on
2026-09-15**, and requested separation between simultaneous cues.

## Can we read the game's map?

**The active rooms already contain a collision grid we can read.** The native
point collision routine reads a 16-bit cell from the room's own grid. Its room
lookup searches the current room and active neighbors. The prototype copies
these loaded client grids and queries those snapshots; it does not generate a
second procedural map, reveal the automap, load extra rooms or alter collision.

This establishes a local radar source, not proof that an entire act's detailed
collision is resident. An absent room or unreadable cell stays **unknown**. A ray
stops there; a short unknown boundary does not announce either a wall or open
space. A known clear run can establish local passability even if the far end of
the 150-subtile scan is not loaded. Detailed coverage depends on room streaming.
Full-act maps, exits, seed derivation and destination beacons remain separate work.

### What exists beyond an active room?

The installed engine has two related structures: **logical rooms** describe the
level's layout; **active rooms** own live collision grids. The following was
confirmed by static analysis of this build, independently of community offsets:

| Data / lifetime | Native evidence |
|---|---|
| Logical room list | `0x3614C0` starts at `Level+0x10`, follows `RoomEx+0x48`, and compares room rectangles at `RoomEx+0x60/+0x64/+0x68/+0x6C` while building neighbor lists. `RoomEx+0x90` is the containing level. |
| Logical → active room | `0x361290` sets `RoomEx+0x58`; active-room creation `0x2EF340` calls it, then constructs collision with `0x363A90`. The reverse reference is `ActiveRoom+0x18`. |
| Collision allocation | `0x363A90` allocates the grid from the active room's bounds and populates it from tile data. `0x2F0540` stores its pointer at `ActiveRoom+0x38`. |
| Room unloading | `0x2F0110` calls `0x361260`, which clears `RoomEx+0x58`, and `0x363A10`, which frees the grid and clears `ActiveRoom+0x38`. |

Thus logical room rectangles are another possible **direct read**, but a room
rectangle does not describe its internal walls or traversable doorways. The
unload path proves that collision is not permanently attached to every logical
room. It does not rule out other engine caches; no complete, persistent detailed
map has been established. How many logical rooms exist before exploration also
requires a live snapshot. These additional fields are research findings only;
the radar continues to read active collision grids without invoking room creation.

The engine automap is a rendering layer (see [map landmarks](map-landmarks.md)),
not a menu tree of traversable cells. The collision grid is the useful source
for this first radar. No generated-map runtime or extra shipped native dependency
was added. Audio uses the Windows system waveform API.

## Evidence from this installation

RVAs below refer to the decrypted D2R `.text` in `re/D2R.rebuilt.exe`; Ghidra
program `D2R_live.exe` in project `re/ghidra/D2R`. Core RVAs refer to the actual
installed `D2RCore.dll`. No classic Diablo address is used as a D2R address.

| Finding | Source and interpretation |
|---|---|
| Get the local client player | Core SDK `getLocalPlayer` wrapper `0x372AD0`, calls at `0x372C44` and `0x372C4D`: slot `0x5A8CF8` resolves the local index getter; `0x5A8D10` resolves player lookup. The latter is D2R `0x9A480`, accepting indices 0–7. The SDK handle itself is **not** a unit pointer. |
| Path and integer collision position | Unit path `+0x38` (`0x34AE80`). **`0x341A20/0x341A30` use `movzx` to read unsigned 16-bit `Path+0x02/+0x06`**. `0x341A60` passes those same fields directly to collision query `0x364D50`, and `0x22EEB0` uses the getters when preparing the walking query. These are the high words of the fixed-point position at `+0/+4`. The original interpretation of `0x341C10/20` and `Path+8/+0xC` as collision coordinates was incorrect. |
| Level identity | Core exported `GetLevelIdFromUnit`, `0x69A620`: dynamic path room `+0x20`, active room extended context `+0x18`, extended level `+0x90`, level ID `+0x1F8`. Compare against SDK `LevelChanged` when known; before its initial event, use the verified player's positive native level. Compare unit ID `+0x08` against `LocalPlayerReady.playerId`. |
| Active room collision grid | D2R `0x2EFB30`: `return *(room + 0x38)`. |
| Grid indexing | `0x366250`: grid starts with signed 32-bit X/Y/width/height at `+0/+4/+8/+0xC`; cell buffer at `+0x20`. Cell is `uint16_t[(y-originY)*width + x-originX]`. Missing room yields `0x27` before masking. |
| Neighbor rooms | `0x2EFDE0`: neighbor-pointer array at `Room+0`, count at `Room+0x40`. `0x38DFB0` checks the current room then this array. `0x2F0680` checks active-room subtile bounds at `+0x80/+0x84/+0x88/+0x8C`. |
| Actual body footprint | `0x341870` reads `Path+0x5C`; `0x364C70` dispatches pattern 0 to one cell, patterns 1/3/5 to `0x366D40`'s centre plus four adjacent cells, patterns 2/4 to a 3x3 box. The prototype obtains this pattern from the player every snapshot. |
| Native movement-test mask | **Read `Path+0x64` each snapshot.** Wrapper `0x3C5610` passes this field and `Path+0x5C` to movement trace `0x364700`; `0x3AD660` also passes it to `0x364C70`. Setter `0x342740` writes the field. The old fixed `0x1C09` came from a destination query at `0x22F0A3`, not the current player's movement mask. |
| Movement origin | `0x364700` advances to destination cells before checking the body footprint; it does not retest the starting cell. The snapshot scan likewise skips an initial diagonal sample that rounds back to the player cell, so an existing overlap does not prevent leaving an obstacle. |
| Local unit presence | `0x341860` reads the unit's footprint mask from `Path+0x60`. Pattern setter `0x363CF0` additionally stamps `0x1000` at the centre for pattern 1 or on a cross for pattern 2 whenever that mask is nonzero. Patterns 3/4 stamp `0x2000` instead; 0/5 add no such marker. `0x3636D0` removes the same shapes. The added `0x1000` intersects the walking mask and must not make the player block its own radar. |
| Isometric orientation | `0x334DB0`, used by active-room projection `0x2EFC60`: `screenX = 80*(tileX-tileY)-80`, `screenY = 40*(tileX+tileY)+80`. Therefore fixed screen up/left/right/down map to world (-X,-Y)/(-X,+Y)/(+X,-Y)/(+X,+Y), normalized for distance. This directly matches the four stick axes; movement never rotates them. Perceived direction still needs a live headphone check. |

The installed code therefore offers both a native collision query and readable
room grids. The grid path was selected because bounded guarded copies preserve
the distinction between an unloaded region and a real obstruction, and scanning
does not require thousands of native calls. The only invoked native function is
the loader's already resolved client-player lookup, with its code checked first.
Queries and copies run on the UI thread, matching Core's client-player access.
No authoritative/server game state is modified.

`native_world.cpp` verifies complete-code FNV-1a fingerprints for seventeen recovered
accessors/readers before enabling the adapter. It validates the loader-resolved
functions, local identity, level, footprint, dimensions and pointers. All copies
use `NativeMemory`'s memory checks and fault containment. Neighbors are deduplicated
and traversal/copy budgets are bounded; missing neighbors stay unknown.
Pointers are reacquired each poll and are not retained in the radar engine.

Public research was a lead, not build verification:

- [diablo2utils pointer chains](https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/pointer-chains.md)
  and [offsets](https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/d2r-memory-offsets.md).
- [D2MOO collision definitions](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/include/D2Collision.h)
  explain the classic mask/footprint concepts; this implementation was derived
  from the installed D2R instructions, not copied from that project's code.
- [PluginSDK](https://github.com/D2RLoader/PluginSDK) supplies lifecycle, input,
  config and the UI-thread scheduling contract.
- [libd2](https://github.com/jaenster/libd2), [D2RMH](https://github.com/soarqin/D2RMH),
  [d2mapapi_mod](https://github.com/soarqin/d2mapapi_mod) and
  [MapAssist](https://github.com/D2RLegit/MapAssist) were reviewed as alternative
  research paths; none is linked, bundled or needed by this prototype.

## Behavior and module responsibilities

- `native_world`: acquire the local player and copy loaded collision grids.
  Read the runtime movement-test mask and footprint. Remove only the local
  footprint and its derived extra marker from the copied native shapes, where
  its footprint bits are present. Preserve other walls, doors, objects and
  unrelated occupancy. No game memory is changed. Occupancy bitfields do not
  encode separate owners; coincident actor contributions cannot be distinguished.
- `radar_grid`: sweep the **complete body footprint throughout the scanned
  passage**, not only in the first two subtiles. Keep the three point lanes as
  context, but they cannot override an earlier body collision. Sample every
  half subtile, up to 150 subtiles (30 D2 map tiles), stopping at an obstruction
  or unknown coverage. Cell-centred sampling and exact direction components
  preserve symmetry; destination tests skip the starting cell as native movement
  does. Report the body-clear distance, not a thin ray through an unusable gap.
- `radar`: fixed **up/left/right/down** screen directions, available immediately
  on entering gameplay. Walking, turning or reversing never remaps the cues.
  Confirm changes for 100 ms with movement evidence, or 250 ms while stationary
  (so stopping against a wall or opening a door cannot suppress the cue forever).
  A 250 ms per-direction cooldown and distance dead band reject chatter.
  World-boundary comparison filters small changes and retreat from the same wall.
  Session/level changes, teleports and polling gaps silently rebase and invalidate
  pending audio and measured velocity.
- `radar_guidance`: measure recent speed/direction, warn of approaching walls
  using time to contact, and check that a proposed opening remains physically
  usable through a 600 ms reaction allowance plus 680 ms audio budget. Test the
  full body at every half-subtile along continued movement, including the turn
  from each possible reaction point. Narrow fleeting openings remain silent;
  stopping can make them eligible. Separate the actor-filtered terrain estimate
  from physical collision, so moving creatures do not alternate walls/exits.
  Never use filtered terrain alone to announce free passage.
- `radar_synth` / `radar_audio`: synthesize 120 ms PCM cues on a separate worker.
  Opening is a high `tin`; blocking is a low `tock`. Left/right use stereo pan;
  down is centred at 0.86 pitch, distinct from up. **Cues play one at a time,
  with at least 50 ms of silence after each**, including cues from adjacent
  scans. A four-slot round-robin queue retains the latest change per direction.
  Each scan cancels queued cues that no longer satisfy guidance. A started tin
  finishes its short tail while physical clearance remains open, but cancels on
  a current obstruction. There is no long backlog of stale openings. A fresh simultaneous set plays
  up, left, right, down; later sets resume the queue's fair direction order.
  No PRISM call or audio-device wait occurs on the game UI thread. See the
  [Windows waveform API](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutopen).
- `radar_service`: gameplay lifecycle, 20 Hz polling, commands and diagnostics.
  `radar_ui` combines the existing `PanelState` open-panel record with native
  visibility. Open NPC dialogue/narrative, offline pause/settings panels and
  losing application focus silence cues. Merely preloaded panels, inventory and
  automap do not suspend them. Screen-reader wording stays in `speech_strings.h`.

Initialization and unchanged terrain remain silent. Ctrl+T works without first
walking. It describes the current observation, never substituting an old open
state for uncertain clearance. The intermediate distance band is **limited
space**; unavailable coverage is **unknown**, without an invented zero distance.
A near-body obstruction is described as nearby; recognized actor obstruction can
be **paso ocupado**. Other known distances use the complete movement body sweep
and round down for speech. Manual clearance does not include the extra forecast
required for an automatic opening cue.

The minimum tuning values are a two-subtile near-body warning, eight subtiles of
open passage and three subtiles of blocked depth. Guidance increases distances
according to measured speed; a tock can warn before the manual status says
blocked. These and the reaction allowance are named design parameters in
`Radar::Settings`, not recovered engine constants. A cue describes a local
passage or approaching obstruction, not a guaranteed route to a destination.
Continuous proximity tones and full-map guidance remain outside this prototype.

## Controls

| Control | Action |
|---|---|
| Ctrl+R | Toggle radar for this run; announces the switch |
| Ctrl+T | Read up/left/right/down states and body-clear depths |
| `access-radar on` / `off` | Console equivalent of the toggle |
| `access-radar status` | Speak states, or explain that terrain/audio is unavailable |
| `access-radar dump` | Same diagnostics as Ctrl+T: status, local 25x25 grid and sensor details |
| `access-radar test` | Audition opening cues up/left/right/down, then blocking cues in that order |

Shortcuts use the SDK input service and are rebindable in its controls UI. They
do not fire in text-editing or key-binding contexts. The controller remains the
movement input; no controller buttons have been reassigned by the radar.

The DLL embeds `radar.toml`; the loader creates `d2rloader/config/d2r-access.toml`
only when absent. `[radar] enabled = false` disables radar on the next launch.
Existing user configuration is preserved. Ctrl+R does not rewrite the file.

## Current in-game test (controller)

1. Launch D2RLoader with NVDA and stereo headphones. Press **A** to enter the
   existing offline character's camp. Press **Ctrl+T** before walking: it should
   report **Arriba, Izquierda, Derecha, Abajo**, without waiting for a heading.
2. Choose a reported open direction and hold the **left stick in that exact
   direction** briefly, release, then press **Ctrl+T**. Reverse and turn; the
   directions must continue matching the stick, independently of the last move.
3. Approach an obstacle, listen for its tock before contact, release the stick
   and wait briefly. Repeat at your usual faster movement speed; its warning
   should occur farther away. If a direction says open but the matching
   stick push fails, **press Ctrl+T while still at that spot before moving
   elsewhere**: the log now saves the nearby grid and full-body sensor depths.
4. Pass an opening with more than one changing direction. Turn the left stick
   toward a tin; verify that there is time to enter it. Brief niches passed
   quickly should stay quiet. Stop at a small passage: it can now announce once.
   Repeat in Blood Moor near creatures; their movement should not sound like
   repeated new walls and exits. Cues should be
   successive with a short silent gap, not mixed; down has the lower pitch.
   Verify ordinary left/right panning and that unchanged terrain stays silent.
5. Press **A** at an NPC, then **B** to leave; test **Menu/Start**, then **B**.
   Dialogue/offline pause must silence the radar with no burst on return.
6. Once the camp checks pass, test a narrow door, an outdoor boundary and a
   waypoint/portal transition. Those remain unverified live.

Optional console audition: Ctrl+backquote, `access-radar test`, then close the
console. This tests cue identity independently of terrain availability.

Inspect the latest session in `d2rloader/logs/d2r-access.log`:

- `[radar] Verified live room collision reader` establishes code-check success.
- `[radar] Live terrain ready` records actual level, position, room count and
  player footprint. Its absence is a failure to acquire terrain, not an open map.
- `[radar-audio]` records output readiness/failure.
- `[radar] lifecycle=...` records event kind, session, player and level.
- `[radar] status=...` identifies the first blocking condition and reports
  expected/observed player and level, path coordinates, footprint mask, raw
  player-cell flags, native movement mask and the number of copied presence cells cleared. It logs
  only on reason changes or explicit `status`/`dump` requests.
- `[radar] direction=...` records emitted changes (0 up, 1 left, 2 right, 3 down; `axes=screen`), speed, terrain and physical depths, raw blocking flags and warning depth. Emission does not prove completion of audio playback.
- `[radar-state]` reports body and lane probes on Ctrl+T or an explicit dump.
  `[radar-guidance]` adds velocity, required opening/warning distances and forecast
  eligibility. `[radar-map]` saves the local grid with `A` for recognized actor
  obstruction and `#` for other blocks on either Ctrl+T or a console dump;
  neither logs every frame.

## Validation and remaining limits

Thirteen CTest suites / **800 checks** pass: the previous 255 checks, 288
radar/audio checks, 200 reaction/occupancy guidance checks, 45 native-world/config
checks and 12 radar UI checks.
Coverage includes all four fixed stick axes at rest and after turns, symmetric
body and point depths in eight directions, crossroads, narrow shoulder gaps
beyond the old near sweep, leaving an overlapping obstacle, stationary door
changes, unknown terrain, lifecycle rebasing, native mask changes and self
footprint removal. Audio tests verify isolated stereo cues, the 50 ms silent
interval and replacement/cancellation of stale queued changes.
Guidance tests cover actor oscillation, preserved walls/water, acceleration,
turns, early warnings, fleeting versus usable openings, stopping, occupied doors,
intermediate forecast collisions and unknown terrain. A limited corridor can
reveal an exit without first touching its wall. The Kirandur comparison records
the bounded query timing exercise and the remaining prediction limits.

The narrow-gap and dynamic-mask regressions failed before their corrections
and pass afterward. Normal build/deployment succeeded; installed and built DLL
SHA-256 values match. Tests establish these modeled cases, not complete live
navigation accuracy or perceptual suitability.

### First live attempt and follow-up

The 2026-09-15 17:08 session passed the native-code checks and initialized stereo
audio. The player entered Rogue Encampment and reported the combined "waiting for
terrain, or paused" message. No `Live terrain ready` line was recorded. The old
logging did not identify the blocking condition, so it cannot establish whether
UI gating or a native read failed in that session.

The follow-up rejects preloaded-but-unopened panels as pause evidence, allows the
initial level to come from the verified player's room, and adds specific spoken
suspension reasons plus native read-stage diagnostics. Regression fixtures cover
preloaded pause/dialogue roots, opening/closing actual menus, unpaused inventory,
initial level discovery and invalid native levels. These fixes and diagnostics
were installed. The next session isolated the remaining failure below.

### Confirmed position failure and correction

The 17:32 session delivered player ID 1 and level ID 1 correctly. The read failed
at `status=position`, with values such as `-2480,72816`: the adapter was reading
the projected coordinate fields at `Path+8/+0xC`. It had not reached the footprint
or collision-grid reads. This establishes a position-source bug, not missing
terrain or a pause in that attempt.

Following native collision callers established the unsigned word getters
`0x341A20/30` above. The adapter now reads `Path+2/+6` and verifies the complete
five-byte bodies of these getters. The native fixture stores the failed session's
projected values alongside valid collision coordinates; it failed before the fix
and passes afterward. Additional checks cover nonzero fractional words and
coordinates above 32767 to prevent accidental sign extension. Successful live
grid acquisition and audio navigation still require the next in-game test.

### First successful terrain read and self-collision correction

The 17:38 session acquired level 1, position `4473,4628`, 35 loaded rooms and
body pattern 1. Terrain remained available while the player moved. The user
heard some cues, then inconsistent silence and zero-distance reports. Several
blocked events had long clear centre rays (for example depths 16/40/37), which
pointed to the near-body probe rather than loss of the map.

Following the native pattern set/reset routines established the extra presence
marker described above. A nearby body probe overlaps the player's own centre,
and the old code treated its `0x1000` as an obstacle. Diagonal point samples can
also revisit that centre and stop at zero depth. The regression reproduces the
false block with `0x1080` at the player cell; it fails before the correction and
passes afterward. It also checks all four body directions, preservation of the
original memory, walls/doors, no-path outside the player's shape, zero masks and
large-unit corner exclusions. In the next session, `rawCell=1080 removedPresence=1` confirmed the
filter running, and the user reported improved consistency.

### 17:58 direction mismatch and accuracy investigation

The final Ctrl+T at 17:58:02 reported position `4467,4700`, old heading 0
(world +X), open front/back and blocked sides. Back meant world -X, which
projects up/left; the user **pushed the stick straight down**. The mismatch
establishes a confusing reference frame, not proof that the specific -X ray
was wrong. The user explicitly chose fixed stick axes, superseding the original
movement-relative v2 design.

Independent investigation found two additional correctness weaknesses: the
body sweep ended after two subtiles, allowing farther shoulder obstructions to
be outvoted by point rays; and `0x1C09` was a constant from a destination query,
not the dynamic movement-test field. Installed-code analysis recovered
`Path+0x64` through `0x3C5610`, `0x3AD660` and setter `0x342740`. Native movement
trace `0x364700` tests destination footprints and excludes its origin. The
adapter/grid now follows those facts without calling movement or mutating game
state. The subsequent 18:19–18:29 session consistently recorded `movement=1C09`.

### 18:19–18:29 excessive cues and reaction timing

The user reports that the fixed-axis/full-body version is better, but too noisy
to navigate reliably in combat. The log contains 538 emitted changes, including
55 opening-to-blocking pairs under one second. Terrain acquisition continued
through entry into Blood Moor. Investigation, Kirandur corrections, native actor
evidence and the implemented response are recorded in
[kirandur-radar-comparison.md](kirandur-radar-comparison.md).

The latest reaction/occupancy build needs the controller test above. Future actor
motion, animation locks, room streaming, every act's collision flags and perceived
audio latency remain unverified. Native code checks target this installed build;
a different patch requires revalidation.

### Cue delivery diagnostics

Generated events now retain an ID and origin through queueing, replacement,
submission and completion/cancellation. The 20:18 live session verified four
navigation cue chains, including an opening queued behind a blocking cue.
See [radar-event-log.md](radar-event-log.md) for fields, timestamps and limitations.
This adds observability; it does not change navigation thresholds or resolve the
movement discrepancies in [the computer-use check](computer-use-radar-check.md).
