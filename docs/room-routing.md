# Room-level routing: the game's own nearness rule

Why the beacon needed a coarse map, where the adjacency rule came from, and
what is still unmeasured.

Since 2026-10-12 the beacon is led by the remembered map instead
([walkable guidance](beacon.md#walkable-guidance)): rooms that touch are not
proof of a way. The sub-goal below still leads keyboard walks the map has no
way for, and the beacon only when no map is kept.

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
satisfy `gap < 6` across solid rock, or meet only at a corner. **Routing does
not use it** (see the next section); `RoomGraph::Near` remains for the level
areas (`level_areas.cpp`).

## Routing links: touching rooms, minus proven walls (2026-09-24)

Walking from one room into another means stepping across an edge they share,
so the route graph links only **touching** rooms (`RoomGraph::Touching`: one
axis gap zero, the other overlapping). Rooms meeting at a corner, or near
across rock, are not linked.

Where collision is loaded, `RoomGraph::Border` judges the shared edge:

- **Open**: an open cell on one side steps (diagonals included) onto an open
  cell on the other.
- **Closed**: one side is fully loaded with no open cell, or both sides are
  fully loaded and no open cells meet.
- **Unknown**: otherwise, and the link is kept.

`RoomGraph::Distances` counts room hops to the target over links that are
not Closed. The unexplored-room scanner uses the same rule: a frontier room
must touch explored ground through an edge the survey grid does not prove
closed (`RoomCatalogue::Exploration`).

### Why (live log, 2026-09-24, Cave level 1)

The earlier breadth-first route over the near list chose chains such as
room 1516,1316 → 1524,1324, which meet only at a corner. With no reachable
cell in the next room, the fallback led the player to the boundary nearest it,
a dead end: `hop=7595,6604 in_next_room=0` repeated for 30 s before
`released reason=known-route-unavailable`. Standing where three rooms meet also
flipped the current room each second, and with it the plan.

## How the doorway is found

`RoomGraph::ChooseSubGoal(graph, grid, from, to)` never picks a point by
geometry. Every candidate is a cell `Navigation::Grid` has proved reachable:

1. **the reachable cell in the room fewest hops from the target, strictly
   nearer than the player's room**; within that room, the fewest steps, i.e.
   its doorway. The whole collision window counts, not just the next room,
   so the choice does not depend on which of two rooms the player straddles;
2. **failing that**, only toward a nearer neighbour **with no collision
   loaded**: the reachable cell of the current room closest to it, since
   walking there is what makes the game load it. A loaded neighbour with no
   reachable cell is a known wall and is never approached;
3. **failing that, nothing**. In exploration mode the beacon then releases
   with "known route no longer available", or refuses the lock.

The grid is built on the **terrain layer**, the route's own (2026-09-25).
On the movement layer a monster's NO_PATH stamp blocks its cell, so a fight in
a doorway cut the player's region off. The sub-goal then vanished and the
beacon released (`known-route-unavailable`) mid-combat. This happened twice in
Dark Wood, 06:54:44 and 06:57:37.

`Beacon::Tracker::Tick` takes the sub-goal as an optional `waypoint`. Only the
route follows it; distance, arrival and ping cadence stay measured against the
real target. `beacon_service.cpp` supplies one only when the target is in a
different room *and* the grid cannot already reach it. For an unexplored room
the target is the room's centre, which may be rock; step 1 then leads to the
room's doorway, and arrival is stepping inside.

`Beacon::FindRoute` first searches a box sized by the straight-line distance.
A winding route leaves that box, so when it finds no complete route it
searches again in the widest box (`searchRadius`).

## Unmeasured

**How often the next room is outside the loaded collision.** Case 1 needs
collision; case 2 is the fallback. `[beacon] hop=… in_next_room=… room=X,Y
WxH tiles rooms=N` logs the case taken on every log tick.
