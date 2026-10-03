# Inventory latency, equipment and NPC actions

Target: native D2R 3.3.93787 / D2RLoader, 2026-09-16.

The user reports both cursor stalls and delayed speech when shopping, repairing
or using the stash, intermittently missing equipment locations/occupancy, and
an undiscoverable blacksmith repair action.

## Placement orientation follow-up

The user subsequently confirmed improvement and reported that available equipment
slots are read while holding equipment. They prefer that navigation but initially
felt lost. The 18:23:59 log changes the native A action from Recoger to Colocar;
18:24:01 reads Arma derecha, Ocupado. This validates the need to describe the
placement context, rather than changing the game's focus routing.

`InventoryCursor::HoldingItem` reads the existing SDK inventory service:
`inventory.h` permits `getLocalPlayer` plus `getCursorItem` on the UI thread.
Handles are reacquired each poll and are never treated as native pointers.
Missing services, player failures and transient cursor errors are unknown state;
they never trigger a false release announcement.

The shared reader announces “Objeto en mano. Elige una casilla o una ranura de
equipo compatible para colocarlo” when an item becomes held, and “Ya no sujetas
ningún objeto” when it is no longer held. It does not infer whether that item was
equipped, stored or dropped. Navigation within the held state stays quiet, while
manual repeat includes its orientation. An empty cell and a new mode notice are
combined so the following poll cannot interrupt the notice with coordinates.
Conversation/session resets clear the mode; reopening inventory while holding
an item restores the context. No focus, cursor or inventory mutation was added.

The full build and all 31 CTest suites now pass, including 15 SDK cursor checks
and 14 new interaction regressions. This supersedes the temporary full-build
failure recorded below. The mode announcement is installed; live NVDA validation
of this additional notice remains pending.

## Changes and evidence

- `NativeMemory::Copy` previously called `VirtualQuery` for every small field or
  string copy, including repeated reads of the same region during the 30 Hz UI
  pass. `ReadScope` caches only readable region permissions on the current thread
  until that pass returns. Actual contents are always copied, every copy remains
  SEH guarded, a fault clears the cache, and full caches fall back to fresh queries.
  Gameplay text and tooltip capture use separate scopes; no cache spans frames.
- The tooltip's 40 ms copy throttle now applies only to unchanged panel/widget/
  cell identities. The first render for a new selection is copied immediately.
  The game's own tooltip creation delay remains; this does not force rendering.
- Equipment names no longer wait for the next polling interval. Slot presence
  comes from the slot's own inventory getter, rather than assuming that a missing
  tooltip means empty. Confirmed empty slots reject leftover rendered descriptions.
  Occupied slots say so while awaiting their complete native item description.
- `NativeControllerHelp::ForFocus`, previously consumed only by skills, now feeds
  the shared inventory and button reader. It reads available controller actions
  even while an occupied grid cell is awaiting its item tooltip. Unchanged actions
  do not repeat on item movement; changed actions are spoken and manual repeat
  includes current help. Conversation ownership suppresses covered inventory help.
  Footer discovery caches names, resolving the actual widgets again each poll,
  so adding this read does not add two full tree walks per inventory poll.

Equipment source: captured `live-rdata.bin` maps InventorySlotWidget vtable
`0x1CF3F20 + 0xC8` to `0x2CA060`. Disassembly of `D2R_live.exe` at
`0x2CA060–0x2CA0A2` resolves the widget's owner via `0x2A7810`, loads its
location at `+0x5D8`, obtains the inventory through `0x34A360`, and tail-calls
`0x3886D0` with that location. It ignores the selection argument. The existing
grid override uses that same virtual slot with explicit cell coordinates.
The query is UI-thread only, archetype/visibility/executable checked and SEH
guarded; a failure is unknown, never empty. See [prior tooltip evidence](native-tooltips.md).

NPC review used the extracted stock `VendorPanelLayout.json`: native actions
include Buy, Sell, Repair, RepairAll and RefreshAll, each with its own tooltip
label. `Controller/VendorPanelLayoutHD.json` uses the shared controller UI;
`ControllerOverlayHD.json` owns a `ButtonLegendScrollWidget` footer. Its active
runtime records supply the actual buttons, captions and hold wording. We do not
hardcode a repair shortcut or force hidden mouse buttons into controller focus.
`Controller/ImbueItemsPanelHD.json` exposes the existing Confirm buttons with
native labels; NPC dialogue services still use the shared list-row reader.
Availability remains controlled by the NPC, quest and game state. The synthetic
repair/refresh button examples in tests do not establish a live controller binding.

