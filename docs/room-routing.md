# Room-level routing: the game's own nearness rule

Why the beacon needed a coarse map, where the adjacency rule came from, and
what is still unmeasured.

## The problem

`native_world.cpp` sets `SnapshotRadius = 152`. That is not a tuning choice:
the game allocates collision (`Room1`) near the player and frees it again, so
there is nothing further out to read. `Navigation::Build` therefore cannot see
past ~150 subtiles however it is asked, every catalogued object beyond that
has `steps == -1`, and `Beacon::FindRoute` falls back to the reachable cell
nearest the target — a straight line with local avoidance. In a winding
dungeon that heading can point confidently through a wall into the back of the
right room.

Raising `searchRadius` cannot fix this. There is no collision out there to
search.

## What is available instead

The level's **logical** rooms all exist from the moment the level does, with
their tile rectangles, active or not. `native_level_exits.cpp` already walks
every one of them (`Level → LevelFirstRoom → RoomNext`) and reads each
rectangle, then keeps only `WarpType` presets. `native_rooms.cpp` does the
same walk and keeps the rectangles.

## The adjacency rule is the game's, not ours

**Read from `0x3614C0` (653 bytes), the function that rebuilds a room's near
list.** It walks every room in the level and keeps those whose gap, measured
between facing edges and negative where the rectangles overlap, is under six
**tiles** on both axes:

```c
void BuildNearList(Room2* self) {
  self[+0x18] = 0;                                  // near count
  other = self[+0x90]->[+0x10];                     // room->Level->FirstRoom
  do {
    gapX = (self.x < other.x) ? other.x - (self.x + self.w)
                              : self.x - (other.x + other.w);
    gapY = ... same on Y ...
    if (gapX < 6 && gapY < 6) push_back(other);     // vector at +0x10/+0x18/+0x20
    other = other[+0x48];                           // ->next
  } while (other);
  // then bubble-sorts by (x + w <= other.x) || (y + h <= other.y)
}
```

`RoomGraph::Near` is that predicate, reimplemented on rectangles we read
ourselves. Two independent reasons it cannot silently drift:

- **Every offset it uses is already in `level_exit_layout.h`** and was derived
  from this same function: `LevelFirstRoom 0x10`, `RoomNext 0x48`,
  `RoomTileX/Y/Width/Height 0x60/0x64/0x68/0x6C`, `ExtendedLevel 0x90`. The
  decompiled body confirms each of them, and `Level+0x1F8` compared against a
  level id confirms `LevelId` too.
- **`0x3614C0` is entry one of `entity_layout.h`'s `Checks` table**
  (hash `0xA85E778CC89FCD70`), so `LayoutCheck::Verify` already refuses an
  image whose bytes differ. Re-hashed offline against the whole-image dump on
  2026-09-19: all four entries match byte-exact, so the bytes read here are
  the bytes the live check verifies.

### Why we compute it instead of reading it

The game caches the list in the room itself, and **both call sites build it
lazily**:

```c
if (room[+0x18] == 0) BuildNearList(room);   // 0x328da0 and 0x328fd0
```

A room nothing has touched has an empty list. Distant rooms — exactly the ones
routing needs — are the ones that have not been touched. So the list is not
readable for them, and the predicate has to be applied to the rectangles we
read ourselves.

(Distinct from `ground_item_layout.h`'s `RoomNeighbors = 0` /
`RoomNeighborCount = 0x40`: that is **Room1**, the collision room, reached
through `RoomExtended 0x18`. Only active rooms have one, which is why
`native_entities.cpp` only ever sees nearby units.)

### It is proximity, NOT passability

The game uses the near list to decide which rooms to *consider*. Two rooms can
satisfy `gap < 6` across solid rock. **Nothing may treat a room route as a
proved path**, and nothing here does — see the next section.

## How the doorway is found

`RoomGraph::ChooseSubGoal` never picks a point by geometry. Every candidate is
a cell `Navigation::Grid` has already proved reachable, i.e. one the BFS over
the game's own neighbouring-destination graph arrived at. In order:

1. **the reachable cell inside the next room with the fewest steps.** By
   construction that is the nearest way in — the doorway — not the room's
   middle and not its far side;
2. **failing that** (the next room has no collision yet) the reachable cell in
   the *current* room closest to the next room's rectangle, which walks the
   player to the boundary they have to cross;
3. **failing that, nothing**, and the beacon keeps doing exactly what it did
   before.

So a sub-goal can no more point through a wall than the grid can, and a room
route that is geometrically adjacent but not connected simply fails to produce
a sub-goal rather than misleading anyone.

`Beacon::Tracker::Tick` takes the sub-goal as an optional `waypoint`. Only the
route follows it; distance, arrival and ping cadence stay measured against the
real target, so the lock is unchanged. `beacon_service.cpp` supplies one only
when the target is in a different room *and* the grid cannot already reach it
— when the direct search works, it is left alone.

## Unmeasured, and how to settle it

**Whether the next room is usually inside the 152-subtile window.** Case 1 is
the good case and needs the next room to have collision; case 2 is the
fallback. Room dimensions come from level generation and have not been
observed live, so which case dominates in a real dungeon is not known.

It cannot make the beacon wrong — every branch ends on a proved cell — only
less helpful. The measurement is already logged: with a beacon locked on a
distant target, `[beacon] hop=… in_next_room=… room=X,Y WxH tiles rooms=N`
gives the room rectangle and the case taken on every log tick.

**Also unmeasured:** how many rooms a real level has, and so whether a rooms
category is a usable list or an overwhelming one. `rooms=N` in the same line
answers that.
