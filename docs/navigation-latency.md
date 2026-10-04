# Inventory latency and controller tabs

## Rune and gem stash browsing — 2026-10-04

The user's own 01:31–01:42 session identifies the rune controls as
`AdvancedStashSlotWidget`. Its 01:41–01:42 trace records menu rebuilds around
13–30 ms, with almost all that time inside permission queries; several presses
rebuilt twice in one tick. These are callback measurements. The asynchronous
log's write timestamps do not measure audible latency. Empty rune slots fell
back to internal names such as `r07` after the cursor's 450 ms fallback window.
The trace highlights Gems but does not open that tab; its shared widget type is
confirmed by both native mouse/controller bank layouts.

These slots inherit `InventoryItemWidget`, but the reader previously obtained
item pointers only for grids, equipment and belts. `NativeUi::ItemWidgetSlot`
now uses the same guarded virtual getter. Offline image evidence:
`AdvancedStashSlotWidget` metadata builder `0x2CE9B0` names
`InventoryItemWidget` as its base; constructor `0x2CE3F0` installs vtable
`0x1CF4678`; slot `+0xC8` points to `0x2CE900`, which returns the current item
at `+0x608` or null. No new direct member read or game write is required.

Every one of the 91 fixed slots in `BankExpansionLayoutHD.json` has an instance
name equal to its `itemCode` and a corresponding game localization key. Runes
use `item-runes.json`; gems use `item-names.json`. The interaction reader now
names that visible fixed identity in its first poll, including empty slots.
Only a successful empty native read appends Empty. A missing getter still
permits the visible name without declaring the slot empty. Properties and
quantities remain available through the existing tooltip detail/line commands.
Late matching tooltips do not repeat an already spoken name. A tooltip naming
another fixed slot stays rejected, including after the generic item's mismatch
timeout; the correct fixed name itself has no wait.

Item cursor movement clears the old tooltip capture with `Discard(now)`; the
former 90 ms item discard window is removed. Controls without an item identity
check retain their previous tooltip guard, which does not defer their labels.
These slot names also bypass the 20 ms empty
grid fallback. Menu refreshes reuse a single build within one UI tick while
still applying cursor/title state changes. The permission cache now shares
readable and executable flags within the existing synchronous scope, holds 256
regions, and replaces old regions when full. It never caches game contents or
permissions between UI updates. SEH still contains changed permissions and
faulting native getters. PRISM and logging keep their independent workers.

Regression coverage exercises immediate rune/gem names, empty/unavailable slots,
late detail without duplicate speech, mismatched tooltip rejection, immediate
next-draw capture, and read/execute permission distinctions and invalidation.
A 96-region fixture proves that repeated widget/getter reads in one scope make
no further Windows queries. Build/test/deployment records are under
`../../re/stash-latency/`; current deployment and live-check status are in
STATUS and HANDOFF. Audible latency remains a user-led game check.

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
`GEMINI.md`; tab announcements still await confirmation. The inventory itself
need not contain tabs: the observed controller strip belongs to its overlay.
