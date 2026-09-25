# Radar readings: exit, floor, wall, door (2026-09-21)

> Since 2026-09-21 an exit or floor reading may carry the **unexplored** qualifier: the same cue
> with a short lower tail. Rules, sound and memory in `docs/exploration.md`.

The reactive radar used to know two words per direction, open and blocked. It
now speaks four, ported from the passability sonar of our Among Us mod
(`reference/audio-navigation/among-us/passability-sonar.md`), whose central
lesson is that a cue you cannot act on is worse than none: "there is floor
that way" and "that way leads somewhere" are different facts.

| Reading | Sound | Meaning |
|---|---|---|
| **Exit** | bright tin (1100 Hz) | the proved route leads out: into another chamber, or to a level exit |
| **Open** | soft breath of filtered noise | floor you can walk, but it leads nowhere new |
| **Blocked** | dull tock (260 Hz) | a wall within the closing depth |
| **Door** | two knocks (180 Hz) | a closed door within the closing depth: the game's own DOOR bit is what blocks |

Down is the same sound lowered by the compiled fraction; left and right are
panned. The `test` audition of `access-radar` plays all four in every direction.

## Rays along the walls: the stick diagonals (2026-09-25)

> **Default back to the cardinals the same day** after the user listened: every
> diagonal sounds from one side, so walls and exits up and down at once were an
> overload of cues. The diagonal frame stays selectable (`[radar] diagonals =
> true`, `access-radar diagonal`). `docs/decisions.md`.

**Why.** D2's walls and corridors run along the world axes. The four stick
cardinals are world diagonals (projection `0x334DB0`), so each of their rays
meets every wall at 45 degrees; in a corridor along a world axis all four read
wall even though the corridor goes on. The user asked to fix that.

**The stick.** The game turns the stick into movement by rotating it 45 degrees
into the world, not through the screen projection: over the 41 logged cues of
2026-09-25 with the stick pushed and the character moving, the velocity is off
that rotation by a median 6 degrees, against 14.5 for the projection, and the
stick pushed to a corner (0.87,-0.85) moved along world -Y within 2 degrees.
So the stick's diagonals are exactly the directions the walls run in.

**The frame.** `Radar::Frame` (`src/radar.h`) holds the rays: `Cardinal`, the old
stick cardinals, and `Diagonal`, the world axes: up-left (-X), up-right (-Y),
down-left (+Y), down-right (+X). The service applies `[radar] diagonals`
(default false since listening); `access-radar diagonal|cardinal` switches for the run, forgets
what the old rays said (silent baseline) and speaks the four names. Only the
radar changes; the NPC announcer, terrain overview, scanner and room exits keep
the screen directions.

- The engine, passage scan and wall memory are unchanged; only the axis differs.
  The scan's lateral tolerance stays one native cell: on an axis ray that is
  one column either side, so a doorway exactly as wide as the body is still
  visible for three cells while walking past it and can be confirmed.
- Sound: the same tin, tock, knock and breath. Every diagonal is panned to its
  side (0.9 of the way); the two lower ones take down's pitch (`DownPitch`,
  `BreathDownPitch`), so left/right and up/down are two independent cues.
- Unexplored qualifier: `Frontier::Quadrants` marks both frames in one pass; a
  diagonal ray takes the frontier whose dominant world axis is its own.
- Names: the readout and the switch use the eight screen sectors
  (`radarMovementDirections`), already in every locale; logs say `up-left` etc.

**Measured.** A seven-cell corridor along world x: the cardinals read wall in all
four directions, the diagonals read floor along it and wall across it; walking
along it a cell per poll, the one-tile doorway in its side wall sounds as floor
then wall, and the wall across the far end is announced ahead
(`CorridorAlongAWorldAxis`). Two maze chambers joined by a one-tile doorway,
walked through along world x: 10 cues on the cardinals (wall and exit flips from
the 45-degree geometry), 2 on the diagonals (exit ahead, then floor, the way back
taken as read). Serpentine walks over the two captures give similar totals (Blood
Moor 76 cardinal / 68 diagonal, camp alcove 42 / 48).

**Limit.** The rays are exact; the stick is not. A stick held a few degrees off
its corner walks slightly across a corridor, and with wall-slide prevention on
the character stops when it reaches the side wall. The radar describes the four
exact diagonals.

## Precision review against the Among Us sonar (2026-09-25)

The user found the radar mostly right but sometimes imprecise, and silent on
very open ground. A review against the Among Us spec and its failure list
(`among us access/reference/audio-navigation/passability-sonar/`) found four
gaps, each now covered by a test that fails without its fix:

- **Small obstacles on open ground were erased.** The clutter filter
  (`radar_structure.cpp`) removes lone stones, trees and posts up to one map
  tile because stock movement steers round them. Since 2026-09-22 wall-slide
  prevention (`docs/wall-sliding.md`, on by default) stops a direct move at
  them instead, so the radar called open the very thing the character stopped
  against. On the Blood Moor capture (`tests/recorded_return_grid.h`), 102 of
  about 1,700 readings over the 25x25 cells around the player said open where
  terrain blocks within the warning distance; one is a 4x4 stone (`WALL|PRESET`)
  a few cells down, read as open 33.9 instead of blocked 6.4. The filter now
  applies only while the game slides (`World::slides`, from
  `WallSlideService::Enabled`). Among Us had the same failure: with the `Ship`
  layer alone its sonar walked the player into tables.
