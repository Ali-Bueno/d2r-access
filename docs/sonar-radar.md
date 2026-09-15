# Sonar radar: first prototype and native map evidence

Implemented 2026-09-15 for installed D2R 3.3.93787 / D2RCore 1.2.1-beta.
**Built and deployed; the first live attempt could not acquire terrain. A startup
and pause-detection correction is installed, awaiting another in-game check.**
The user's `../D2R_SONAR_RADAR_IMPLEMENTATION_SPEC.md` v2 is the design source.
Its referenced `RADAR_ANTISPAM_STABILITY_SPEC.md` was not present in this workspace;
the state, movement and heading rules come from the supplied v2 spec itself.

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
| Path and integer world position | Unit path `+0x38` (`0x34AE80`); native X/Y getters `0x341C10` / `0x341C20` read **32-bit `Path+0x08/+0x0C`**. These are the getters used by `0x34AF60/0x34AFB0`. This prototype does not assume the public `+0x02/+0x06` position fields are authoritative for this build. |
| Level identity | Core exported `GetLevelIdFromUnit`, `0x69A620`: dynamic path room `+0x20`, active room extended context `+0x18`, extended level `+0x90`, level ID `+0x1F8`. Compare against SDK `LevelChanged` when known; before its initial event, use the verified player's positive native level. Compare unit ID `+0x08` against `LocalPlayerReady.playerId`. |
| Active room collision grid | D2R `0x2EFB30`: `return *(room + 0x38)`. |
| Grid indexing | `0x366250`: grid starts with signed 32-bit X/Y/width/height at `+0/+4/+8/+0xC`; cell buffer at `+0x20`. Cell is `uint16_t[(y-originY)*width + x-originX]`. Missing room yields `0x27` before masking. |
| Neighbor rooms | `0x2EFDE0`: neighbor-pointer array at `Room+0`, count at `Room+0x40`. `0x38DFB0` checks the current room then this array. `0x2F0680` checks active-room subtile bounds at `+0x80/+0x84/+0x88/+0x8C`. |
| Actual body footprint | `0x341870` reads `Path+0x5C`; `0x364C70` dispatches pattern 0 to one cell, patterns 1/3/5 to `0x366D40`'s centre plus four adjacent cells, patterns 2/4 to a 3x3 box. The prototype obtains this pattern from the player every snapshot. |
| Player walking mask | Calls at `0x22F0A3` and `0x566A95` pass **`0x1C09`** to the footprint query at `0x364C70`. This includes terrain, object, door and no-path bits; ordinary player/monster/item presence bits are absent. |
| Isometric orientation | `0x334DB0`, used by active-room projection `0x2EFC60`: `screenX = 80*(tileX-tileY)-80`, `screenY = 40*(tileX+tileY)+80`. Therefore +X projects down/right and +Y down/left; left is a negative quarter-turn in world coordinates. Perceived direction still needs a live headphone check. |

The installed code therefore offers both a native collision query and readable
room grids. The grid path was selected because bounded guarded copies preserve
the distinction between an unloaded region and a real obstruction, and scanning
does not require thousands of native calls. The only invoked native function is
the loader's already resolved client-player lookup, with its code checked first.
Queries and copies run on the UI thread, matching Core's client-player access.
No authoritative/server game state is modified.

`native_world.cpp` verifies complete-code FNV-1a fingerprints for ten recovered
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
- `radar_grid`: test the actual body footprint; scan three parallel lanes per
  direction, at half-subtile increments, up to 150 subtiles (30 D2 map tiles).
  Integer cell positions are sampled about their centres, with boundary ties
  resolved toward the scan direction. The eight heading vectors have exact
  cardinal zero components. This prevents reversed rays from reporting unequal
  depths and avoids numerical drift into a neighboring row or column.
- `radar`: movement-relative front/left/right/back; stable state per direction,
  spatial dead band, 100 ms confirmation plus movement evidence and cooldown.
  Heading uses 150 ms of actual displacement to tolerate integer-coordinate
  staircases. A confirmed heading change, session/level change, teleport or
  polling gap rebases silently and invalidates pending audio.
- `radar_synth` / `radar_audio`: synthesize 120 ms PCM cues on a separate worker.
  Opening is a high `tin`; blocking is a low `tock`. Left/right use flat stereo
  pan; rear is centred at 0.86 pitch. Same-scan events share a mixed buffer.
  No PRISM call or audio-device wait occurs on the game UI thread. See the
  [Windows waveform API](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutopen).
- `radar_service`: gameplay lifecycle, 20 Hz polling, commands and diagnostics.
  `radar_ui` combines the existing `PanelState` open-panel record with native
  visibility. Open NPC dialogue/narrative, offline pause/settings panels and
  losing application focus silence cues. Merely preloaded panels, inventory and
  automap do not suspend them. Screen-reader wording stays in `speech_strings.h`.

