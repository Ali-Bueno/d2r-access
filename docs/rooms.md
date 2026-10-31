# Room exits (2026-09-20)

> Since 2026-09-21 each way out never taken is tagged `sin explorar` in the entry line:
> `docs/exploration.md`. The exit finder was removed on 2026-10-22 (below).

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
  where announcing a boundary would be noise. Outside, BACK+Y says "Not in a dungeon room".
- **Chambers of a hand-made inside level** (2026-10-30, a tester: "players rely on Mod+Y to find
  exits"; the Throne of Destruction, Andariel's and Mephisto's levels, the Kurast temples):
  asked for only, never announced on entering. `Rooms::Chamber` floods the loaded floor from the
  player, stopping at the gaps `Rooms::Doorways` finds (every one, door gaps included, not the
  creak's selection); each gap with the chamber on one side only is a passage, nearest first.
  Level exits within a stair's reach (4 subtiles) of its floor are the chamber's. An arch wider
  than two tiles is no doorway, so two halls joined by one are one chamber; floor not loaded
  ends the chamber with no passage said. Log: `[room] chamber: N floor cells, D doorways loaded`.
  A shut door's cells are wall in the terrain layer, so its gap is no passage, as on a maze border.

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
vocabulary: eight compass points, north the top of the screen, or twelve clock
hours ([spoken directions](radar-readings.md#spoken-directions-2026-10-13)).
By default they are not the room's world-axis sides; the switch below says
those, as the screen diagonals they are drawn at.

Level exits standing inside the room — stairs, portals, waypoints — are spoken
with the game's own destination name, which `NativeEntities` already resolves.
Every other opening is a direction alone, because a gap between two maze cells
leads to more dungeon and has nothing to be named after.

### The map's sides (2026-10-14, a switch)

A tester working the exit finder through the Underground Passage asked for the
room's own layout: D2 draws the world axes, along which every wall runs, at
the screen diagonals, at 2, 4, 8 and 10 o'clock on its 2:1 projection. With
`passage-map-sides` on (Ctrl+S, Navigation; on by default, the user's choice
of 2026-10-14; off gives the screen sides below):

- **The room line** says each passage by its wall: northeast (world -Y),
  southeast (+X), southwest (+Y), northwest (-X), or 2, 4, 8 and 10 o'clock.
  A level exit standing in the room is said by the wall it lies toward.
The game turns the left stick the same 45 degrees into the world
([decisions 2026-09-25](decisions.md)), so the stick diagonal that names a wall
is also the one that walks toward it. Tests: `tests/room_sides_tests.cpp`.

## Exit review

Entering a room gives one line. Asking gives the detail:

- **Ctrl+Y** (pad: BACK+Y, console `access-rooms layout`) says the ways out of
  the room you stand in again, whether or not it was announced.
**The exit finder is gone (2026-10-22).** It took the right stick in a maze room and named the
ways out on one side per push (Ctrl+V, BACK+A, `exit-finder`). A tester: the room line, the
asked-for line (BACK+Y) and the running passage announcements already say the passages, so it
was a third system, and its toggle was a pad combination better spent elsewhere. The user agreed.
Removed with it: `ExitReview`'s list, sides and cycle (what the room line still needs is
`RoomSides`), the stick read turned 45 degrees, and the strings `finder*` and `roomOpening`. The
combination it used (BACK+A, or BACK + right stick down with `pad-route-on-a`) is free.

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
- No room name in the exit line. The room's `LvlPrest` row is now known
  (`RoomEx+0x74/+0x40`, `docs/places.md`), and the place service names landmarks
  inside it, but a maze cell's own name (`CaveNE`) only encodes its open sides.
- Maze levels only, by design. Towns and the open acts keep the radar, the
  entity scan and the beacon.
