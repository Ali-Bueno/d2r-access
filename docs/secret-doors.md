# Secret doors (2026-10-22)

A tester: in the Jail some rooms have no door; a wall panel must be found and clicked. A sighted
player sees a pattern in the wall and glints of the loot behind it; the mod's loops sounded the
loot, and its tracker led to items no one could reach. They asked for a cryptic hint, "You feel a
cool draft", rather than the door named outright. Screenshots and log: the user's Downloads,
`d2r access test build 2026-10-22 notes.7z` (`secret_*`).

## What the game has

`objects.txt` row `HiddenDoor2`, \*ID 129, "secret door 1", Name key `Door`:
- **OperateFn 18**, which no other row has (`EntityLayout::SecretDoorOperateFn`).
- IsDoor 0, so the mod's doors (`door_sounds.h`, `shutDoor`) never took it.
- Selectable only in mode 0: shut, it can be targeted; once opened it drops out of the units read.
- SizeX/Y 1×5; HasCollision, BlocksLight and BlocksVis set while shut.
- Placed by a preset: `LvlPrest.txt` "Act 1 - Jail NSEW" lists `JailNSEWSecret.ds1` as the fifth of
  its five files; the log loaded that piece in Jail Level 2 (origin 3536,1312 in tiles). Whether
  the Barracks or the Catacombs have one is unknown (no row of theirs names a secret file).

Before this build the shut panel was read as an interactable object named like a shut door, so
it was likely listed in the scanner (INFERRED: the log's survey lines never named it).

## What the mod does

- A shut secret door (`Entity::secretDoor`) is never listed: to the eye it is wall.
- Walking within 10 units of one says "You feel a cool draft." once; it is felt again after the
  player has been 20 units away, or when it is loaded anew (`SecretDoors::Draft`, read every
  500 ms with the doors). Log: `[items] secret door felt: N shut loaded, one at x,y`.
- Switch `secret-drafts` (Ctrl+S Sounds, on).
- Opening it is the game's own: standing at the panel, the target it offers is read as any other.

## Not known

The panel's unit position against the screenshot's arch, its collision bits (whether the route
planner already walks through it as through a door), and the label the game targets it with. The
first `[items] secret door felt` line with a `[interaction] World target` at the panel answers them.
