# Directional movement without wall sliding

Requested 2026-09-22: a blocked direction must stop the character rather than
redirect it along a wall. The radar and footsteps describe actual movement;
silencing them would leave the unexpected travel in place.

## Classic comparison

D2MOO reconstructs classic Diablo II 1.10f; it is corroborating source, not an
offset source for Resurrected. At revision
`5596f5cb6c5251a0a07c6637d26458b06099d516`:

- [PathWF.cpp](https://github.com/ThePhrozenKeep/D2MOO/blob/5596f5cb6c5251a0a07c6637d26458b06099d516/source/D2Common/src/Path/PathWF.cpp#L501-L538)
  implements obstacle following and sliding. Classic click movement was not
  universally a straight-line movement that stopped at walls.
- [Path.cpp](https://github.com/ThePhrozenKeep/D2MOO/blob/5596f5cb6c5251a0a07c6637d26458b06099d516/source/D2Common/src/Path/Path.cpp#L419-L448)
  checks a blocked target when path flag `0x1000` is set, then asks
  `PATH_PreparePathTargetForPathUpdate` to find a replacement. Failure skips
  the path generator and yields zero path points.
- [PathMisc.cpp](https://github.com/ThePhrozenKeep/D2MOO/blob/5596f5cb6c5251a0a07c6637d26458b06099d516/source/D2Common/src/Path/PathMisc.cpp#L229-L261)
  contains that target-adjustment algorithm. Classic walk packets carry a
  destination, whereas D2R has a distinct directional movement path type.

This feature reuses the native failure behavior for direct movement. It does
not claim to reproduce classic mouse pathfinding or replace the movement engine.

## Installed D2R evidence

Derived from decrypted D2R 3.3.93787, then checked with read-only live process
memory on 2026-09-22. The initial classic-helper approach was **inert**: type 17
has default flags `0x16800`, without `0x1000`, and never reaches `0x343920` in
that state. That prototype passed code fingerprints but recorded no prevention;
its hook has been replaced, not combined with the directional hook.

| RVA / field | Evidence |
|---|---|
| `0x3ABD00` | Type-17 dispatch entry, observed in the live table at `0x1D000C0 + 17*8`. Clears path counts and delegates to `0x3AAFE0`. |
| `0x3AAFE0` | Directional solver. Reads desired floats at Path `+0x22C/+0x230`. First probes that direction; then tries a direction rounded to 45 degrees (native constant at `0x1D010B8` is pi/4) and finally eight alternate directions. |
| `0x3AA030` | Native probe: void(context, startXY, directionXY, int distance, outPoint, outTravel, outAlignment, outCollision). Zero distance executes native output initialization and skips traversal. |
| `0x3AB24C` | Return address of the original desired-direction probe; always preserved. |
| `0x3AB2A1`, `0x3AB2F6`, `0x3AB6B4` | Return addresses of snapped-direction retries and the eight-direction fallback. Only these calls are constrained. Zero travel cannot create path points; the solver keeps its normal failure behavior. |
| Context `+0x38`, Path `+0x50` | Dynamic Path pointer and path type. `0x429860` and `0xFB370` explicitly choose type 17 for directional movement. |
| Path `+0x40`, Unit `+0x38` | Owner and backreference. Repeated live snapshots matched the canonical local player. Path `+0x70` is the target unit, **not the owner**; its null/foreign values must not gate this feature. The classic blocked-target helper also reads owner kind through Path `+0x40`. |

`native_wall_slide.cpp` checks six complete native functions before installing
one SDK inline hook at the probe. Its 16-byte prefix ends on an instruction
boundary. The return address is captured in the hook before delegation.

For the local player's type-17 path only, fallback calls reach the **original**
probe with distance zero; every other argument is forwarded unchanged. Native
code initializes its own outputs and performs the normal no-path cleanup. There
are no position writes, collision-mask changes, fabricated routes or patches to
monsters. Other players, mouse routes and other path types retain stock probes.
Unknown/unreadable ownership also retains stock behavior. No animation-mode
filter is used: locomotion mode can be selected after path generation.

## Stairs and narrow passages (2026-10-23)

The tester got stuck again and again on the Arcane Sanctuary's stairways and in the Palace
Cellar's gaps, with wall knocks, in a two-player game (`mp_stuck_*`: 203 knocks in 28 minutes in
the Sanctuary against 1.2 a minute in town; standing still with the stick pushed 13% of the time
there, 2-7% elsewhere). Every knock was this hook refusing one of the game's fallback probes, and
all the movement was the game's own stick move: no route, no auto-walk. The Sanctuary's stairs run
along a world diagonal (screen up and down), 3-4 cells wide in each row with edges stepping a cell
a row; the player's cross body leaves a walking line one or two cells wide, so a stick a few
degrees off the diagonal meets a step almost every cell, and only the game's 45-degree retry
(snapping the direction) walks it. Multiplayer only added a few position corrections (8 jumps in
100 minutes); with the mod, the same stairs trap a player offline too. The other player hosted and
walked by route hops (click pathing, untouched) and refused only against flat walls.

So the two 45-degree retries (`SnappedReturn`, `CenteredReturn`) keep their distance when the
player threads a passage along the push: terrain in `WallBump::Contact` reach on both sides across
it (`WallBump::Squeezes`, eight ways, read on the UI thread with the wall bump and published to the
hook as a bitmask, `WallSlideService::SetSqueezes`). The eight-way fallback (`AlternateReturn`)
stays refused, and so does a retry in the open, along a wall with open floor beyond, or in a
corridor six cells wide: a push into an open wall still stops. Switch `narrow-slide` (Ctrl+S
Navigation, "Slide along stairs and narrow passages", on). `[movement] ... narrow_slides=` counts
the retries let through. Unverified live: walk the Sanctuary's entrance stairs with the stick a
little off their line.

## Controls and validation

Enabled by default. `access-movement on|off|status` controls prevention for the
session and speaks its state. `[movement] enabled = false` changes the startup
default; `[native] enabled = false` prevents hook installation. No per-collision
speech. `[movement] blocked_alternatives=` counts rejected candidate probes,
aggregated at most once per five seconds. Several candidates can be rejected in
one movement attempt; this counter is not proof of a stationary character.
The hook also counts the local player's refused attempts with prevention off,
and passes on the direction of the solver's first probe (return `0x3AB24C`):
the wall bump's evidence (`docs/radar-readings.md`).

Build/deploy completed 2026-09-22 with the game closed. All 61 CTest suites in the
shared workspace pass; the two new suites contain 50 checks. An isolated copy
of the Git change also builds and passes both movement suites and localization.
Build and deployed DLL SHA-256 match:
`43D2471FCA7C6FB277A8B860B84B8D3DE41A65158CFAB330A4295E5D31276E32`.
The corrected hook still needs a live controller test; the earlier session used *Superseded 2026-11-01: testers finished the whole game, Acts I to V, with this in place.*
the inert classic-helper prototype and does not validate this implementation.

Automated checks exercise native ownership (including a foreign target unit),
path-type scope, exact call-site scope, original argument/output forwarding,
toggles, unsupported builds and shutdown. Live validation must compare the same
wall with prevention on/off: approach, hold into it, move parallel, move away,
and repeat diagonally and at a doorway. Check fractional positions, delayed
drift and rubber-banding. Test the controller separately from keyboard
bindings; mouse click routes should keep working.

Strict movement can stop within one collision cell of a position where native
angle snapping would otherwise find a way through. That is a consequence of
refusing a changed direction and needs a doorway/oblique-wall listening check.
An offline authoritative unit may be a different instance from the client local
unit; live validation must rule out delayed correction before claiming all
wall sliding is eliminated. Special movement skills are not reimplemented. *Superseded 2026-11-01: testers finished the whole game, Acts I to V, with this in place.*
