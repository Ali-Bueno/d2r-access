# Inventory latency, equipment and NPC actions

Target: native D2R 3.3.93787 / D2RLoader, 2026-09-16.

The user reports both cursor stalls and delayed speech when shopping, repairing
or using the stash, intermittently missing equipment locations/occupancy, and
an undiscoverable blacksmith repair action.

## Placement orientation follow-up

### Repeated instruction while choosing equipment (2026-09-22)

The user reports repeated "Choose a space or a compatible equipment slot" before
the destination is read. This phrase is the mod's `itemInHand` string. The retained
log contains only the latest title-screen probe, so no live repetition trace is
available. Code inspection found `CursorNotice` reset its held state on every
non-inventory focus; returning from a transient empty focus or another control
then treated the same carried item as a new pickup.

Held state now follows the SDK cursor result across focus changes. While an item
is held, non-inventory polls can clear a confirmed release silently; unknown reads
preserve the previous state. Conversation/session resets still clear it. Manual
repeat adds placement context only while inventory focus owns the reader.
Equipment destinations also bypass the wait for a generated item name while
carrying an item: slot and known occupancy read immediately, and a rendered item
description can follow. Ordinary item browsing retains its concise name behavior.

Four related CTest suites pass, covering pickup once, empty focus, unrelated
controls, cursor-read failures, occupied magic destinations without tooltips,
release outside inventory and a subsequent pickup. Controller listening remains
pending. The changes do not alter game input or item placement.

### Initial implementation (2026-09-16)

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

## Socketing

### The problem (live log, 2026-09-28)

A Topacio fragmentado was put into a rare Mandoble ("Filo de la tormenta").

- **The sword was never named.** While the gem was in hand, the sword's cells logged
  `[item] cell … unit=…C040 -> ""` and nothing was spoken. The game renders no item tooltip while
  the hand holds an item, and a generated name (magic and better) waited for one.
- **The feedback was wrong.** The socketing was announced as a plain drop: `placement -> "Colocado
  en Fila 2, columna 1"`.

### Naming the item under the hand

A generated name now comes from the tooltip last seen for that runtime id. `ItemHand::Names` keeps
it, and trusts it only while the item's table name and quality still match. Without a remembered
name it falls back to the table name plus quality ("Mandoble. raro"). Equipment slots benefit too:
a worn item under the hand is now named instead of "Ocupado".

### What the hand did

`ItemHand::Resolve` works from two readings:

- the runtime id in hand, read through the SDK's `getCursorItem`/`getItemInfo`;
- the item the focus rested on while the hand was full, with its `socketCount`/`socketedItemCount`
  from `forEachInventoryItem`. `runtimeId` is the native unit id at `+0x08`, per the SDK README.

The outcomes:

| What happened | Outcome | Announcement |
|---|---|---|
| Hand emptied onto an item that stayed there and gained a filled socket | Socketed | "Engarzado en X", plus the socket state |
| Hand emptied onto an item that stayed there and whose quantity grew (a scroll into a tome, a stack) | Merged | "Añadido a X" |
| Hand still full, but holding a different item (the game handed the target back) | Swapped | "Intercambiado. En mano: X" |
| Hand emptied by the drop button (X) with no such evidence | Thrown | "Tirado al suelo" |
| Hand emptied anywhere else | Placed | the old "Colocado en …" |

Each non-placement outcome logs `[item] hand outcome=… sockets=a/b->c/d quantity=m->n`. Tests:

- `tests/item_hand_tests.cpp`;
- the socketing scenario in `tests/interaction_reader_tests.cpp`.

### Second live session (2026-09-28): the target had no sockets

The game swapping instead of socketing was the game's own rule. Every attempt was on "de electrocución
Mandoble", and the log gave `sockets=0/0` for it. Its tooltip has no "Con huecos (N)" line, only
"Max Sockets: 2". That is the RT "Show Ranges" line: the most sockets the base can be given, not the
ones it has.

