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
confirmation, or the next room change, clears the mark.

## Queue

`CueQueue::Take` plays the most urgent pending direction first: walls and
doors, then exits, then floor, round robin among equals. Cues held for the
interval or the retreat gate were already retained, never dropped.

## Not ported, and why

- Continuous distance: the world is a subtile grid; nothing to gain.
- World-axis directions: Among Us walls run along its axes. Ours run along
  screen diagonals and the four screen cardinals are the user's choice; the
  pocket whose only way out runs along a world axis is still the exit
  review's job (`STATUS.md`).
- Its pitch encoding is ours already.

Tests: `radar-passage` (door, band, chamber exit, level exit), `radar-engine`
(door cue, way back), `radar-audio` (four timbres, urgency), `rooms`
(typical width).
