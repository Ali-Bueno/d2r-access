# Route description (2026-10-13)

A blind tester found quest places hard to reach: a sighted player glances at the
minimap and plans a way there, which the beacon's ping does not give. The user
asked for that planning on the scanner, and explicitly **no autowalk**. This
reverses the 2026-09-20 rule "never a dictated step sequence" at the user's
request ([decisions](decisions.md)); it is asked for, never spoken on its own.

## Controls

- **BACK + right stick down** (pad), **Ctrl+E** (keyboard), console
  `access-beacon route`: the route to what the beacon follows, else to what the
  scanner or the exit finder named last on this level (the beacon's offer, any
  age). Every press plans again from where the player stands, so asking on the
  way gives the rest of it, and to where the target stands now: what the
  scanner named is read again first (`Refresh`), as the beacon does for its
  lock. Until 2026-10-19 the route went to where the scanner had seen it, so a
  route to another player led to where that player had been (a tester: the ping
  said "11 units northeast", the route "19 units west").
- With BACK held, a right-stick push never reaches the scanner or a menu
  (`InputDirection::TakeBackWithRightStick`); only *down* is bound so far.

## What is said

"Route to Akara, 85 units: north, 20; then door; then east, 30" — the whole
length, then each straight leg as a direction and its units. In the clock
style the legs are hours ("12 o'clock, 20"). After six legs the rest is
counted ("then 3 more legs"). A way that crosses ground not seen yet ends with
"Part of it crosses ground not seen yet". Standing at it: "Akara, right here".
No way: exploration says "No route through explored areas to X" (as the
beacon's refusal), full assistance "No known way to X".

A leg's compass point is the way to walk it, as every direction the mod says
([spoken directions](radar-readings.md#spoken-directions-2026-10-13)): the
stick's and the arrows' diagonals are the world axes, so a doorway is always
crossed northeast, southeast, southwest or northwest. On the screen's 2:1
projection, used until 2026-10-19, a leg a little off an axis was "east" or
"west" and walked into the door frame (a tester).

## How

`BeaconService::DescribeRoute` plans exactly as guidance does
([walkable guidance](beacon.md#walkable-guidance)): `LevelRoute` over the
remembered map, past shut doors, whole map for other players and unseen town
NPCs. In exploration mode it prefers explored rooms (the same `ExploredFor`,
which always admits the target's own room: what the scanner lists can be
routed to); when they hold no way it guesses across rooms never stood in, as
ground not seen yet, and says so. A room target ends at the way's first step
inside the room (`LevelRoute::Area::Of`): its centre can be wall, and that was
why a route to the unexplored room next door failed (2026-10-14).

`RouteLegs` (pure, `tests/route_legs_tests.cpp`) turns the cells into legs:

1. **Straighten.** From each corner, as far along the way as a clear straight
   line goes over the way's own ground with the whole body on it (half-subtile
   samples, the body's cross or square as the planner fits it; the cell a move
   starts from is not tested, as the game does not). The search's cell-by-cell
   zigzag over open floor becomes one straight leg; corridors keep their
   corners, and a line can no longer cut a door frame's corner.
2. **Doors** split the way: the first cell of each shut door ends a leg and is
   said as "door".
3. **Merge** neighbouring legs said in the same direction, **fold** a leg
   under 3 units (`MinLegUnits`, about the body's width: a jog round a corner)
   into the one before it, else the one after, and merge again. A leg is
   folded only when the leg it joins still runs straight over clear ground;
   the step through a doorway does not, so it stays a leg of its own
   ("northeast, 10; then southeast, 2; then northeast, 10"). Before 2026-10-19
   it was folded and merged away, and the walk went on along the wall.

Lengths are straight lines in units (collision subtiles), the scanner's own
measure.

## Following it

Added 2026-10-14: the user reported players getting lost on a route. Once a
route is said, `RouteFollow::Follower` (pure, `tests/route_follow_tests.cpp`)
walks its legs with the player, polled every 250 ms:

- **At each turn** (3 units before it, `TurnUnits`), the leg to walk now:
  "Now east, 21". A shut door ahead is said with the leg past it: "Now door;
  then north, 16". A turn already walked past moves on to the leg the player
  is nearest.
- **Off the route**: further than 8 units (`OffRouteUnits`) from every leg
  still to walk for 2 seconds, it says "Off the route to X" once and stops.
  Asking again plans from where the player stands.
- **The end**: within 3 units of the way's end. Without a beacon lock it says
  "Arrived at X"; a locked beacon says its own arrival.
- **A target that walks** (a player, an NPC, a monster) is read again every
  second (`MovedCheckMs`); once it stands more than 5 units (`MovedUnits`) from
  where the way was planned to, the way is planned again from where the player
  stands. The leg to walk is said ("Now northeast, 11") only when it is not
  the one being walked; reaching the target says the arrival. Log:
  `[route] follow moved name=X to=x,y legs=N -> <text>|(same leg)|here`.
- It stops with the level or game, when its locked target is released, and
  with the `route-prompts` switch (Ctrl+S, Navigation, on by default). It never
  walks and never pings.

Log: `[route] follow turn|off|end leg=N at=x,y name=X -> <text>`,
`[route] follow stopped name=X`.

## Limits

- One level only: a quest in another area is reached exit by exit, each exit
  being a scanner target of its own.
- Teleport pads are not crossed (the walking plan does not use them).
- Not verified in the game. Log: `[route] from=x,y to=x,y cells=N doors=N
  legs=N guess=0|1 restricted=0|1 -> <text>`, or `[route] none name=...`.
