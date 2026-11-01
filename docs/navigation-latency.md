# Inventory latency and controller tabs

## Evidence (2026-09-15)

The user reported intermittent delays after the initial polling/cache optimization.
The 11:12–11:22 session contains complete accessibility callbacks lasting
719–1000 ms. Several start with an `[interaction]` line and end with the slow-tick
warning; idle longest-gap reports alone also include work outside the plugin.

The live plugin context resolved SDK `logInfo` to D2RCore 1.2.1-beta RVA `0x3CF110`.
Disassembly follows it to `0x3D14F0`, which opens the plugin log on every call,
writes the line at `0x3D1714`, calls `FlushFileBuffers` at `0x3D171D` (IAT slot
`0x5125C0`), and closes the handle. These were synchronous calls on the UI thread,
before `ScreenReader::Speak` could enqueue its text. The forced flush is verified;
whether it explains every reported pause still requires the next in-game trace.

`AsyncLog` now queues runtime diagnostics to a separate worker that calls the same
SDK logger, outside the producer mutex. It is independent of the PRISM worker, so
disk logging cannot block speech delivery there either. Its pending queue is capped
at 256 messages; saturation drops older diagnostics, and shutdown discards the
remaining backlog before joining the worker. SDK log timestamps reflect write time,
so navigation timing should use the durations recorded inside `[pump]` messages.

## A slow machine's log (2026-10-12)

A tester's wife, hosting a multiplayer game, logged 3651 updates over 30 ms in 70 minutes, about
one a second, 3303 led by `MapSurveyService` (25-53 ms) and each paying 46-63 page checks for
8-14 ms: one `VirtualQuery` took 0.2-0.3 ms there, against microseconds here (same build, no slow
update in the user's log). `InputDirection` took 5-8 ms, `GameTextReader` and `KeyboardService`
3-7 ms. Three changes, none verified on her machine yet:

- `NativeMemory::ReadScope` keeps the regions it found readable for a second (`RegionCacheMs`)
  instead of forgetting them at the end of each update; contents are never cached, a fault in a
  copy clears the regions, and SEH still guards every copy.
- `InputDirection::Poll` asks an empty XInput slot every 2 s (`EmptySlotProbeMs`), Microsoft's
  guidance for that slow call, not on every update.
- The scanner's catalogue pass (`MapSurveyService::Tick`) skips menus, dialogue and pause, and
  while the player stands within one subtile of the last pass it runs every 2 s instead of 0.5 s.
  A pass of 30 ms or more logs its parts at most every 5 s: `[survey] slow pass N ms: world,
  entities, grid, catalogue`. A question to the scanner always reads the world afresh.

## Tabs outside controller focus

The same session showed `BankPanelMessage:SelectTab` while focus remained in
`PlayerInventoryExpansionLayout`. The previous item reader only searched that
focused panel for tabs, missing the stash's separate panel.

A live read also found the main menu tabs under
`ControllerOverlay/TopTabsContainer/D2RLoaderControllerTabViewport/UISwitcherTabs`.
The selected label was `Inventario` at index 3 (`+0x16C4`) while the inherited
FocusableWidget selection (`+0x544`) was 2. This corroborates the original tab input
handler derivation (`0x878D30`): the two selection fields are not interchangeable.
The visible `TextN` children also contained `Grupo`, `Reg. de búsquedas`, `Personaje`,
`Árbol de hab.`, `Mercenario`, and `Opciones`. These paths are evidence, not reader
special cases.

`TabReader` discovers the shared `TabBarWidget` archetype across visible panels,
including controls initially hidden inside them. It reads each control's actual
visibility and selected text on subsequent polls. It announces only changed labels,
including when focus remains in the other inventory. Inventory context consumes
the same tab state; it no longer makes a duplicate tree walk. A tab change accepts
the current cell as the new baseline so delayed coordinate speech does not replace
the tab name. Dialogues, feature disable and session changes reset tracking.

## Validation

Nine CTest suites / 219 checks passed. New coverage includes a deliberately blocked
SDK logger, bounded producer work and backlog, string ownership, severity/order,
shutdown/restart, unfocused stash tabs, controller overlay tabs, returning to a tab,
initially hidden sub-tabs, rebuilt panels, visibility and dialogue suppression.

In game, open inventory with the normal controller shortcut, navigate with the
D-pad, then use LT/RT (L2/R2) to move between its top tabs. At the stash, repeat with
the selection still in the player inventory. Listen for the changed tab once and
normal cell readout on the next directional movement. Compare `[tabs]` and
`[interaction]` with `[pump] slow accessibility tick`. The user subsequently
confirmed smooth inventory navigation on 2026-09-15: “la navegación ya se siente
fluida por el inventario”. Only that performance checkbox is complete in
`GEMINI.md`; tab announcements still await confirmation. The inventory itself *Superseded 2026-11-01: testers finished the whole game, Acts I to V, with this in place.*
need not contain tabs: the observed controller strip belongs to its overlay.

## Keyboard build lag report (2026-10-05)

A tester of `d2r-access-teclado-2026-10-05.zip` reports lag while navigating
the inventory and shops, worse on cells with items. Read-only review of
`f6e33c5`; nothing is measured on the tester's machine yet. Suspects, most likely first:

The tester plays with the **controller**, so 1 and 2 do not apply to the
report (the keyboard cursor is idle until an arrow key is pressed with a pad
connected); they stay listed for keyboard play.

1. `KeyboardMenu::Refresh` rebuilds the whole menu on every arrow key and every
   250 ms (`Build`: widget walk, every grid cell, `Thin` O(n²)); a changed item
   layout also reruns `MenuCursor::Mend`, superlinear in the cell count.
   Each key logs `[cursor] read the menu again for a key in X ms`.
2. Item speech waits for the game's tooltip: `Land` discards tooltips for
   `SettleMs` (90 ms), and the fallback name comes at `SilenceMs` (450 ms).
   Empty cells speak at once.
3. Periodic inventory walks: arrivals (100 ms, while the inventory is open),
   weapon swap (100 ms), vitals (1 s); `HandNotice` walks every container
   each 33 ms poll while an item is held.
4. `[pump]` and `Laps` reported only updates of 30 ms or more.
5. Controller path: `NamesItem` holds an item's tooltip back up to
   `NamesItemMs` (400 ms) when its body lacks the item's base name, and only
   occupied cells wait for it. Logged as `held back a tooltip` / `taken after`.
   The occupied-cell readout is also recomputed on every 33 ms poll.

Done: `InventoryCensus::Log` (every cell, every 250 ms, diagnosis only) now runs
only with `access-itemlog on`. `Laps` also logs the worst update of every 5 s
from 8 ms (`[pump] worst update`). Next: the tester's `d2r-access.log`, looking
for `held back a tooltip`, `taken after` and `[pump] worst update`.
