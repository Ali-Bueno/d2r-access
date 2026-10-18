# The waypoint panel (2026-10-17)

A tester (2026-10-15/16, with screenshots and a log of gold, grey and blue rows): each row should
say whether it can be travelled to. A row reads "<area>", "<area>, undiscovered" (grey: never
activated), "<area>, you are here" (blue: the player's area) and, while the area is a terror zone,
"..., Terror Zone" (purple, with the skull).

## What the rows do not tell

All nine rows are `ButtonWidget`s cloned from `UnselectableButtonTemplate`, focusable and enabled;
their only child is the `TerrorZone` image. A 0x200-byte dump of every row (gold and grey, the
tester's log of 2026-10-17) differs only in pointers and names. The previous heuristic (a row
without a `SelectableButton`/`CurrentButton` child is locked) read every row as locked: those are
templates the panel copies images from, never children.

## The game's row table

Read in `re/D2R.rebuilt.exe` (D2R 3.3.93787), not yet measured in the game:

| Function | RVA | Role |
|---|---|---|
| FillRows | 0x1EA9B0 (1077 bytes) | clears nine `{int level, byte open}` entries at **0x2A969E0** and fills them, count at **0x2A969D4**, for the act tab it is given: `open` = activated and not the player's level |
| StyleRows | 0x1EC430 (1006 bytes) | row *i* from entry *i*: blue when the level is the player's (`0x34B440` → `0x2EFC10`), gold when `open`, grey otherwise |
| SelectTab | 0x1EC820 (723 bytes) | stores the tab at **panel+0x168**, refills the table |
| Click | 0x1EBF50 | the message payload is the row index; the level comes from the table |

The rows are named `Act<A> Button<i>`; the table is the tab's only when `panel+0x168 == A-1`.
The player's level is read the radar's way (player room → RoomEx+0x90 → Level+0x1F8).
`NativeWaypoints` verifies the three functions (they hold the globals unmasked) and is the
`NativeUi::FocusState` reader, so the words reach every readout through `UiText::Node::state`.

Also found: the client's 16-byte waypoint flags behind the pointer at 0x2A969C8 (copied from the
open-panel packet, `0x1EC1A0`), and the panel's colours at +0x18C/+0x19C/+0x1AC/+0x1BC. Not used.

## Terror zones

The `TerrorZone` child is shown (visible byte +0x51) only for a level in the active terror zone
list with the feature on; hidden subtrees never reach the snapshot, so its presence under the
focused row is the test (`ui_text.cpp`). Its frame at +0x90 is 0 open, 1 current, 2 locked.

## Live check

`[waypoint] row Act1 Button3 -> open (tab=0 rows=9 level=5 open=1 player=1)`, once per row focused.
Expected: `rows` 9 in Act I, `level` the Levels.txt id of the row's area, `open` 0 on grey rows,
`player` the area the player stands in. `[waypoint] Verified.` at start.

## The row words were never said (build 2026-10-17)

The tester's log had every row right (`-> here`, `-> undiscovered`) yet only names were spoken:
the button readout snapshots the button alone, so the focus-state hook got the button's name
("Act1 Button0") and the button as the panel, and `FocusWords` and `TerrorZone` both test for
"WaypointsPanel". `NativeUi::Snapshot` takes the owning panel (`named`) for its name and hook now.
Emote words after choosing a row ("GG, button") came from the always-open `EmoteWheelPanel`: the
focus tracker's geometry fallback skips open panels that draw nothing (`NativeUi::DrawsNothing`).