- **A gap passed right after its wall was announced was lost.** The cue
  interval was counted from the last cue of any kind, so an opening confirmed
  within 500 ms of a wall cue waited, and the wall returning after a narrow
  gap replaced it: "tock ... tock". Walking along a world-axis wall at one
  cell per poll, a one-tile gap four cells past where the ray met the wall
  was never heard. The
  interval now counts between openings only (`Sensor::lastOpeningMs`), as the
  setting always described it; walls were and are never delayed. Among Us
  kept these gaps because its walls need three sweeps to be believed.
- **A reading that returned to what was last said still sounded.** Floor that
  turned into an exit and back inside the cue interval repeated the floor cue.
  A passable change equal to the last cue is now dropped (`hold=unchanged`),
  and a way-back taken as read records its qualifier too. Walls still re-warn
  on every approach.
- **The way-back mark never lapsed.** It now lapses a doorway's width
  (`wayThrough`) from where the border was crossed, as in Among Us.

**Tried and rejected: holding the retreat instead of dropping it.** Among Us
holds a cue back rather than dropping it, and here backing off a wall drops
the opening, so a gap found by then walking along that wall does not sound.
Holding it was built and tested, then reverted: D2 walls run along the world
axes, at 45 degrees to all four screen rays, so the route reaches past the
remembered point as soon as the player walks along the same wall, and the
held opening sounded as a phantom breath a few cells later. The comparison
walk and `AlongAnAxisWall` in `radar-engine` show both cases. Gaps are found by
walking along a wall within the warning distance: breath at the gap, tock
after it.

A serpentine walk over the Blood Moor capture (every other row of the 21x21
around the player, a cell per poll) gives, with the game sliding, the same 59
cues as before, several openings up to 400 ms earlier; with prevention on it
gives 76, the 17 new ones all the stone: a wall while it is ahead, floor once
it is passed.

## Louder cues and a darker downward breath (2026-09-25)

The user asked for slightly louder, more pronounced cues, the breath above all,
and a clearly graver breath downward: an opening up and one down at the same
time were hard to tell apart, because `DownPitch` (0.86) barely moves filtered
noise. Every cue's `Level` is 0.17 (was 0.15), `BreathLevel` 2.4 (was 2.0), and
the downward breath's cutoff is `BreathDownPitch` = 0.55 of 700 Hz, almost an
octave darker, raised by 1/sqrt(0.55) because low-passed noise loses power with
the square root of its cutoff. The tin, tock, knock and unexplored pips keep
`DownPitch`. Listening under the game's ambience remains the final check.

## Cue separation and outdoor exits (2026-09-22)

The user requested a slightly darker floor breath to distinguish it from the
upward exit cue. Its low-pass cutoff is now 700 Hz instead of 880 Hz, with gain
2.0 instead of 1.8 to keep approximately the same signal level. Cue duration,
stereo placement and the other readings retain their existing sound. Listening
under the game's ambience remains the final check.

The radar now requests area-border bearings as well as distant warp presets.
This restores outdoor transitions such as Rogue Encampment to Blood Moor to
its exit candidates. These bearings describe the meeting of level rectangles;
the local passage scan still checks the approach depth and width, so including
a distant bearing alone does not produce an exit cue.

## What decides an exit

`ScanPassage` (`src/radar_passage.cpp`) proves the straight route as before
and then asks what it means:

- **A level exit on it.** The route arrives within a doorway's width of an
  exit position that lies on the route's own width (`World::exits`: warps,
  stairs, portals and outdoor border exits, from the same entity reader the
  exit review uses, refreshed once a second).
- **Another chamber.** On a maze level (`Levels.txt DrlgType 1`, where one
  streamed room is one chamber, `docs/rooms.md`) the route crosses into
  another streamed room *and* continues a doorway's width past the border.
  Touching the border is not leaving through it. Outside maze levels the room
  partition means nothing, so only level exits make an exit.

The doorway's width, `wayThrough`, is the level's own: the median width of
every opening of every streamed room (`Rooms::TypicalOpeningWidth`), clamped
between the body span and the opening depth. It falls back to the opening depth
(two map tiles) when the partition means nothing or no opening is loaded yet.
The wall distance itself stays at the validated two tiles: it is a reaction
distance, not a doorway.

## No dead band

The two thresholds are one hysteresis: passable at or beyond the opening
depth, an obstacle at or below the closing depth, and in between **the
committed reading stands** (`PassageQuery::committed`). A committed door
stays a door there; a committed floor or exit is read again, because the band
only protects obstacles. With nothing committed the band reads as floor, so
the first observation after a reset is always a verdict. The 2026-09-20
"wall swallowed as the silent baseline" class of bug cannot recur.

## The way you came in

When the player's streamed room changes on a maze level, the direction most
against the step that crossed the border is marked (`Sensor::wayBack`). Its
next confirmed exit reading is the doorway just walked through: it is taken
as read (`hold = "way-back"`, `lastCue = Exit`) and not sounded. Any other
confirmation, the next room change, or walking a doorway's width
(`wayThrough`) from the crossing clears the mark.

## Queue

`CueQueue::Take` plays the most urgent pending direction first: walls and
doors, then exits, then floor, round robin among equals. Cues held for the
interval are retained, never dropped; an opening withheld by the retreat gate
is dropped on purpose (see the 2026-09-25 review above).

## Not ported, and why

- Continuous distance: the world is a subtile grid; nothing to gain.
- World-axis directions: ported on 2026-09-25 as the diagonal frame (above);
  the cardinals stay selectable.
- Its pitch encoding is ours already.

Tests: `radar-passage` (door, band, chamber exit, level exit), `radar-engine`
(door cue, way back), `radar-audio` (four timbres, urgency), `rooms`
(typical width).
