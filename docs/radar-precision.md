# Radar geometry and cue precision

> The user rejected this build's running behavior. The later
> [surface-tracking correction](radar-surface-tracking.md) replaces interpolated
> player samples with verified supporting wall lines, coalesces adjacent facets,
> and stops replaying partially played cues. This investigation is historical.

Implemented 2026-09-16 after the user reported spam during almost any movement
and requested Kirandur's behavior. This supersedes the speed/queue-driven
classification in the earlier radar documents. Controller/listening validation
of this revision is pending.

## Evidence from the installed game and original reference

The 07:05–07:18 D2R session generated 879 direction events over 731.796 seconds:
490 boundary events and 389 clearance events. There were 808 submitted buffers,
746 completions, 71 queue drops and 62 interruptions. These are software events,
not a measurement of perceived sound or navigation success.

Two particularly clear failures used unchanged player cells and world hit points:

- Right, player `(4683,4626)`, hit `(4691,4618)`, clearance 10.5: a closed/open
  pair 125 ms apart as the warning threshold fell from 13.0 to 4.0.
- Up, player `(4664,4555)`, hit `(4656,4547)`, clearance 10.5: another closed/open
  pair 125 ms apart as the threshold fell from 11.8 to 6.5.

Slowing down or a shorter audio queue could therefore announce an opening even
though no passage had appeared. The same wall could also produce a range-entry
cue and a second, independent proximity cue.

The original Kirandur NativeAOT decompile was read directly again:
`D:/code/decompiled games/Kirandur/sonar-decompiled/sonar-cluster-focused.c.txt`.

| Function | Verified behavior |
|---|---|
| `0x1403f5600` | A 0.1 s accumulator polls the fixed rays; the caller dispatches classification changes. |
| `0x1403f5de0` | Wall-to-wall changes compare successive world hit positions projected onto the ray; thresholds are strictly greater than +1.5 or less than -1.5. Losing a previously classified hit can produce the opening variant. |
| Same function | The two-frame invalid-result counter is retention, not an instruction to play every valid scan. |
| `0x1403f6480`, `0x1403f6fd0` | Local world queries supply hit/classification; exhausting the range differs from an invalid result. |

The workspace reference's simplified `FramesSince < 2` pseudocode is not an
equivalent implementation. Neither the original caller nor its wall-change
branch adds D2R's former speed-dependent open/blocked classifier.

## Diablo II map and movement investigation

Read-only Ghidra runs on the installed game's decrypted `D2R_live.exe` rechecked
ten functions. `re/scripts/D2RDecomp.java` is the existing extraction tool; no new
offset, hook or native dependency was introduced.

| D2R RVA | Finding and consequence |
|---|---|
| `0x364700` | Integer destination trace visits collision cells, skips its origin, and stops at a failed body test. Equal X/Y deltas visit diagonal centers. |
| `0x364C70`, `0x366D40` | Runtime pattern 0 tests one cell; 1/3/5 test a five-cell cross; 2/4 test a 3x3 square. A thin center ray cannot establish player clearance. |
| `0x3C5610` | Supplies `Path+0x5C` body pattern and `Path+0x64` movement mask to that trace. |
| `0x334DB0` | Screen displacement is `X=80*(worldX-worldY)`, `Y=40*(worldX+worldY)`. Screen cardinal probes are world diagonals. A wall along a world axis is oblique to those rays. |
| `0x22EEB0` | Destination handling uses collision/path queries. A requested destination is not proof of the player's actual path or current stamp location. |
| `0x340E70`, `0x343920` | When enabled by the native path flags, a rejected target leads to alternate directional candidates, each checked against the body mask. This explains why blocked straight rays do not imply complete immobility. |
| `0x3ACC60`, `0x3822B0` | Movement helpers also constrain candidate endpoints with the native trace. Do not substitute the requested input direction for measured displacement. |

