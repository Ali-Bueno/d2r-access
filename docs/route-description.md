# Route description (2026-10-13)

A blind tester found quest places hard to reach: a sighted player glances at the
minimap and plans a way there, which the beacon's ping does not give. The user
asked for that planning on the scanner, and explicitly **no autowalk**. This
reverses the 2026-09-20 rule "never a dictated step sequence" at the user's
request ([decisions](decisions.md)); it is asked for, never spoken on its own.

## Controls

- **BACK + right stick down** (pad), **Ctrl+E** (keyboard), console
  `access-beacon route`: the route to what the beacon follows, else to what the
  scanner named last on this level (the beacon's offer, any
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
length, then each leg as the way to walk it and its units. In the clock
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

`RouteLegs` (pure, `tests/route_legs_tests.cpp`) turns the cells into legs
the player walks with the stick or the arrows, which go eight ways only:

1. **Legs of the eight ways.** Each leg runs exactly one of the eight ways,
   so pushing the stick the way it is said walks it, to its end. A search over
   (cell, heading) finds them, a turn costing 3 units of walking (`TurnUnits`):
   of two ways nearly as long, the one with fewer legs. It keeps within 8 cells
   of the planner's own way (`CorridorCells`), over ground the way may cross
   with the whole body on it (the body's cross or square as the planner fits
   it; the cell a move starts from is not tested, as the game does not). A
   diagonal step also needs the body to fit on both cells beside it, as at a
   wall's corner: a doorway is then crossed along the world axis it runs on,
   never cut on the stick's north, east, south or west. Only where no such way
   exists may it cut a corner as the planner's own steps do, and failing that
   the planner's steps are the legs (each of them one of the eight ways too).
   The search is walked back from the end first, so it heads where the way goes
   and stays small; a way longer than 96 cells (`StretchCells`) is laid out a
   stretch at a time, each stretch's last leg laid out again with the next.
2. **Doors** split the way: a shut door is opened from any cell next to it
   (`DoorReach`), said as "door", and the leg after it walks through.
3. **The end** is anywhere the way counts as there, not the planner's last
   cell, so the last leg need not jog onto it: within the beacon's arrival
   distance round the place (`LevelRoute::Area`), on a cell walked to from the
   place within it (`LevelRoute::Arrival`, for the planner too). Before
   2026-10-19 any cell of the square counted: a route to the Den of Evil
   "arrived" 7 units from its mouth, which rocks close on three sides, and a
   thing just past a wall was reached from the wall's other side.
4. **Merge** neighbouring legs said in the same direction.

Before 2026-10-19 a leg was any straight line clear of walls, then said as its
nearest compass point: "east, 22" walked as the stick's east ran past the
Den of Evil's mouth, and "north, 18" left the player beside a Crypt doorway,
not in line with it (a tester, `docs/decisions.md`). The planner's zigzag over
open floor is now one leg along an axis and one along a diagonal, where it was
one straight leg.

Lengths are in units (collision subtiles), the scanner's own measure: a leg
along a diagonal of the stick covers 1.41 units a cell.

## Following it

Added 2026-10-14: the user reported players getting lost on a route. Once a
route is said, `RouteFollow::Follower` (pure, `tests/route_follow_tests.cpp`)
walks its legs with the player, polled every 250 ms:

- **At each turn**, half a second before it at the player's own pace
  (`TurnSeconds`; 3 units at a run, `TurnUnits`, at least 1), the leg to walk
  now: "Now east, 21". A shut door ahead is said with the leg past it: "Now
  door; then north, 16". A turn already walked past moves on to the leg the
  player is nearest. Before 2026-10-19 the turn was said 3 units before it at
  any pace: a step early at a walk, and a step early in a doorway is its frame.
  Never before half the leg is walked (2026-10-21): a leg of 3 units or less was "done" where
  it began, so the first prompt came at once and cut the route off. The tester's grave in Tower
  Cellar 4: "northwest, 3; then southwest, 4; ..." was heard as "Now southwest, 4", and walking it
  led into the grave. The route itself went round it each time (`[radar-collision]`: the grave's
  cells `0x0011`, blocked); the follower's skip was the cause, in the Den of Evil too.
- **Off the route**: further than 8 units (`OffRouteUnits`) from every leg
  still to walk for 2 seconds, it says "Off the route to X" once and stops.
  Asking again plans from where the player stands.
- **The end**: within 3 units of the way's end, or for a room anywhere inside it, as the
  beacon's (`InRoom`; 2026-10-21: a tester crossed into unexplored rooms away from the way's
  end cell and heard nothing, then "Off the route"). Without a beacon lock it says
  "Arrived at X" (speech kind Arrivals, held 1.5 s); a locked beacon says its own arrival.
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
- Not verified in the game. Log: `[route] from=x,y to=x,y cells=N doors=N *Superseded 2026-11-01: testers finished the whole game, Acts I to V, with this in place.*
  legs=N guess=0|1 restricted=0|1 -> <text>`, or `[route] none name=...`.

