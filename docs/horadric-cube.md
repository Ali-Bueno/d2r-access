# Horadric Cube

A tester's request, 2026-10-18. Code: `src/cube_state.*` (where the cube is, its name, the open hint),
`src/cube_notice.*` (the spoken news), `src/inventory_arrivals.*` (the pure diffs), the arrival service
(`src/inventory_arrival_service.cpp`, the 100 ms sample) and `src/interaction_reader.cpp` (the grid readout).

## Design

- **One diff.** The arrival service's sample now also reads `ItemContainer::Cube`. `Moves` names every item
  that changed between backpack, stash and cube and where it was, whatever button did it (X in the cube
  window, Y in a gem, rune or material tab). Cube-involved moves are said as "Moved <item> into the Cube",
  "... to Cube" (from a stash tab) and "... into inventory". Backpack to stash keeps its own notice
  (`StashNotice`, Ctrl+S), stash to backpack stays "New in inventory".
- **Groups.** A hold of Y moves three; items of one class and quality in one sample are one phrase. The game's
  item names are not pluralised (13 languages), so the count is **"Moved 3 Chipped Topaz to Cube"**
  (`itemCount`, `StashCount::Counted`; no "×", which screen readers say as "times"), not "three Chipped Topazes".
- **Transmute.** `NewInCube` lists cube items not seen before; with cube items used up in the same sample
  (`CubeConsumed`) they are the result: "New item in Cube: Flawed Topaz". New cube items with nothing used
  up are a stack the stash split a piece off and are said as a quick move. Only while a cube is on screen
  (`CubeState::Up`); the baseline resets when that changes, so the cube's first sight is no news.
- **Cube opened.** `CubeNotice::Tick` says "Cube opened" (cutting in, `ScreenReader::Urge`) as
  `HoradricCubeLayout` enters `PanelState`. The window is named "Horadric Cube" by `Title()` (the game's item
  name); before, any unlisted panel read as "Inventory".
- **Naming the grid.** The cube's grid (the window's `grid`, the stash's `horadriccube_grid`) sets the readout
  body to the cube's name; the body changing is what makes the reader speak it. Back on a stash tab the body
  is the tab again, so the tab is read before the cell. With a carried cube shown in the stash the tab reads
  "Runes - Cube open"; with no cube, nothing is added and "closed" is never said.
- **Hold A to open.** Probably the game's own item line `OpenHoradricCubeController` ("Hold <A> to Open",
  misc.txt, ui-controller.json): the footer legend reader (`NativeControllerHelp`) reads only legend entries,
  and the tooltip path cleans the glyph away. The controls line of a focused cube now gets the game's string
  (`CubeState::WithOpenHint`) unless the legend already holds it. Not confirmed in the running game.

## The tester's first pass (build 2026-10-18-2)

Window changes, X both ways, Y into the cube and transmute results all heard as designed. Two gaps:
- **"Hold A to open" with the name.** The tooltip's line was read as "Hold to Open": its button is
  U+E013, beyond the 19 controls `ControllerText` knew, and the cleaner drops unknown glyphs. The
  code is now decoded (see [controller glyphs](controller-glyphs.md)) and the focused
  cube reads "Horadric Cube. Hold A to Open" (`CubeState::OpenHint`, appended unless the readout has
  it); `CubeState::NamedOpenHint` puts the button back into the tooltip's own line in the detail.
- **Y out of the cube into a specialty tab** said nothing: `InventoryArrivals::Departures` only
  watched the backpack and the stashes. It watches the cube too, so the stash's Y notice says
  "Chipped Topaz moved to the Gems"; a sample where a transmute left something new in the cube
  skips the cube's departures (those were its ingredients).

## Unknowns (need the running game)

- Whether `ItemContainer::Cube` lists the cube's contents while the cube is closed, and whether Y's split
  stacks get new runtime ids. Both paths are handled; the log tells which.
- Whether a stash cube "shown" (widget `advancedstash_horadriccube` visible) is what the tester calls open.
- Whether `HoradricCubeLayout` reaches `PanelState` when opened by a held A.
- Recipes that change an item in place give no "New item" (same id).

## Log lines to send

`[cube] stash carried=.. shown=..`, `[cube] open hint game=".." legend=".."`, `[cube] window opened`,
`[cube] into cube|quick move to cube|into inventory|result ...`, and `[items]` lines around a transmute.
