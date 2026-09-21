# Native target labels and inventory descriptions

Evidence: D2R 3.3.93787, live D2RLoader process and the existing `D2R_live.exe`
Ghidra project, 2026-09-15. Addresses below are executable RVAs. The user left
the character beside the stash with its label visible. A read-only memory probe
captured the exact text **`Tu alijo personal`** from tooltip record zero; record
one contained only the controller font glyph U+E013. No distance or world
position is inferred from this label.

## Transient tooltip data

`TooltipsPanel` has no widget children for its descriptions. Its runtime vtable
is `0x1D76AE8`; the type getter is `0x8811C0`, draw slot `+0x18` is `0x880550`.
That draw function walks a contiguous vector:

| Member | Offset / value | Evidence |
|---|---|---|
| Vector pointer / count | panel `+0x168` / `+0x170` | `0x880550` draw and cleanup loops |
| Record stride | `0x2E8` | same loops |
| Body / footer strings | record `+0` / `+0x28` | `0x8DA7E0`, cleanup in `0x880550` |
| Primary description | record zero | always selected in `0x880550`; later entries include comparisons and glyphs |
| Single-record renderer | `0x8DA7E0`, `void(record*)` | called after delay/visibility filtering |

Every frame destroys both strings and clears the count after rendering. A UI
poll can miss the entire lifetime. `native_tooltips.cpp` therefore installs one
SDK-owned inline hook on the single-record renderer, verifies its first 16
bytes, copies only a currently rendered primary record, and calls the stock
trampoline. It does not change the strings or the original draw logic.

The UI thread re-resolves the tooltip panel. The hook checks that the incoming
record matches its current first entry, bounds the vector, and copies strings
through guarded memory access. It copies at most every 40 ms, remembers the
last render time, and expires an unseen description after 180 ms. Controller
focus and its selection are copied alongside the text so an old cell's tooltip
cannot be attached to a newly selected cell. Copies are protected by a short
mutex; speech remains on the existing PRISM thread. Gameplay lifecycle,
`access-text off`, and conversation overlays suspend capture.

## Inventory and target archetypes

- `InventoryGridWidget`, `InventorySlotWidget`, and subclasses of
  `InventoryItemWidget` use one interaction reader. Item descriptions come
  from the stock tooltip body/footer. Other cells are never enumerated.
- Controller selection is the `FocusableWidget` pair of signed integers at
  `+0x544`/`+0x548`; `InventoryItemWidget` function `0x2A89C0` reads this pair
  and uses it to look up the selected item. Grid width/height at `+0x628` and
  `+0x62C` are used by `InventoryGridWidget::Update`, `0x2C5FA0`.
- `TabBarWidget` has selected index `+0x16C4` (input handler `0x878D30`), and
  actual label children `Text0` through `Text7`. The stash live tree had
  `Personal`, `Comp.`, `Gemas`, `Materiales`, `Runas`. Only the selected label
  is announced. These are runtime values, not assumed tab order.
- Storage/player inventory titles use game localization keys `stash` and
  `minipanelinv`; equipment and belt transitions use localized plugin phrases.
- `MonsterHealthPanel` provides a second, visible-tree target-name source,
  including its Name/Unique/Additional text boxes. It is suppressed while a
  different menu owns focus. The user confirmed hearing Charsi's target label;
  the log identifies that particular announcement as a primary world tooltip.

World labels settle for 80 ms. Inventory coordinates allow 20 ms (one subsequent
30 Hz poll) for a new tooltip; an already rendered, focus-matching item description
is delivered immediately. Existing NPC menu-row navigation also reads immediately,
while new dialogues and repopulated text keep the 80 ms settling window.
A stationary target is
silent; losing it and returning re-arms the same name. Different inventory
cells retain distinct selection identities even when their item names match.
Private-use font glyphs are removed from speech while the surrounding text and
Unicode punctuation remain intact.

The inventory title is cached per panel entry. Shared tab discovery now lives in
`TabReader`, covering visible panels outside controller focus as well. It includes
initially hidden sub-tabs, then rechecks visibility and selection without full
walks; close, overlays, toggles and session changes clear the relevant cache.
See [latency and tab evidence](navigation-latency.md). The conversation
reader skips item grids instead of walking and discarding their full panel trees.
An explicit `access-text dump` still walks the inventory for diagnostics. Tests
verify scan counts and delivery timing. `[pump] slow accessibility tick` reports
callbacks over 30 ms, at most once per five seconds, independently of gaps between
game updates. The user confirmed smooth inventory navigation after SDK logging
was moved to its own worker (2026-09-15). Tab announcements remain unconfirmed.

