# Detours: unexplored ground reached through the neighbouring area

A tester, 2026-11-24 (1.0.23): come into the Flayer Jungle on the north bank of its river,
the scanner listed two areas and no unexplored one. The bridge was a few steps back, in the area before.

## What the log shows

- `19:09:14` level 78 (Flayer Jungle) come into from 76 (Spider Forest; the report says Great Marsh)
  at 5320,4567; 11, then 21 of 192 rooms seen.
- `19:09:54` `[survey] level 78: 4 unexplored rooms with no way in from 5368,4566 left out: 154 163 172 179`.
  True of that level alone: its ground is shut all round that bank.
- `19:12:18` come into again at 5320,4620, the south bank: 0 left out, and the four rooms listed.

The level's map was right and the question too narrow: the way was through the other level.

## The rule

`LevelRoute::Detours` (`level_detour.*`) joins the kept maps of the level and of one neighbour
(`Memory::Join`; every level of an act has the same coordinates) and searches both at once from the
player. A room cut off on foot (`LevelRoute::Reach`) that this search gets to has a detour: where to
leave, and where to come back in.

- Neighbours: the levels `LevelAreas::Neighbours` gives whose maps are kept, that is, walked this
  game. Their ground loaded from beside the border is learnt too (`Memory::LearnBeside`).
- Ground of the neighbour counts only once shown. While **most of the level was never shown** (under
  half of its rooms) and no detour is known, ground not shown yet counts too, as a guess. A finished
  level's walled-in rooms would otherwise be listed as ways through any unseen neighbour.
- With no unexplored room left in reach of such a level, and none at its edge with a detour, the
  nearest of all its unexplored rooms that has one is listed.
- One neighbour at a time: a way through two other levels is not found.

## What the player gets

1. The room is listed as "unexplored area 179, through Spider Forest", at the crossing to leave by.
   Guidance, the route and Shift+Home lead there; Shift+Home walks across. H takes the nearest one
   once nothing else is left to explore.
2. 2.5 s after arriving on the other side (past the banner) the way on is said: "unexplored area
   179, Flayer Jungle, 14 units, south". It is offered to guidance and walks unless the player
   scanned something first, and is listed with the unexplored areas there.
3. Once its room is got to on foot the choice is forgotten.

A detour's crossing is an exit of category `UnexploredRoom` (`DetourService::IsDetour`), which
`ZoneTransitions::Crosses` leaves alone: other borders are moved to their nearest crossing, the one
that leads nowhere here.

## Cost and evidence

A search over two 64x192-tile levels, none of it seen: 21.5 ms, plus 2.5 ms for the ways to all 192
rooms (measured offline, 2026-11-24). It is kept while the player stands on the same cell and
neither map changes, and made only when the scanner or H is asked and a room is cut off.

`level-route-tests`: a river across two levels with the bridge in the neighbour, both legs, the
guess, no bridge, another game's map. Not run live. Log: `[survey] level L: N unexplored rooms by way
of a neighbouring level`, `[survey] detour onward`, `[keyboard] explore: room N by way of level L`.
