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
  different menu owns focus. Actual NPC/controller validation remains pending.

The interaction reader settles changes for 80 ms. A stationary target is
silent; losing it and returning re-arms the same name. Different inventory
cells retain distinct selection identities even when their item names match.
Private-use font glyphs are removed from speech while the surrounding text and
Unicode punctuation remain intact.

Current boundary: cells without a rendered tooltip announce their coordinates.
They are **not declared empty**, since absence of a tooltip alone is insufficient
evidence. This is an initial inventory reader, not complete inventory operation
coverage. Item order and equipment/storage behavior still need a live item test.

## Validation

`native-tooltips` uses synthetic native panel/string records and the SDK hook
registration interface. It covers primary-only capture, full text, focus/cell
association, expiry, reset, invalid memory/counts and rejected installation.
`interaction-reader` covers target return, duplicate suppression, overlays,
storage tabs, coordinates, identical items, stale text rejection and equipment.
The existing native UI and gameplay tests cover archetype selection and lifecycle
integration. Build/test results are recorded in `STATUS.md`.