The user confirmed axe descriptions at the blacksmith on 2026-09-15. Full equipment
and storage operations still need live item tests. Cells with no rendered text
are not declared empty solely because a tooltip is absent.

## Item speech without grid coordinates

The blacksmith trace showed a coordinate-only announcement followed approximately
350–390 ms later by coordinates plus the item description (12:10:01–12:10:20).
Removing only the latter prefix would leave the first interruption intact.

`InventoryItemWidget` function `0x2A89C0` obtains the selected x/y and calls the
virtual item-at-cell getter at vtable `+0xC8`. The live `InventoryGridWidget`
vtable `0x1CF3800` points that slot to `0x2C49F0`: it resolves the widget's owner
through `0x2A7810`, uses its inventory page at `+0x630`, and queries the selected
cell through `0x386A00`. The latter looks up a pointer in the inventory grid;
it does not move an item. Source: existing live `.rdata` capture and Ghidra
decompilation of those functions, 2026-09-15.

The native adapter now uses this getter on the UI thread, after verifying widget
class, visibility, grid bounds, and executable function memory. A native fault or
unavailable getter returns **Unknown**, distinct from an empty cell; no item
pointer is retained. This lookup is needed only while a grid's current description
is absent. It works through the widget owner for vendor, player and storage grids.

An occupied cell waits silently for the rendered tooltip. Once available, its
description replaces the coordinate option; name, stats and footer remain intact.
Empty/unknown cells retain the existing 20 ms coordinate fallback. Equipment slot
names and action labels are unaffected. If an occupied cell never renders text,
it remains silent. The user confirmed the new behavior on 2026-09-15:
“parece que funcionó bien”.

## Inventory action labels and equipment locations

A read-only probe with the inventory open found `gold_button` as a native
`ButtonWidget` (vtable RVA `0x1CE4148`). Its embedded caption at `+0x8A0` was
empty, but both `+0x88` and `+0x310` contained **Soltar oro**. Reading only the
caption therefore missed this action. The generated layout catalog also offered
an unrelated sibling label, so native focus and strings own these actions now.

`FocusableWidget` registration `0x875CB0` and the live reflection table confirm:

| Field | Member offset | Live reflection record RVA |
|---|---|---|
| tooltipString | `+0x88` | `0x2535D90` |
| buttonLegendString | `+0x310` | `0x2535E20` |

Each reflection record is 0x30 bytes: name pointer `+0`, kind `+8`, member offset
`+0x10`, and member size `+0x20`. Both string fields have size 0x28. The button
reader combines a caption with distinct help, or falls back to its legend when
both are empty. `+0x88` is read only on known button classes here; it is not a
generic text field on every widget. Toggle buttons retain the existing value
reader. A focused action can also use the captured primary tooltip body/footer,
but only when the capture belongs to that exact panel and widget.

`InventorySlotWidget` registration `0x2CA5F0` identifies `location` at `+0x5D8`
(live reflection record `0x236C530`, size 4). All ten live equipment slots matched
the `location` fields in `game-data/global/ui/Layouts/PlayerInventoryOriginalLayout.json`:

| Value | Layout location | Spoken Spanish location |
|---|---|---|
| 1 | head | Cabeza |
| 2 | neck | Cuello |
| 3 | torso | Torso |
| 4 | right_arm | Arma derecha |
| 5 | left_arm | Arma izquierda |
| 6 | right_hand | Anillo derecho |
| 7 | left_hand | Anillo izquierdo |
| 8 | belt | Cinturón |
| 9 | feet | Pies |
| 10 | gloves | Guantes |

The layout's weapon/ring sprites corroborate the arm/hand distinction. Runtime
reading uses the native location value, not widget names. Unknown values add no
invented location. A slot name precedes its rendered item description and remains
available without items; absence of a tooltip still does not imply an empty slot.

Actions read immediately and inspect only the focused control subtree. The
conversation reader skips discarded full-panel/fallback walks for this archetype,
as it already did for inventory cells. NPC dialogues retain priority. The user's
specific scroll-related control is still unidentified; synthetic scroll-action
tests validate the common rendered-help path, not that live control's coverage.

## Tooltip record layout and body markup

Evidence: offline disassembly of the runtime image of build 3.3.93787 (raw ==
virtual, base `0x140000000`), cross-checked against the item readouts logged
live on 2026-09-20. Addresses are absolute.