## The whole length optional; the ground logged (2026-10-20)

A tester: after Mod+A the whole length ("27 units") is not what a player needs before the legs.
Switch `route-total` (Ctrl+S Navigation, on): off, the route says "Route to X: east, 11; then ...".
The user, the same day: the legs ("east, 11; then ...") are what bothers them. Switch `route-legs`
(on): off, "Route to X, 27 units", or "Route to X" with both off (`RouteLegs::Wording`). The turn
prompts while walking keep their own switch, `route-prompts`.
Each route asked for also logs the raw collision 24 cells around the player
(`RadarTrace::Collision`, `[radar-collision]` lines as the radar's snapshots), so a leg the game
would not walk can be replayed from the log. The case that asked for it: a Tower Cellar doorway
whose lower half an open grave closes; the route led through it into the corner past the grave
(screenshots `edge_case_1/2`, no log yet).

## Doors that read as objects (2026-10-22)

A tester's log (`no_route_d2r-access.log`, the user's Downloads): in Jail Level 2, "No route
through explored areas to To The Jail Level 3" three times (`[route] none ... here=0 map=1
restricted=1`), from the room next to the stairs' room, then a route at once after the tester
opened a door by hand. The collision dumps show that door, three cells at 17875,6644-6646,
carrying OBJECT (0x0400) and no DOOR (0x0800) while shut, and nothing once open. The kept map
(`LevelRoute::Memory::Learn`) took OBJECT alone for a chest or a barrel, seen shut, so no way,
not even one opened as it goes, passed the door; the only way to the stairs went through it.

Now the survey hands the kept map every shut door unit it reads (`KeyboardService::NoteDoors`,
logged `[route] shut doors noted on level L: N`), and a cell shut by OBJECT alone within the
door's length of one is learnt as a door's: the route says "door" there and walks through.
OBJECT alone anywhere else stays shut. Barrels the tester broke on the way were in open floor and
not the cause.

## Turns at least 5 units apart; distances in steps (2026-10-22)

A tester: the stick does not walk exactly 1, 2 or 3 units, and routes, near doors above all,
turned after legs that short; they asked for a 5-unit minimum between turns behind a switch, to
learn whether it gets players stuck on geometry. `route-long-legs` (Ctrl+S Navigation, on):
`RouteLegs::Describe` with `MinLegUnits` folds each leg under 5 units, shortest first, into the
neighbour heading most its way (never across a door), which then runs straight from one's start
to the other's end; neighbours said the same way merge again. "north, 1; then northeast, 6"
becomes "northeast, 7" and ends where it did, a cell beside the doorway's line, which the wall
slide or the doorway's width has to absorb: that is the trade being tried. A leg with only doors
beside it stays. Off, the legs line up exactly, as before.

Rejected: a minimum inside the search (a state per cells walked since the last turn). An offset
of 1 to 3 cells needs a diagonal leg of exactly that many, so the search detoured instead: "north,
1; then northeast, 6" (7 units) became "east, 6; then northwest, 5; then northeast, 3" (14).

**Steps.** The same tester: say distances in steps, one step being one click of the walk
(`step-click-interval`, 5 units by default), so "10 steps" is 50 units. `distance-steps` (Ctrl+S
Navigation, off): `EntityRadar::SetUnitsPerStep` makes `DistancePhrase` and each leg's number count
steps of that interval, at least 1 ("1 step" for 2 units); "right here" is unchanged. It covers
every distance the mod says through `DistancePhrase` (scanner, tracker, route, NPCs, overview).
The radar test's "units clear" stays in units.

## Long legs never fold through a wall (2026-10-25)

With `route-long-legs`, a leg under 5 units was folded into a neighbour without looking at the
ground. In Asheara's hut two 3-unit legs round a pillar folded into one straight line through it,
"northwest, 1", and the wall slide refused it (an isolated pillar is no narrow passage). A fold is
now made only when its straight line, sampled every half cell, crosses ground cells only
(`RouteLegs::Lengthen`, the route's `crossable` ground: walls and objects excluded, the body not
required), else the next way is tried and then the legs are kept. The body may still brush a door
frame, as the 2026-10-22 rule meant. Tests: `route_legs_tests.cpp`, the pillar.
