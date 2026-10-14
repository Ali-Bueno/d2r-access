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
  way gives the rest of it.
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

## How

`BeaconService::DescribeRoute` plans exactly as guidance does
([walkable guidance](beacon.md#walkable-guidance)): `LevelRoute` over the
remembered map, past shut doors, over explored rooms only in exploration mode
(the same `ExploredFor`), whole map for other players and unseen town NPCs. A
room target ends as soon as the way steps into the largest square centred in
the room.

`RouteLegs` (pure, `tests/route_legs_tests.cpp`) turns the cells into legs:

1. **Straighten.** From each corner, as far along the way as a clear straight
   line goes over the way's own ground (half-subtile samples). The search's
   cell-by-cell zigzag over open floor becomes one straight leg; corridors keep
   their corners.
2. **Doors** split the way: the first cell of each shut door ends a leg and is
   said as "door".
3. **Merge** neighbouring legs said in the same direction, **fold** a leg
   under 3 units (`MinLegUnits`, about the body's width: a jog round a corner)
   into the one before it, or after it at the start, and merge again.

Lengths are straight lines in units (collision subtiles), the scanner's own
measure.

## Limits

- One level only: a quest in another area is reached exit by exit, each exit
  being a scanner target of its own.
- Teleport pads are not crossed (the walking plan does not use them).
- Not verified in the game. Log: `[route] from=x,y to=x,y cells=N doors=N
  legs=N guess=0|1 restricted=0|1 -> <text>`, or `[route] none name=...`.
