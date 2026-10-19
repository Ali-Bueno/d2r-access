# Controller potion assignments

Read the belt through its normal inventory focus. Each cell announces the
game's belt-slot help (including the D-pad direction), its row, its item and
the total number of physical items in that column. Row one is the bottom
row used by the controller; higher rows hold reserves for the same direction.
Columns one to four are also the keyboard's belt keys 1 to 4. An occupied
cell's item is named by the game's tooltip, which is drawn at once; the button
and row lead it and the count follows (`src/belt_reader.cpp`).
The count includes mixed potion types and does not claim they are identical.
Placing an item announces its destination direction and row ("Placed in ...,
Row 1"), as the other inventory sections announce theirs. That and "Item in
hand" share one switch in the Ctrl+S settings list (`hand-speech`), on by
default.

An empty bottom cell can display a translucent potion from the inventory.
The reader uses the game's own selection query and identifies this as an
automatic inventory potion; it is excluded from the physical column count.
An unreadable reserve cell suppresses the count rather than guessing zero.
No input, potion consumption, assignment or inventory movement is performed.
View/Back receives no new binding. The user's planned mod settings menu will
use that button in a separate task; this feature lives only in the inventory.

## Native evidence

Build 3.3.93787, full mapped image captured read-only on 2026-09-23 at the
title screen. All addresses below are RVAs, image base `0x140000000`.

- `BeltWidget` vtable `0x1CE2FA0`, getter `+0xC8 -> 0x227440`, reads a
  selected slot's X value. Its inventory getter `0x388390` accepts slots
  0..15 (`0x3883F2`) and returns a unit pointer without moving it.
- The position routine `0x227010` computes column as slot modulo four and
  row as slot divided by four at `0x22708A..0x22709A`.
- `game-data/local/lng/strings/ui-controller.json` keys
  `ControllerHelpBelt1..4` carry the actual control glyphs U+E003, U+E001,
  U+E002, U+E004: left, up, down, right. Existing `ControllerText` decodes
  these in the selected locale; directions are not inferred from screen order.
- The draw path calls `0x77E10` before rendering controller fallback items.
  For an empty bottom slot it calls `0x14CF40`, then passes that result and
  the column to `0x2271F0(widget, column, mode, nullptr)` at `0x226757..68`.
  The returned inventory item is drawn with `controllerDefaultItemTransparency`.
  This is the same read-only selection path used by the reader.
- `native_belt.cpp` verifies all three native function bodies before calling
  them. A mismatch only removes automatic inventory-potion detail.

## Validation

The 2026-09-23 build passed all 67 CTest suites and was installed with its DLL
hash verified. Game startup completed with NVDA; the automatic-potion guards
initialize on first use, so startup alone does not validate that native call.

Native UI tests check explicit reserve reads (`NativeUi::BeltItemAt`), invalid
indices, failure as unknown and unchanged native selection. Interaction tests
cover button and row speech, empty cells, reserve rows, per-column counts,
placement, automatic inventory items and the tooltip-named item. Live controller
listening remains required, especially the translucent automatic potion case.