## Validation

Plugin compiled and deployed with the game closed. Thirteen UI CTest suites /
458 checks pass, covering immediate equipment locations, occupied/empty transitions,
missing and delayed descriptions, capture during rapid movement, action changes,
footer rebuilds, cached discovery, dialogue suppression and native memory faults.
The 257-widget synthetic UI traversal averaged 3.325 ms without a scope and
0.094 ms with one over forty passes. Timing is reported, not asserted; this is
not a measurement of live FPS or screen-reader output latency.

The full `build.ps1` attempt also encountered incompatible radar tests in the
parallel radar edits (`Radar::Engine::CloseDepth` / removed `OpenDepth` in
`radar_passage_tests.cpp`). The plugin and all thirteen relevant suites were
then compiled as explicit CMake targets. No radar source or tests were changed
for this task.

Pending user-led live check: navigate rapidly through inventory, a vendor and
the stash; cross head/neck/weapon slots with and without an item held; remove
and equip an item; confirm the spoken repair action in the blacksmith's shop
and other services when available. Compare `[pump] slow accessibility tick`
with the previous session and `[interaction]` action/selection messages with
what NVDA actually reads. No purchases or quest services were exercised here.

## Scrolling legend fallback — 2026-09-17

The user reported that an item announces only its primary action ("equip") and
never repair, identify, sell or buy. The layouts explain where those live: on
controller, `Controller/VendorPanelLayoutHD.json` **drops** the mouse panel's
`button_buy`/`button_sell`/`button_repair`/`button_repair_all` widgets
entirely, and every per-item action instead becomes an entry of the global
`ControllerOverlayHD.json` footer — a `ButtonLegendScrollWidget` named
`Legend` whose layout declares eight sub-text slots `Legend1`..`Legend8`
(`dataUpdateTime` 0.2). The mouse path uses `ContextMenu.json`'s `OptionsList`
(`RecyclingListWidget`) instead; its rows carry no text in the JSON either.

`NativeControllerHelp::Legend` read only the entry vector at `+0x88` whose
RVAs (`0x874270`/`0x873B00`/`0x87B080`) were established for
`ButtonLegendWidget`. `ButtonLegendScrollWidget` is a different class and need
not share that layout, which would leave the overlay footer contributing
nothing — exactly the reported symptom, since the focused panel's own plain
legend still supplies the single primary action.

`Legend` now falls back to the widget's declared sub-text slots when the vector
read yields nothing: children named after the legend itself plus an index
(`Legend1`, `Legend2`, …), walked until one is missing, read with the existing
`ReadText` and passed through `ControllerText::Read` for the glyphs. No new
offset, and the fallback cannot fire while the vector read works.

Still to confirm live: at the blacksmith, whether the spoken help now lists
Buy/Sell/Repair; the `[interaction]` log line records whatever is read.

## Upstream hand/socket/arrival additions — 2026-10-03

Hand-ported `12f0df3` into the current tooltip and keyboard reader. Generated
names learned from a matching tooltip are reused while a held item suppresses
tooltips. Specific stack/tome merges, swaps, drops and confirmed socketing
are queued after focus speech. Plain pickup and release remain silent, as
previously requested on this branch. Existing requirement refusal, tooltip
identity guards, autodetail, keyboard grid and physical-pad behaviour stay in
place.

Socket totals come from the SDK; filled counts come from the item's verified
native inventory. The SDK's `socketedItemCount` was reported as zero for filled
sockets upstream. Invalid ownership/count reads remain unknown and never
claim a successful socket. Socketing requires a one-socket increase on the
same item with unchanged total sockets; unchanged counts also stay quiet.
These are reads, not equipment-stat writes.

`InventoryArrivalService` polls only while the inventory is open, naming a
bought or unequipped item and its native grid cell or belt destination.
`access-arrivals on|off` and `[inventory_arrivals] enabled` control it.
Pure outcome and arrival tests run without the game; live feedback is pending.
