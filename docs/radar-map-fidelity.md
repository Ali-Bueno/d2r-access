# Native map, Kirandur geometry and diagonal movement

> The later [precision correction](radar-precision.md) supersedes speed-driven
> cues and adds grid/range stability and thin-gap guards. This document records
> the earlier investigation and build.

Investigated and installed 2026-09-16 after the user reported missed exits and
footsteps while Ctrl+T said up/right were blocked. This supersedes the **opening
forecast** in the earlier reaction/timing documents. Live listening validation
of this revision remains pending.

## Findings from the player's last session

The 23:46–23:52 session generated 217 direction events; 206 buffers completed,
six were interrupted and five were dropped. Delivery is distinct from detecting
an exit: neither a generated event nor a completed buffer proves that navigation
is correct or that the sound was perceptually clear.

The two last Ctrl+T snapshots were at 23:52:33 and 23:52:40, Blood Moor:

- Positions `(4809.67870,4548.58472)` and `(4815.41591,4548.50230)`.
- Both straight up and straight right hit terrain flags `0x0005` immediately.
- The saved 25x25 grid has a wall across all rows at world Y <= 4546, a clear
  row at 4547 and the player on 4548. World X can change without entering that
  wall. The second grid also contains a corner farther along +X.
- The footstep summary at 23:52:37 records 17 moved events, net
  `(11.008728,-0.092255)` subtiles. At 23:52:42 the player had moved back
  `(-9.091965,-0.000259)`. Neither interval contains a stalled-footstep decision.

These are real displacements along the boundary. Holding a diagonal stick does
not imply moving along both of its straight cardinal rays: collision response
can redirect motion along the wall. We did not record analog input, so the log
does not establish the exact stick angle. It does establish that silencing all
these footsteps would hide actual travel. No footstep-filter logic changed here.

## What the map actually supplies

Fresh read-only Ghidra decompilation of the installed, previously dumped native
image rechecked `0x364700`, `0x364C70`, `0x366D40`, `0x3C5610`, `0x22EEB0` and
`0x334DB0`. No new offsets, hooks or runtime native calls were introduced.

| Layer | Native evidence and consequence |
|---|---|
| Map tiles | Collision is subdivided into five subtiles per tile; flags are 16-bit values, not a screenshot or an automap outline. |
| Resident rooms | `ActiveRoom+0x38` owns its grid; origin, size and cell pointer were already verified at `0x366250`. Unloaded collision is unknown. Logical room rectangles alone cannot describe internal passages. |
| Body clearance | `0x364C70` selects point, five-cell cross or 3x3 box from the player's actual pattern. Pattern 1 in this session is the cross, checked by `0x366D40`. A clear central pixel/cell alone does not prove passage. |
| Movement mask | `0x3C5610` passes the player's `Path+0x5C` pattern and `Path+0x64` mask to `0x364700`. The latter visits destination cells and stops at a failed body check; it skips the starting cell. Equal X/Y displacements visit diagonal cell centres. |
| Coordinate projection | `0x334DB0`: screen X = the product of 80 and the difference between X and Y, screen Y = the product of 40 and the sum of X and Y. World +X is screen down/right; world -X is up/left. Fixed screen cardinal rays are world diagonals. |
| Occupancy | Player/creature stamps share the grid with walls, objects and doors. Existing copy-only player-stamp removal and actor/physical layers remain. Bitfields do not fully encode ownership. |

The classic [D2MOO collision implementation](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/src/D2Collision.cpp)
corroborates tile subdivision, room grids and footprint patterns. It is a
community reconstruction of classic Diablo II, **not an offset source for D2R**;
the D2R findings above come from this installation's code and live snapshots.

## Why the previous adaptation missed openings

The original local Kirandur decompile was checked again:
`D:/code/decompiled games/Kirandur/sonar-decompiled/sonar-cluster-focused.c.txt`.

1. `FUN_1403f5de0` compares consecutive **world hit positions** along the ray.
   A wall receding by more than 1.5 query units produces the opening variant even
   if another wall is still hit. The opposite movement produces closing. Merely
   approaching the same world hit point is not a new geometric edge.