Initialization is silent. Walk a few steps to acquire a heading. Stable open
fields and unchanged walls are silent. The radar is not a repeated proximity
beeper and gives no navigation speech automatically.

The first tuning values are two subtiles of body sweep, eight subtiles of open
depth and three subtiles of blocked depth. These are named design parameters in
`Radar::Settings`, not recovered engine constants. Boundary-jump enhancement,
proximity tones and full-map guidance are not part of this prototype.

## Controls

| Control | Action |
|---|---|
| Ctrl+R | Toggle radar for this run; announces the switch |
| Ctrl+T | Read front/left/right/back states and known clear depths |
| `access-radar on` / `off` | Console equivalent of the toggle |
| `access-radar status` | Speak states, or explain that terrain/heading/audio is unavailable |
| `access-radar dump` | Speak status and log the local 25x25 grid plus sensor diagnostics |
| `access-radar test` | Audition opening cues front/left/right/back, then blocking cues in that order |

Shortcuts use the SDK input service and are rebindable in its controls UI. They
do not fire in text-editing or key-binding contexts. The controller remains the
movement input; no controller buttons have been reassigned by the radar.

The DLL embeds `radar.toml`; the loader creates `d2rloader/config/d2r-access.toml`
only when absent. `[radar] enabled = false` disables radar on the next launch.
Existing user configuration is preserved. Ctrl+R does not rewrite the file.

## First in-game test (controller)

1. Launch D2RLoader with NVDA and stereo headphones. Select the existing offline
   character and press **A** to enter the Rogue Encampment.
2. Hold the **left stick** in one direction for a few steps, then release it.
   No four-cue startup burst should occur. At rest, unchanged terrain is silent.
3. Move along an existing building boundary with the **left stick**, pass an
   opening and continue beyond it. Listen for one side `tin`, then one `tock`.
4. Reverse or turn the **left stick**. Confirm that changing direction alone
   does not produce four sounds. Check left/right with a known building edge;
   the projection convention still requires this perceptual validation.
5. Approach Charsi, press **A** to interact and use **A** to open a spoken topic.
   Terrain cues should remain silent through dialogue; use **B** to return.
6. Press **Menu/Start** to pause, then **B** to return. There should be no queued
   burst on return. Open inventory through the normal controller menu and close
   it with **B**; the unpaused world should retain radar sensing.
7. After this baseline, test a closed/open door, an outdoor area boundary and
   a waypoint/portal transition. These have not yet been verified live.

Optional keyboard diagnostics: Ctrl+T gives a spoken observation. For an ASCII
sample, open the developer console with Ctrl+backquote, enter `access-radar dump`,
then close the console before movement checks. `access-radar test` auditions
sound identity independently of terrain availability.

Inspect the latest session in `d2rloader/logs/d2r-access.log`:

- `[radar] Verified live room collision reader` establishes code-check success.
- `[radar] Live terrain ready` records actual level, position, room count and
  player footprint. Its absence is a failure to acquire terrain, not an open map.
- `[radar-audio]` records output readiness/failure.
- `[radar] lifecycle=...` records event kind, session, player and level.
- `[radar] status=...` identifies the first blocking condition and reports
  expected/observed player and level, path coordinates and footprint. It logs
  only on reason changes or explicit `status`/`dump` requests.
- `[radar] direction=...` records emitted changes (0 front, 1 left, 2 right, 3 back).
- `[radar-map]` / `[radar-state]` are explicit diagnostics only; no per-frame dump.

## Validation and remaining limits

Twelve CTest suites / 527 checks pass: the previous 255 checks, 235 radar/audio
checks, 25 native-world/config checks and 12 radar UI checks. Synthetic cases cover body-width gaps,
door masks, actor filtering, unknown cells, crossroads, stable walls/open fields,
stationary changes, turns/zig-zag/diagonal staircases, session/level/teleport/gap
rebasing, equal depths around symmetric walls in all eight headings, cardinal
rays staying in their own cell row/column, stereo direction and clipping, and
guarded native-memory fixtures. The symmetry regression failed with the original
floor-based sampling before the centre/boundary and heading-vector correction.
The normal build deployed the DLL to the game's plugins folder.

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
are installed; successful live terrain acquisition remains to be confirmed.

These checks do **not** establish live spatial calibration, perceived latency,
actual room coverage at 150 subtiles, door animations, every act's collision
flags, controller pause behavior or audio-device compatibility in game. Those
are the next validation step. Code or pointer mismatches leave the radar quiet.
Native code checks target this installed build; a different patch needs revalidation.
