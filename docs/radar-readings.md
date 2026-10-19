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
| **Open** | soft breath of filtered noise, 180 ms, rising up / falling down | floor you can walk, but it leads nowhere new |
| **Blocked** | dull tock (260 Hz) | a wall within the closing depth |
| **Blocked**, small | short hollow knock (640 Hz) | a small obstacle you can step round (switch `small-obstacles`) |
| **Door** | two knocks (180 Hz) | a closed door within the closing depth: the game's own DOOR bit is what blocks |

Down is the same sound lowered by the compiled fraction, except the breath, which
rises for up and falls for down; left and right are panned. The `test` audition of `access-radar` plays every reading in every direction, the small obstacle and the wall bump included.

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
the screen directions (until 2026-10-19, when every compass point became a way to
walk: [spoken directions](#spoken-directions-2026-10-13)).

- The engine, passage scan and wall memory are unchanged; only the axis differs.
  The scan's lateral tolerance stays one native cell: on an axis ray that is
  one column either side, so a doorway exactly as wide as the body is still
  visible for three cells while walking past it and can be confirmed.
- Sound: the same tin, tock, knock and breath. Every diagonal is panned to its
  side (0.9 of the way); the two lower ones take down's pitch (`DownPitch`) and
  down's falling breath, so left/right and up/down are two independent cues.
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

## A rising and falling breath, from the D4 radar (2026-09-28)

The user still could not tell an opening ahead from one behind: the darker
downward breath of 2026-09-25 was not enough. They preferred the breath of our
D4 mod (`D:\code\d4 dungeon assist`, `src/D4Assist.Audio/RadarCueSynth.cs`),
which is copied unchanged: the same noise through a resonant band-pass (Q 3)
whose centre sweeps 900 → 2400 Hz for an upward ray, 600 → 220 Hz for a
downward one, and holds at 1200 Hz for the sides. It lasts 180 ms
(`Radar::BreathDurationMs`) instead of a struck cue's 120 ms so the sweep is
heard, and is peak-normalized to 0.7 of the struck cues' level. Diagonals follow
their screen side. Replaces `BreathCutoffHz`, `BreathLevel` and
`BreathDownPitch`; the tin, tock, knock and pips are unchanged. The falling
breath shares a direction with the enemy swoop (`docs/entity-cues.md`), but that
one is voiced, not noise; listening in the game remains the final check.

## Louder cues and a darker downward breath (2026-09-25)

> Superseded for the breath by the rising and falling sweep above.

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

## Small obstacles and the wall bump (2026-10-11)

Both are radar sounds on the radar's device, volume and `mod-sounds` switch,
each with its own switch in the settings list (on by default).

**Small obstacle** (`small-obstacles`). Collision cannot tell a rock or pillar
from a wall (both are baked `WALL|PRESET`), so the shape decides: the passage
scan records the blocked body position nearest the ray's axis within the
closing depth (`Passage::obstacle`), and `SmallObstacle` (`radar_structure.cpp`)
tests the blocked cells under it with the clutter filter's own rule: an
8-connected WALL/OBJECT group, fully loaded, at most one map tile (5x5) across,
standing alone with a way round it. The search stops as soon as the group
outgrows the tile. It runs whatever the sliding switch says; while the game
slides, such clutter is filtered out of the scan and never heard at all. The
flag rides on the wall cue like the unexplored mark (`Sensor::small`, set when
the wall is confirmed) and changes only the timbre: a short (12 ms decay),
higher knock with an inharmonic partial, between the tock and the tin. With the
switch off the radar is unchanged.

**Wall bump** (`wall-bump`). A 90 ms thud when the stick pushes into terrain,
ported from the Last Epoch mod (`BumpCueSynthesizer`, `WallBumpDetector`): a
sine falling 150 → 70 Hz (30 ms decay) under a one-pole low-passed noise click
(1800 Hz, 6 ms decay, half the body's level), 2 ms attack, 12 ms release, at
1.4 × the tock's level, panned by the push's screen-horizontal share. The
evidence is the game's own refusal: the wall-slide hook
(`native_wall_slide.cpp`, `docs/wall-sliding.md`) counts every fallback probe of
the local player's directional move, prevented or not, with the direction the
solver tried first (its desired probe, return `0x3AB24C`).
`WallBump::Detector` thuds when, in one radar poll, attempts rose, the exact
position did not move (a slide is no contact) and terrain blocks within two
cells on either axis along the push (a creature is not a wall). It thuds once per contact and
re-arms after a step of one subtile, a push turned past 45 degrees, or 300 ms
without a refused attempt; never twice within the radar's cue interval.
It needs the native movement hook; it runs while the radar's world can be read,
the radar itself on or off. Each thud logs `[wall-bump]`; a refused push it leaves silent logs
`[wall-bump] silent refusal: slid` or `no terrain along the push`, at most every 2 s. Live
2026-10-12 (Lut Gholein): a push screen-up was silent where sideways thudded. Screen directions
are world diagonals, and the contact test stepped two cells along the push, which on a
diagonal reaches only 1.4 cells on either axis; it now walks until two cells on either axis. Footsteps against the
wall stay muted by the stalled-footstep filter (`footsteps`), which keys on the
same frozen position.

2026-10-12, a tester: Act 1 borders and the Rogue Encampment's edge did not thud. Contact now
also counts a cell outside every room or BLANK (the game reads it as 0x27, WALL included,
`docs/collision-semantics.md`), and tests the body one cell along the push, so a ragged border
beside the line touches too. The radar itself still treats those cells as unknown. Small obstacle
off gives the plain tock: no code path silences it (the tester heard nothing), so the radar level
was raised instead (below); a log from a rock with the switch off would settle it.

## Levels and wall tones (2026-10-12)

The radar sat 6-9 dB under the item loops (the tester and the user: a bit quiet). `CueLevel` is
0.24 (was 0.17, +3 dB); the door's double knock plays 1.4 times that with an 18 ms decay (was
12 ms), the faintest cue before ("Sound-Radar-Door inaudible").

**Wall tones** (`wall-tones`, off by default; `[wall_tones]` in `d2r-access.toml`): a soft band of
noise per screen direction, louder as terrain closes in within 12 subtiles, (1 - d/range)^2, bands
800/450/450/250 Hz, ported from the Last Epoch mod's `WallSonar` (Grimdark's walltones). Three
lanes across the body, the farthest winning; off the map counts as a wall. Its own XAudio2 voices
(`wall_tone_audio.cpp`), silent in menus, dialogue and pause. Unheard in game so far.

## Spoken directions (2026-10-13)

Every direction the mod says goes through `SpokenDirection` (`src/spoken_direction.h`): the
scanner, the NPC announcer, room exits, the exit finder, the terrain overview, the radar
readout and its sound test, the keyboard's target lines and the route description. They are
still screen directions, named two ways (Ctrl+S, Navigation, `direction-style`):

- **Compass points** (default): north is the top of the screen, east its right. Eight points
  for a bearing (`Radar::ScreenSector`), four for a side (radar rays, finder, overview).
- **Clock hours**: twelve is the top, three the right. A bearing is one of twelve hours
  (30 degrees, finer than the eight points); a side is 12, 9, 3 or 6.

The tester asked for compass words instead of up/down/left/right and the user added the clock
as an alternative. Distances are **units** (one collision subtile), no longer "steps": a step of
the character covers several subtiles. Per language the twelve hours are written out
(`clockHours`), so each grammar ("a la 1", "на 2 часа") is right.

**The eight points are the ways to walk** (2026-10-19, the user): a compass point is the nearest of
the eight ways the stick and the arrows walk, the world turned 45 degrees without the screen's 2:1
squash (`SpokenDirection::Bucket`), so a world axis (every wall, every doorway) is exactly a
diagonal. Before, the points were cut on the projection, where east and west each covered 79 degrees
of walking and north and south 23: a route leg through a doorway was "east" and walked into the
frame, and a target the stick reaches by pushing southeast was "east" too. Every bearing follows:
scanner, BACK+RT, NPC announcer, room exits, area overview, radar motion, keyboard target lines and
the route ([route description](route-description.md)). Clock hours keep the projection, which puts
the axes at 2, 4, 8 and 10 o'clock. `Radar::ScreenSector` stays the screen's for logs and ray names.

## Queue

`CueQueue::Take` plays the most urgent pending direction first: walls and
doors, then exits, then floor, round robin among equals. A wall bump plays
before any of them, a newer one replacing an unplayed one. Cues held for the
interval are retained, never dropped; an opening withheld by the retreat gate
is dropped on purpose (see the 2026-09-25 review above).

## Not ported, and why

- Continuous distance: the world is a subtile grid; nothing to gain.
- World-axis directions: ported on 2026-09-25 as the diagonal frame (above);
  the cardinals stay selectable.
- Its pitch encoding is ours already.

Tests: `radar-passage` (door, band, chamber exit, level exit), `radar-engine`
(door cue, way back, small obstacle), `radar-audio` (timbres, urgency, bump),
`radar-structure` (small obstacle), `wall-bump` (contact, re-arming), `rooms`
(typical width).

## The wall ahead, again and again (2026-10-16)

A tester's friend asked for the wall knock to repeat while running into the wall (knock, knock,
knock), the weather drowning the wall tones' swoosh. `Engine::SetWallRepeat` (`radar-wall-repeat`,
Loop speeds; once by default, every 250 ms to 1.5 s): the ray within 45 degrees of where the player
heads -- the velocity, or standing, a push the game refuses (`RadarService::PushHeading`) -- sounds
its wall or door again each interval while it stays confirmed and seen. The other rays keep once
per change. The setting survives the engine's rebase.