2. `FUN_1403f5600` dispatches classification changes separately. Its 0.1-second
   sampling and two-frame invalid-classification retention are not unconditional
   sound triggers. The simplified reference pseudocode gets this wrong.
3. The D2R implementation instead reduced distances to open/limited/blocked.
   A recess from roughly 10 to 17 subtiles was "open" on both sides of the
   change, producing no sound. A regression reproduced that failure before the fix.
4. Its additional forecast required continuing straight through the sound queue
   and 600 ms reaction allowance before allowing an opening. A usable turn could
   be rejected because continuing straight would hit a wall or pass the turn.
   That is not Kirandur's current-world query.

## Installed translation and its deliberate differences

| Kirandur behavior | D2R implementation / remaining difference |
|---|---|
| Four fixed horizontal probes | Preserve the user's fixed screen up/left/right/down axes. Full-body collision replaces a point query. |
| Local range 20, projected hit change > +/-1.5 | `boundaryRange=20` and +/-1.5 in **collision-subtile query units**. One D2 subtile per reference query unit is a chosen adapter mapping, not proof that both games have the same scale. Longer scans remain available for spoken distances and speed warnings. |
| Receding / returning wall, including wall-to-wall | `radar_geometry.cpp` tracks body-hit centres in world coordinates, so existing clear space can widen or narrow audibly. A closing cue means space narrowing, not necessarily immediate inability to move. |
| No current-world exit hidden by speculative future movement | Openings use current full-body and occupancy checks. The future-straight-line veto and speed-expanded opening depth were removed. The D2R eight-subtile usable-depth guard remains. |
| Classification retention | D2R keeps the user's faster two-observation confirmation, normally 50 ms apart, with silent unknown coverage/session rebasing. It does not copy Kirandur's invalid-result semantics onto missing D2R rooms. |
| Motion / approach | D2R additionally warns using measured closing speed, remaining audio time and a 600 ms allowance. This augments geometry changes; it no longer suppresses current side exits. |
| Audio presentation | Existing synthesized 120 ms cues, stereo/pitch directions and 50 ms silent gaps. Original assets, seven semantic sounds and HRTF are not reproduced; no extra native dependency was added. |
| Spoken sonar from current observations | Ctrl+T now prefixes actual recent movement in eight screen sectors, or no recent movement, then explicitly labels the four straight-line passages. A first sample says motion is not measured yet. |
| Doors, portals, heights, slopes and drops | Still not semantically reproduced. D2 collision provides blocking flags; it does not itself establish an interactive door identity, portal destination or Kirandur-style height/drop physics. |

Thus this is a closer translation of the **flat geometry behavior**, not a claim
of one-to-one equivalence for both games. A tin identifies a currently usable
opening/widening; it cannot promise that it will remain available after the player
continues moving or that it leads to a destination. Pending cues are still
invalidated if their geometry reverses or the current opening becomes occupied.
Both sides keep independent history, retry identity and serialized delivery.

## Verification

- `radar-geometry-tests`: 46 checks, including the previously failing finite-wall
  recess, its return, current turns near a forward obstacle, unchanged/parallel
  walls, one-poll flicker, unknown coverage, local range, occupied recess/retry,
  and the recorded 23:52 wall grid with Spanish movement readout.
- `radar-delivery-tests`: 40 checks. Real engine/queue simulation now includes
  repeated finite recesses on both up/down and left/right, with both opening and
  closing tones completing and no self-invalidated second direction.
- Existing body, occupancy, unknown-coverage, lifecycle, running-wall-warning
  and stalled-footstep regressions pass. Earlier tests that required hidden
  running exits were replaced with current-passability assertions, not retained
  as a conflicting specification.
- `build.ps1` deployed successfully with the game closed. All **21 suites / 1038
  checks** passed; 655 concern radar/footsteps. The shared build includes the
  separately committed menu fix. Installed/build DLL SHA-256:
  `76F4FDA5A4D11BE0067558932032B3DE13B5E4C58FE6BDD74CD5C0CE4E7E077A`.

No new-version controller or listening test was performed. Next: walk past a
recess, return through it, and use Ctrl+T while moving along a wall. Generated
events now include `trigger=boundary|clearance` and the world hit; the existing
delivery stages distinguish changed/occupied geometry from lost audio.
