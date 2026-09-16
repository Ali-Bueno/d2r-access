# Kirandur reference and D2R reaction-aware radar

> Current behavior: [radar precision](radar-precision.md). The reaction/queue
> classifier described below is historical; it no longer generates cues.

> **2026-09-16 update:** [Native map fidelity](radar-map-fidelity.md) supersedes
> the opening forecast described here. Current exits are no longer hidden by
> predicted continued movement; finite wall recesses/returns are audible, and
> Ctrl+T includes actual motion. Queue timing still advances wall warnings.
> The investigation and test results below describe the earlier revision.

Investigated 2026-09-15 after the user reported excessive cues and unusably brief
openings in the camp and Blood Moor. The requested game remains **Diablo II:
Resurrected**; the user corrected their accidental mention of Diablo IV.

## What the reference actually does

Read the workspace's `reference/audio-navigation/kirandur/sonar-navigation-spec.md`
and `10-porting-guide.md`, then checked their claims against the original local
decompile: `D:/code/decompiled games/Kirandur/sonar-decompiled/sonar-cluster-focused.c.txt`.
Addresses below are Kirandur function VAs, not D2R addresses.

| Evidence | Finding |
|---|---|
| `FUN_1403f5600`, lines 3246–3380 | A 0.1-second accumulator drives four primary rays. The caller looks up a classified cue when current type differs from previous type. |
| `FUN_1403f5de0`, lines 4355–4465 | The counter at ray `+0x28` retains a briefly invalid classification. It does **not** authorize an unconditional sound on every valid scan. |
| Same function, wall-to-wall branch | Project the difference of successive **world hit points** onto the ray direction. Changes greater than +1.5 or less than -1.5 select receding/approaching variants; small changes are silent. This branch specifically requires both types to be wall. |
| Same function, lost hit point | Losing a previous non-neutral hit can dispatch the receding/opening variant. |
| `FUN_1403f6480`, lines 5115–5124 | Reaching maximum range can return a valid neutral classification without a hit point. No hit and invalid data are separate concepts. |
| `FUN_1403f4f00`, audio dictionary | Sound entries cover types 1–6; type zero is not an ordinary opening sound lookup. |
| `FUN_1403f6fd0` | Classification distinguishes wall, door and ground using its world queries. D2R's current adapter has collision footprints, not Kirandur's terrain heights. |

**Correction to the workspace reference:** its simplified condition combining
`typeChanged`, a counter below two and projected displacement is not equivalent
to this call flow. Copying that condition literally would produce repeated cues.
The two-frame counter concerns classification retention; ordinary type-change
dispatch occurs in the caller. No original implementation code is copied here.

## Evidence from the latest D2R session

The session beginning 18:19:52 logged **538 emitted radar changes**. Of 264
same-direction opening-to-blocking pairs, 55 were under one second and 120 under
two seconds. These count engine events, not verified completed audio playback.
At 18:29:26.610, right/down blocked at `4678,4557`; at 18:29:27.257, the same
position had 12/21.5 subtiles clear. Such flips motivate checking occupancy;
the old log did not preserve the obstructing cell flags, so it cannot establish
that enemies caused each particular event. Runtime movement mask was `0x1C09`.
Level 2 (Blood Moor) was reached at 18:24:56, with terrain still available.

## D2R adaptation

- Keep the approved fixed stick axes, full native body footprint and 50 ms silent
  audio gaps. Read loaded collision directly; no recreated map is involved.
- Separate a **terrain estimate** from the original **movement collision**.
  D2R `0x341D70` restores monster footprint `0x100`; `0x341600` replaces it with
  corpse `0x8000`. Native pattern stamping `0x363CF0` adds `0x1000`/`0x2000`
  markers alongside actor footprints. Player `0x80` was observed live. These
  two additional functions are fingerprinted, bringing the adapter to 17 checks.
- The terrain estimate excludes recognized actor bits and their co-stamped
  markers. It preserves wall, object, door, no-player, blank and standalone
  marker bits. The classic [collision flag definitions](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/include/D2Collision.h)
  identify water `0xC0`, which overlaps player `0x80`; cells with missile `0x40`
  retain that bit. **Every opening still requires the original movement query.**
  Shared bitfields do not encode ownership, so actor-over-terrain `NO_PATH`
  remains ambiguous and this estimate never proves a path is free by itself.
- Measure actual displacement over 250 ms, using the recent 100 ms direction and
  the larger speed estimate to respond to acceleration/turns. Stop estimating
  movement after 200 ms without displacement. Lifecycle resets discard velocity.
- Allow **600 ms of reaction after playback**. The initial implementation added
  a fixed four-cue batch of `4 × (120 + 50) = 680 ms`. The
  [running revision](radar-running-timing.md) now budgets the actual pending and
  simultaneous cues, including scheduling guards. At the current speed, test each
  half-subtile of continued movement through that horizon. At every future origin,
  the proposed turn must still fit the body through the required open depth.
  Unknown coverage rejects the cue. An exit must already be usable now.
- Required depth after turning is at least eight subtiles or 600 ms of travel,
  whichever is larger. Approaching-wall warnings use closing speed times the
  resulting reaction/playback horizon plus confirmation time, with a three-subtile minimum.
  Thus a tock means **obstacle approaching**, possibly before immediate blockage.
- The initial world-boundary filter suppressed retreat from the same wall.
  The user's later requirement supersedes it: [every real state change](radar-state-delivery.md)
  must be announced on return trips. Confirm two consecutive observations,
  without a travel requirement or an opposite-state cooldown. A known wall in
  the limited band can still establish an opening baseline.
- Unusable openings remain pending instead of being committed as open, avoiding
  an opening/closing pair for a fleeting niche. Stopping at a small usable gap
  can confirm it. Moving actors in an unchanged field cause neither wall nor
  exit events; Ctrl+T can report **occupied passage**.
- Pending cues must still satisfy guidance. A started tin can finish its short
  tail while the current physical passage remains open, even if its future
  reaction window has just shortened. A current obstruction still cancels it.

The reaction window and timing thresholds are **D2R design settings**, not
recovered Kirandur constants. The adaptation does not reproduce its slope/drop
classification, world units or entire seven-class sound vocabulary.

## Validation and practical limits

`radar-guidance-tests` covers native occupancy, preserved terrain/water, short
versus wide exits, acceleration, turns, stopping, occupied doors, intermediate
forecast obstructions, unloaded coverage, early wall warnings and reopening when
retreating from the same wall. The actor-oscillation and limited-corridor regressions were observed
failing before their fixes. Existing footprint, audio, UI and lifecycle suites
remain in place.

Manual timing via `radar-guidance-tests --profile`: 60 moving scans, 35 synthetic
rooms with the containing room last, median **1.56 ms**, maximum **1.93 ms** on
this machine. This measures the engine queries, not native copying or live-game
frame time; there is no fragile timing assertion in CTest.

The forecast assumes the current direction/speed and snapshot persist. It cannot
predict an enemy entering later, a new controller input, attack/cast animation,
or future room streaming. Four directional passages also do not describe an
entire winding route. Unknown data remain unknown; paused/resumed states rebase
silently. **Live perceptual/navigation validation is still required.**
