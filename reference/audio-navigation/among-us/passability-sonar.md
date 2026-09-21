# Among Us — passability sonar (our own BepInEx mod)

**Source:** our own mod, `D:\code\unity and such\among us access` (full source, no
RE). The design spec and its 43 documented failures live there under
`reference/audio-navigation/passability-sonar/`. This is the cross-game summary:
what it does differently from a plain reactive radar and what we ported.

## The four readings

One cue per state change, per world-fixed direction (N/E/S/W: the characters
have no facing). Distance is **not** sonified (tried and abandoned: with a fixed
reach it always changed at the same point).

| Reading | Sound | Decided by |
|---|---|---|
| Exit | chime, 880 Hz struck bar, long ring | a doorway the body fits through laterally **and** whose far face the centre can reach; or a march past the room boundary by a full doorway width; or a march that lands inside another declared room/corridor |
| ShutDoor | wooden knock, 220 Hz, damped | doorway alignment + centre reaches the near face of a closed door |
| Wall | thud, 165→110 Hz glide | clearance below the doorway width (2×body hysteresis once walled) |
| Open | soft filtered-noise breath | floor with room to move that leads nowhere new. Explicitly **not silent**: silence lost 25 of 96 test readings |

N/S sit centre and are told apart by ±3 semitones; E/W pan 0.9. Flat 2D, never
spatialized.

## The rules worth stealing

- **Reachability over range.** "Is anything walkable within vision radius" was
  the single biggest bug (892 false positives on one map). A route counts as an
  exit only when it provably gets *somewhere else*.
- **One derived scale.** Every threshold is `wayThrough`, the median width of
  the current map's own doors (fallback: corridor width, then body width).
  Nothing about the geometry is configurable, "and that is the feature".
- **No dead band.** Wall vs. not-wall is one threshold with hysteresis keyed
  on the committed reading; the first sweep always yields a verdict.
- **Asymmetric belief.** Exit and ShutDoor are believed on the first sweep, a
  non-exit needs three (~0.3 s), so a doorway walked past quickly is not lost.
- **The way you came in.** Crossing a doorway marks the reciprocal direction;
  its next exit reading is taken as read, not replayed.
- **Hold, don't drop.** Cues arriving during a direction's rest are held;
  exits are prioritized when several are pending; round robin starts one past
  the last speaker.
- **Validation by sampling.** An offline grid sampler classifies ~1500 points
  per map and diffs two builds point by point, plus 41 unit tests over the
  pure rules.

## Ported to D2R (2026-09-21)

Four readings (Exit / Open / Blocked / Door), the committed-reading hysteresis,
the derived doorway width, the way-back mark and the urgency-ordered queue:
`docs/radar-readings.md` in the D2R repo. Not ported: continuous distance (grid
world) and world-axis directions (D2 walls run along screen diagonals).
