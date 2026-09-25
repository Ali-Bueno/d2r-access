# Room exits (2026-09-20)

> Since 2026-09-21 each way out never taken is tagged `sin explorar` in the entry line and listed
> first in the review: `docs/exploration.md`.

Walking into a room in a dungeon speaks the ways out of it:

> *"Salidas: adelante, a la derecha, Escalera a Catacumbas nivel 2 atrás"*

Ported from the Grim Dawn accessibility mod `grimdark`, which answers "where am
I and how do I leave" with a named room plus a cycled exit list
(`reference/audio-navigation/grim-dawn/navigation-spec.md` §5-6). The port is
much smaller than the original because D2 gives us the segmentation for free.

## Why there is no segmentation algorithm

`grimdark` bakes an offline SQLite map of 18,070 segmented rooms because Grim
Dawn's world is fixed and hand-made. D2 generates its levels, so there is
nothing stable to bake — but it also does not need one:

- A **maze** level (`Levels.txt DrlgType = 1`) is assembled out of `LvlPrest`
  cells, and **one streamed room is one cell**: a chamber or a corridor
  segment. 40x40 subtiles in the Crypt, 60x60 in the Catacombs, 120x120 in the
  Den of Evil. The generator already did the work.
- Other level types are excluded. `DrlgType 2` is a hand-built preset area (a
  town, a quest area) and `DrlgType 3` is open ground; in both the room
  partition is the generator's own bookkeeping and cuts across open floor,
  where announcing a boundary would be noise.

`src/generated/level_flags.h` carries the per-level flags, generated from the
game's own `Levels.txt` by `tools/gen_level_flags.py` and committed like the
other generated headers. Its row count is the patch tripwire.

## What counts as a way out

`Rooms::Describe` (`src/rooms.cpp`) walks the four borders of the room the
player stands in and keeps the runs of cells that are open **on both sides**:

- **The cell across the border must belong to another streamed room.** A hole
  in the wall backing onto unknown ground is not an exit. D2 only builds a
  collision grid for *active* rooms (`Room1+0x38`, freed on unload), so a
  doorway into a room the game has not streamed in yet is invisible to us. It
  stays unspoken rather than guessed — the same positive-evidence rule the HUD
  sight gate uses.
- **The gap must admit the body.** `PassableWidth` comes from the native
  footprint patterns: a point body passes a one-cell slot, a cross or square
  body needs three, because it occupies the cells beside it too. This is
  `grimdark`'s wall-sliding lesson — the probe has to measure what the body
  measures, not what is convenient.
- **The terrain layer, not the movement layer.** A monster standing in a
  doorway does not close the doorway, exactly as the beacon and the radar read
  it.

## What is spoken

Directions are **screen directions** from the player, in the entity scan's own
vocabulary (`scanDirections`: adelante / a la izquierda / a la derecha / atrás).
They are not the room's world-axis sides: D2's screen axes are the world
diagonals, so "north" would name a direction the player cannot walk.

Level exits standing inside the room — stairs, portals, waypoints — are spoken
with the game's own destination name, which `NativeEntities` already resolves.
Every other opening is a direction alone, because a gap between two maze cells
leads to more dungeon and has nothing to be named after.

## Exit review

Entering a room gives one line. Asking gives the detail, one way out at a time,
nearest first:

> *"Escalera a Catacumbas nivel 2, 12 metros, a las 4, rodeando, 1 de 3"*

Label, distance, clock bearing, how you get there, and where you are in the
list. **Ctrl+V** or `access-rooms next` (the R3 hold was removed on 2026-09-25:
the scanner covers it); the beacon is a short L3 click. Each request moves to the
next one and wraps; the place in the list is kept by the point under review, so
walking about and asking again continues where you were. Leaving the room or the
level starts over at the nearest.

- **The label.** A plain opening has no name to give, so it is the passage word
  (`roomOpening`); a stairway, portal or waypoint keeps the game's own name.
- **The list.** In a maze level, standing inside a streamed room: the room's
  openings plus the level exits standing inside it. Off-grid on a maze level —
  and everywhere else, town, the open acts, where the room partition means
  nothing — it is every level exit the game knows about, which is what the
  entity scan already sees (`docs/distant-exits.md`).
- **The clock.** Screen frame, 12 straight up the screen, over the same
  isometric projection as everything else. The world axes fall between two
  hours and round symmetrically (2, 5, 7, 10), so the four screen cardinals stay
  exactly 12, 3, 6 and 9.
- **How you get there** is the beacon's own verdict on the same collision
  snapshot (`Beacon::ReachWord`: recto / rodeando / no se llega, nothing beyond
  the search box), computed only for the one being spoken.
- **It is offered to the beacon**, so a short left-stick click (or Ctrl+B) right
  after locks onto it and pings the way there. An opening is not a unit, so the
  beacon takes it as a fixed point (`docs/beacon.md`).
- **Never re-announced as you walk.** The review is on demand only; grimdark
  found the alternative "reads as noise".

## Timing and silence

| | |
|---|---|
| Poll | 250 ms; walking into a room is slow |
| Settle / dwell | 1000 / 400 ms, ported from grimdark `core::rooms::Hysteresis` |
| Repeat | never for the same room; forgotten on level or game change |
| Silent | menus and dialogue (`RadarUi::Inspect`), outside maze levels, and on **Ctrl+M** / `access-rooms off`; the automatic line also waits while the UI reader is speaking (`uiOwnsSpeech`), the asked-for review does not |

`Rooms::Hysteresis` inverts what a settle normally means, which is what makes it
feel right: the dwell applies **only inside the first second after a change**.
The first room, and any room entered after a settled second in the previous one,
is spoken at once; only a flip back and forth on a threshold has to hold for
400 ms, and straddling a border never announces anything. Off-grid keeps the
current room — not knowing where you are is not leaving.

`[room] entered` in the log records the rectangle, the opening count and the
spoken line.

## Limits

- Only streamed rooms. An opening into ground D2 has not loaded is not
  reported, and there is no way around that from inside the process: inactive
  rooms have no collision grid at all.
- No room name. Nothing recovered on the room identifies its `LvlPrest` row, so
  a room is described by its ways out, not by itself. The preset filenames do
  encode the open sides (`CaveN`, `CaveNE`), which would be a cross-check if
  that link is ever reversed.
- Maze levels only, by design. Towns and the open acts keep the radar, the
  entity scan and the beacon.