That session also showed a gem thrown with X over the sword announced as "Añadido a" the sword.
Merged now needs its evidence (the target's quantity grows), and an X press is Thrown.

To stop the guessing, with a gem, rune or jewel in hand every item under it says "Sin huecos",
"Huecos libres: N" or "Huecos llenos". Whether the held item is one of those comes from the game's
own `ItemTypes.txt`: `gem`, `rune` and `jewl` all have `sock` ("Socket Filler") as parent, read
through the same type-ancestry walk the ground-item sounds use (`NativeGroundItems::SocketFiller`).

Confirmed live 2026-09-28: D2R swaps when a gem is put on an item with no free socket.

### Third live session (2026-09-28): the SDK's filled-socket count is wrong

The rare "Filo de la tormenta" was read as `sockets=0/1` (one socket, none filled), and the reader said
"Huecos libres: 1". But the game refused a Rubí twice:

- on the equipped sword, the character said "No puedo";
- in the backpack, the game swapped the gem for the sword.

The sword's tooltip lists "Añade 1-8 p. de daño de rayos", exactly the Topacio fragmentado weapon
bonus, and that topaz had left the hand onto the sword in the first session. So the socket is filled,
and the SDK's `socketedItemCount` read 0. The character (level 16) meets the Rubí's level 12.

The reader no longer trusts that count:

- **Hovering with a gem, rune or jewel in hand:** says the total ("Con huecos: N", like the game's
  "Con huecos (N)") or "Sin huecos". It never says how many are free.
- **Socketed:** the hand holding a socket filler emptied onto another item with sockets that stayed
  under the focus, without the drop button.
- **Refused:** the game handed that item back for the gem, or answered "No puedo" on an equipment
  slot. Both say "No se pudo engarzar" (`socketRefused`); the slot refusal is no longer called a
  failed equip.

The filled count now comes from the item's own inventory; see the next section.

### Socket contents

Recovered 2026-09-28 from `re/D2R.rebuilt.exe` (D2R 3.3.93787) and confirmed live on the running game
(loader 1.3.1-beta).

An item's socketed gems, runes and jewels are the items of its own inventory:

| Field | Where | Evidence |
|---|---|---|
| Item's inventory | `Unit+0x90` (or `+0x98` while the `+0xA0` owner reference counts), magic `0x01020304` | getter `0x34A360` (already fingerprinted for corpses) |
| Owner unit / owner id | `Inv+0x08` / `Inv+0x48` | socket insert `0x3856E0`; link-id getter `0x388BA0` |
| Item count | `Inv+0x4C` (dword) | `0x3856E0` reads it as the socket cell and increments it; remove `0x38BA30` decrements it |
| First / last item | `Inv+0x10` / `Inv+0x18` | first-item getter `0x388C10`; append `0x38B2A0` |
| Next item | `ItemData+0xB0` (ItemData = `Unit+0x10`); `+0xA0` parent inventory, `+0xA8` previous | next-item getter `0x38ABA0` |

Live readings:

- **Filo de la tormenta** (one socket, holding a chipped topaz): inventory count 1. The first item is a
  class-577 unit in mode 6, a new unit id and not the chipped topaz still in the bag.
- **Controls:** potions, loose gems and the key have no inventory. Rings, amulets, armour and a magic
  sword have an inventory with count 0.
- **The player's own inventory:** count 26, and walking the list gives exactly 26 items.

Having an inventory does not mean the item has sockets, so the total still comes from the SDK's
`socketCount`.

`NativeEntities::FilledSockets` reads the count after checking three things: the magic, that `Inv+8`
is the item itself, and that `Inv+0x48` is its id. A count above 6 (the most `gemsockets` allows) is
rejected. It is gated by `EntityLayout::SocketChecks`:

- `0x34A360`, `0x388BA0`, `0x388C10` and `0x3856E0`;
- hashes reproduced offline and live.

Do not fingerprint `0x38BA30`: the loader rewrites one of its calls in memory.

When the count is unverified or unreadable, the reader says only the total ("Con huecos: N").

## Arrivals

### The problem (live log, 2026-09-28)

The user: sometimes a bought or taken-off item "is not in the inventory" until the panel is closed and
reopened. The log showed no stale read: every grid cell resolved live. What happened instead:

- **09:08:02:** the "de electrocución Mandoble" was bought with the focus on the vendor, and nothing
  was said.
- **09:08:10:** a ruby was picked up.
- **Until 09:08:27:** columns 1, 2 and 4 were searched with the ruby in hand, while the sword lay in
  column 3.
- **After that:** closing the panel put the ruby back, and the sword was found after reopening.

Four purchases at 09:22 were silent too, and potions can go straight into the belt, where the grid
never shows them.

### What it says now

While `PlayerInventoryExpansionLayout` is open, `InventoryArrivalService` samples the player's
backpack, belt and worn items through the SDK's `forEachInventoryItem`, every 100 ms. The item info
gives `container`, `x`/`y` (top-left cell, per the SDK README) and `runtimeId`.

`InventoryArrivals::Diff` reports an item newly in the backpack or belt, or one that moved there from
the worn slots. It skips the item that was in hand, because "Colocado en …" already covers it, and it
skips moves within the backpack, such as sorting. What it reports is spoken without interrupting:

- "Nuevo en el inventario: X, Fila r, columna c" (or "…, Cinturón");
- "Desequipado: X, Fila r, columna c".

Names come from `InteractionReader::ItemName`: the tooltip name the reader remembered for that runtime
id (a vendor item keeps its id when bought), else the table name and quality.

Log: `[items] arrived id=… place=… at=x,y unequipped=…`. Console: `access-arrivals on|off`. Config:
`[inventory_arrivals] enabled`. Tests: `tests/inventory_arrivals_tests.cpp`.
