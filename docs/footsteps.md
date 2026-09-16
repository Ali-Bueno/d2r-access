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
- A one-second break, entity-generation change, clock discontinuity, non-walking
  animation or unavailable position clears the baseline. Turning the filter
  off restores the normal footfall function.
- The hook skips the floor and equipment-footstep event only. It does not change
  movement, animation, terrain, global mixer volume or other sound categories.
  A sound already playing may finish its short tail.
- `[footsteps]` logs the first local event and audible/stalled transitions, with
  cumulative allowed/suppressed counts. It does not log every frame or footfall.

## Verification scope

Pure policy tests cover a sustained stall, immediate subcell resumption, slow
movement, NPC interleaving, action animations, missing data, identity changes,
long gaps and reset. Build and live verification are recorded in `STATUS.md`.
The native hook targets the HD sound event; classic graphics/audio coverage is
not established. It detects stationary walking footfalls, not the identity of
the obstacle. It does not provide a map landmark or route to a destination.