The two 07:09 Ctrl+T snapshots at `(4671,4548)` and `(4670,4548)` show the same
wall at world Y <= 4546 and a clear row at 4547. Their fractional positions
changed along X while Y stayed near 4548.60. This corroborates travel along the
wall; it does not establish the exact analog stick angle.

D2 collision is a 16-bit grid with five subtiles per map tile. Room residency
limits coverage; terrain and actor stamps share cells. The existing verified
adapter, copy-only local-player-stamp removal, actor separation and original
movement-mask opening checks remain. The classic
[D2MOO collision reconstruction](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/src/D2Collision.cpp)
and [flag definitions](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/include/D2Collision.h)
corroborate the grid/pattern model; they are not D2R offset sources.

## Corrected behavior

- **One geometry event source.** Local wall arrival/loss and meaningful finite
  recesses/returns generate cues. Speed and audio backlog no longer create a
  second classification that alternates wall/open on the same obstacle.
- **Stable range edge.** Local range remains 20 collision subtiles. A detected
  wall is retained through one projected cell step beyond that range:
  `abs(axis.x)+abs(axis.y)` (sqrt(2) for the screen-cardinal rays). This avoids
  chatter from a single grid step across the cutoff without a repeat cooldown.
- **Spatial sampling.** When several integer cells pass between observations,
  inspect intermediate origins in the current grid. Only use that interpolation
  if those origins fit the body in known terrain; sliding may have taken another
  path. Intermediate samples never emit cues or assert actual player positions.
- **Continuous walls.** Subthreshold hit drift is removed from pending boundary
  identities so following an oblique wall neither repeats cues nor invalidates
  an already queued recess. Finite changes also require current clearance to
  remain meaningfully wider/narrower than at their origin. This prevents a gap
  passed entirely between polls from leaving an invented opening or closing.
- **Unusable recesses.** A too-shallow or occupied opening stays pending. Its
  return to the original wall does not emit an orphan closing. A real announced
  opening still announces its return on both outbound and return trips.
- **Unknown coverage.** Missing data inside the local scan clears pending cues,
  even when the first eight subtiles are loaded. Restoring coverage establishes
  a silent baseline rather than reviving an old opening.

The original four screen directions, 120 ms tones, 50 ms silent gaps, current
full-body/occupancy guards, two-observation confirmation, lifecycle suspension,
Ctrl+R toggle and Ctrl+T movement/clearance readout remain. Logs now identify all
events as `trigger=boundary` and distinguish actual retries with `retry=1`.

This translates the flat geometry behavior, not the complete Kirandur game.
The one-subtile query-unit mapping, eight-subtile usable-depth guard, range
hysteresis, finite-depth confirmation and serialized synthesized audio are D2R
adaptations. Semantic door/portal identity, elevation physics and the original
HRTF presentation are not reproduced. A closing cue denotes a boundary arriving
or space narrowing; it does not necessarily mean immediate collision. No fixed
reaction-time guarantee is made, particularly at exceptional movement speeds.

## Validation and installation

The new precision suite has 51 checks. Its speed/backlog, duplicate encounter,
range edge and delayed oblique-wall cases all failed before their fixes. Further
regressions caught unusable-niche closures, pending-cue drift, lost room coverage
and the transient thin-gap failure before installation of the final revision.
Real finite recesses still open and close on both rays and both travel directions
at 50, 150 and 200 ms observation intervals. Existing native body/mask, actor,
door, queue, return-trip, lifecycle and footstep suites also pass.

Normal `build.ps1` compiled and deployed at 07:49 BRT. All **23 suites / 1141
checks** passed, including the other agent's controller changes; radar/footsteps
account for 706 checks. Build and installed DLL SHA-256:
`0D66EFB021BEFDBF617F266750CB62C6B8D85BC7E9BA8B7583480326F57FC157`.

The earlier shared 07:38 build already included unfinished radar changes. It
was replaced by this verified build. No new controller/listening session has
been performed. Event-only logs cannot reconstruct every old collision frame,
so the old 879-event session is evidence of failure, not a replay benchmark or
a claimed percentage reduction in live spam.