`TooltipsPanel::Draw` at `0x140880578` iterates the record vector at stride
`0x2E8`; `0x1408DA7E0` renders one record. The per-record cleanup frees exactly
two strings, at `+0x00` and `+0x28`, and the renderer reads text from those two
pointers and nowhere else, so **a record holds two strings and no separate name
field**. The item's name is inside the body string.

| offset | size | field | how it is used |
|---|---|---|---|
| `+0x00` | 0x28 | body engine string `{data, size +0x08, capacityAndFlags +0x10, inline +0x18}` | `mov rcx,[rsi]` into the text draw; `cmp qword [rsi+8],0` tests empty |
| `+0x28` | 0x28 | footer engine string, same shape | `mov rcx,[rsi+0x28]`; `cmp qword [rsi+0x30],0` |
| `+0x50` | 0x10 | rect A -- x, y, w, h as int32 | background fill `0x140657B90` |
| `+0x60` | 0x10 | rect B -- the body's text rect | `lea rdx,[rsi+0x60]` -> text draw `0x140902E20` |
| `+0x70` | 0x10 | rect C -- the footer's text rect | `lea rdi,[rsi+0x70]`, same text draw |
| `+0x80` | ~0xA0 | text style block, ten 16-byte `movups` to the stack | passed as `r8` to the text draw |
| `+0xA8` | 4 | a mode inside that block | `cmp dword [rsi+0xa8],1` |
| `+0x120` | 0x10 | four dwords | args to `0x14090CBD0` on the empty-body path |
| `+0x151` | 1 | flag: run the per-rect pass `0x1408DAAD0` | called once per text rect |
| `+0x154` | 4 | fixed-point fraction in 128ths | `imul edi,[rsi+0x68]`, `sar edi,7` -- splits the body rect into two coloured spans |
| `+0x158` | 0x10 | a colour, four floats | patched into the stack copy of the style block |
| `+0x168` | ? | block passed as the fill's fifth argument | `lea r14,[rsi+0x168]` |
| `+0x2D8` | 1 | byte flag | passed on the stack to `0x140657AB0` |

### The body is stored bottom-up

The body is the drawn lines joined by the localized `newline` token, **last
drawn first**: line 0 is the bottom of the tooltip and the final line is its
title. Measured from the readouts of two items on 2026-09-20 -- a Short Sword
stored as `Sword Class - Fast Attack Speed / Max Sockets / Durability /
One-Hand Damage / Short Sword`, and a Minor Healing Potion stored as
`Points: 45 / Minor Healing Potion`. `TooltipText::Read` therefore walks the
body backwards so its lines come out in screen order.

The lines themselves are composed in `D2Client\src\UI\InventoryTooltips.cpp`,
which the image attributes thirteen functions to, spanning `0x1402BCB1A` ..
`0x1402C2909`; the two composers are `0x1402BD480` and `0x1402BE560`.

### The inline font command takes one index character

`U+E07E` introduces a font command and **exactly one character follows it**.
The renderer subtracts `'0'` and accepts the result below `0x27`, so the index
runs `'0'` through `'V'` -- 39 values. Three parser sites range-check it
identically: `0x140908BD1`, `0x14090C225` and `0x14090F660`. Index 2 is the one
a title is drawn in.

Item tooltips are drawn with index `'0'`. Stripping only `'1'` and `'2'` left
that `'0'` in the text, so every spoken line began with a stray "0" and the
base-stat labels ("One-Hand Damage: ") no longer matched their own lines.

### A trap when disassembling a runtime dump

An image dumped from a running, modded game contains the plugin's own inline
hooks. At `0x1408DA7E0` the first five bytes are a jump out of the image, over
`push rsi; sub rsp, 0x110`; disassembly only makes sense from `+9`. The original
bytes are in `DrawTooltipBytes` in `native_tooltips.cpp`.


## Validation

`native-tooltips` uses synthetic native panel/string records and the SDK hook
registration interface. It covers primary-only capture, full text, focus/cell
association, expiry, reset, invalid memory/counts and rejected installation.
`interaction-reader` covers target return, duplicate suppression, overlays,
storage tabs, coordinates, identical items, stale text rejection and equipment.
Grid-query tests cover occupied/empty/unknown, invalid cells, hidden widgets and
fault containment. Interaction tests simulate delayed descriptions, movement to
another occupied cell, return to an empty cell, and complete coordinate-free speech.
The existing native UI and gameplay tests cover archetype selection and lifecycle
integration. Build/test results are recorded in `STATUS.md`.
