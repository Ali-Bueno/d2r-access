# Character panel, equipment slots and placing items

Three things the 2026-09-17 live feedback exposed in the inventory screen.

## The character panel (`CharacterStatsPanel`)

Same panel name in all three layouts (`CharacterStatsPanel.json`,
`CharacterStatsPanelHD.json`, `Controller/CharacterStatsPanelHD.json`), so
`FindPanel("CharacterStatsPanel")` reaches it everywhere.

Every row is a `<name>Title` label with its number in a sibling widget, and all
the numbers are **blank in the JSON and filled at runtime** — only the labels
carry a `@strchrXXX` key. The four raisable attributes already had a reader:
their `<stat>PointButton` is focusable and `NativeProgression` reads the
sibling `<stat>Title` / `<stat>Stat` pair. **The rest had none.** The controller
layout gives each other `<name>Title` a focusable child called `Tooltip`, so
the game stops on defence, stamina, life, mana and the four resistances and the
mod said nothing there.

`src/character_stats.cpp` reads those rows:

- The focused widget is either the `<name>Title` itself or its `Tooltip` child;
  either way the title supplies the spoken label, straight from the game.
- The value is resolved generically as the title's stem plus `Stat` or `Text`
  (`StrengthStat`, `FireText`), so a row nobody listed still reads. Only the
  rows the layouts name differently need the table in that file:
  `ArmorClassTitle` → `ArmorClass`, and the three current/maximum pairs
  `StaminaTitle`, `HPTitle`, `ManaTitle`.
- Current/maximum rows are joined as "45 of 120" (`currentOfMaximum`).
- The **body** is the panel name plus who the character is — `CharacterName`,
  `FullClassInfo` (or `ClassInfo` in the original layout) and `Level` with the
  game's own `strchrlvl` label. Because the reader only speaks the body when the
  context changes, that identity is said **once on entering the panel** and then
  navigation reads one row at a time: the "header, then navigate" the user asked
  for.
- A row whose label has not rendered yet speaks nothing rather than a stale
  value; a missing identity part is skipped, never guessed.

**The button that opens it** (`MiniMenuCharacter`, in the HUD's `MiniMenu`
beside the equipment) had no name because the layout gives it only an icon.
`tools/gen_panel_widgets.py` now maps a `MiniMenuButtonWidget`'s own `menu`
field to the game's matching `minipanel*` string key, so all seven mini-menu
buttons — character, inventory, skills, map, party, quests, game menu — are
named from the game's own string table.

## Equipment slots that sometimes went silent

`NativeUi::EquipmentLocation` returns 0 for every failure alike — widget not
visible, guarded read missed, unclassified — and the caller only spoke a slot
name for a location in range. When that read missed while the game rebuilt the
panel, the name simply vanished; if `EquipmentItemPresence` was `Unknown` in the
same poll the whole announcement was empty and the move was silent.

A slot's location never changes and widget names are stable across rebuilds, so
`interaction_reader.cpp` now remembers the last location seen under each slot
widget's name and uses it when the live read misses, logging that once per slot
(`[interaction] equipment location unreadable for …`). A slot that was never
read successfully falls back to the generic word for equipment, so moving onto
it can no longer be silent.

## Saying where a carried item landed

Releasing an item only said "item released". It now names the destination
(`itemPlacedIn`): the equipment slot for a paperdoll slot, the row and column
for an inventory or stash cell, and the belt for the belt. The belt is a single
`BeltWidget` grid rather than four slot widgets, so which of its four cells
received the potion is **not** known yet — that is the remaining gap for belt
assignment feedback.

Identifying an item on controller is holding A on the tome or scroll, then A on
the target: the inventory's hint reads "A: Recoger(Mantener para: Usar)", and a
tap only picks the tome up (the user's log, 2026-10-20; they report identify
works). The mouse path goes through `ContextMenu`'s runtime-filled
`OptionsList`, which the mod does not read at all yet.

## Where the button hints go

The controller hints are the last thing in an announcement, but until now they
could still be *heard* first: an occupied cell or slot speaks the moment the
focus moves, while the game's own item tooltip is about 350 ms behind, and the
hints went out in that first utterance. They are now withheld while an occupied
cell or slot is waiting for its description and appended to the utterance that
finally carries it, so the order heard is always item, then grid position, then
what you can do with it. The cost is deliberate: a cell whose description never
renders no longer announces its hints.

## Mercenary statistics (2026-09-22)

`HirelingInventoryPanel` shares the title/value-row reader. Its controller layout
uses `HPTitle`/`HPStat`, `HireTypeText`, and a nested `DamageStat`. The visible
tree supplies row order; identity uses `MiniPanelHireinv` plus the rendered name
and type/level. In the observed no-focus state, the existing right-stick cursor
can read these rows. Any actual focused widget in equipment or another panel
keeps the stick's usual gestures. Switching panels resets both row position and
the identity announcement.

The menu switcher's tab strip (`UISwitcherTabs`, a `TabBarWidget` outside these
panels) is the exception: a left-stick push up from the top row lands there
(log 2026-09-24 15:19:24), and the stat cursor then kept yielding until the panel
was reopened. With focus on a tab strip the visible character, hireling or
advanced panel keeps the right stick and its cursor (2026-09-25).

The Mercenary-tab log only reaches player inventory slots, never a hireling slot;
equipment navigation still needs a live check with an available mercenary. See
[the tooltip/menu evidence](native-tooltips.md#compared-items-and-menu-regressions-2026-09-22).

## Hiring rows (2026-09-22)

2026-10-13, a tester: a row said only the name and cost, and Y said nothing. The
row's skills (`Skill0..2` icons, the text at +0x698 or, while it is not built,
the name of the skill id at +0x960) now follow 400 ms after the row, names only;
Y says them in full (with autodetail). When the icons give nothing, Y speaks the
tooltips the game draws while it is held. `[hire] row skills ->` and
`[hire] Y -> "..." (n icons)` tell which path worked; the icons' offsets were
read from the character panel's setter and are unverified on hire rows.

2026-10-13, the tester again: still nothing. The pad path above was dead code:
`HireStats::Tick` sent every tick to the keyboard path when `KeyboardMenu::Active()`,
which is true for any menu in front, a pad player's too. The tester's log shows
only the keyboard path's `[hire] record 0` lines, which already held the icons'
skill texts (so +0x698 reads on hire rows). The gate is now `KeyboardMenu::Engaged()`,
the keyboard cursor's own state (no pad connected, or an arrow key took the menu);
`[hire] list entered, pad|keyboard cursor` says which path a session takes.

The user confirms that the hiring list already reads. The extracted HD layout
`game-data/global/ui/Layouts/HireMenuPanelHD.json` defines `HireName`, `HireLevel`,
`HireCost` and three `HirelingSkillIconWidget` children. Its controller variant
uses a single `ListItemWidget` candidate without separate authored focus targets
for those icons. `game-data/local/lng/strings/ui-controller.json` contains
`ShowMercSkills` (ID 27022, "Show Mercenary Skills"). The widget types and string
establish a skill-display action, but do not establish which tooltip records it
draws or its live controller binding.

Previously, a generic list row was owned by `GameTextReader`, which suppressed
`InteractionReader` as though a conversation were active. That reset native
tooltip capture and omitted controller action hints. Runtime `ListItem` controls
now share the interaction path with other controls, while actual dialogue/menu
surfaces retain priority. Candidate text reads immediately; any matching rendered
tooltip records follow in screen order without equipment-comparison labels or
font glyphs. Only changed details announce automatically. Right-stick up repeats
the currently visible descriptions; ordinary repeat also retains the candidate
and action hints. Changed selection, leaving the row or closing its descriptions
discards the old detail. No skill properties or bindings are synthesized.

Automated coverage checks routing, native hint order, multiple rendered records,
repeat/silence behavior, stale selection and transitions to unrelated controls.
Live inspection stopped at the title screen because Windows refused foreground
focus to the guarded desktop helper; the owned process was closed without input.
That initial probe is superseded for hiring by the user's later gameplay log.
Mercenary equipment still needs controller listening validation.

### Full skill description interrupted by its first record

The later log shows Kyoko's full Freezing Arrow, Cold Arrow and Inner Sight
descriptions at 20:50:53.815, then an automatic Freezing Arrow-only repeat at
20:50:54.268. Kundri similarly reads Exploding Arrow, Fire Arrow and Inner Sight
at 20:50:59.053, then only Exploding Arrow at 20:50:59.365. These are `[interaction]`
outputs, not requested `[detail]` reads. Candidate changes correctly switch the
fire/cold sets. The first-only update interrupted the longer utterance, explaining
the user's impression that each candidate had just one skill.

The shared row reader now compares cleaned tooltip records individually. A new
or changed record, or a new candidate, announces; removing records alone stays
silent. Manual detail still follows the current snapshot, and closing the tooltip
clears it. Reopening the full skill display announces again. This avoids preserving
hidden skill data or relying on text-prefix heuristics. Three related CTest suites
pass, including the full-set-to-first-record regression; listening is pending.

## Advanced statistics

The controller `AdvancedStatsPanelHD.json` sets `ScrollBar` (a
`ScrollControllerWidget`) as its default focus. Each statistic is runtime text in
`ListContainer` → `ListItemWidget` → `StatText`; focusing the scroll bar alone
only announced the panel title (observed 2026-09-23). Both character and
hireling `AdvancedStats` buttons open this same panel.

The shared stats reader owns the focused scroll bar and speaks the localized
title, rendered subject name, and first row in one utterance. This suppresses
the separate title paths that would interrupt the row. The right stick reads
the remaining live rows in tree order. Replaced row identities reset its
cursor; changing a value in an existing row does not. Native scroll controls
and other focused widgets keep their normal gestures. The `character-stats`
suite covers initial speech, idle ownership, row order, traversal, value
changes, rebuilt rows, and focus handoff; live controller and NVDA validation
remains pending.

The 2026-09-23 build passed all 71 CTest suites without compiler warnings;
the installed plugin matches the built DLL's SHA-256.

## Hit-chance tooltips (2026-10-25)

Static reading of `re/D2R.rebuilt.exe` (3.3.93787, code identical on 3.3.94995); no live check
yet. RVAs are live; fingerprints are `{rva, size, LayoutCheck::Hash}`, computed with the same
masked FNV as `tools/layout_fingerprints.py` (the tool reproduces the shipped `0xF2B40` entry).
The image's `.rdata` does not match the live one everywhere (`re/notes.md`), so a string named
below from a `lea` is only trusted where its length and its neighbours agree.

**Where the text is made (verified, code).** `CharacterStatsPanel` (factory `0x14E5820`, object
`0x250` bytes, vtable live `0x1FDCEF0`) keeps the hover text in an engine string at **`+0x198`**
(`data +0x198`, `size +0x1A0`, inline buffer `+0x1B0`) and its anchor rect at `+0x238`. Its
message handler (`0x14E90A0`, call at `0x14E9C5A`) passes the payload's x/y (`+4`/`+8`) to
`0x14E8300`, which clears the string, hit-tests children (`0x856220`, direct child by name) and
fills it from one of two builders. The panel's vtable method `0x14E59B0` draws it whenever the
size is non-zero (`0x14E5DD6`), through live `0x3E2A92A` (outside the rebuilt image), the same
callee `FocusableWidget` uses for its own `tooltipString` (`0x875A51`).

- Attack rating: `0x14E7FE0(panel, out, player, hand)`. `hand` is 1 for the first skill block, 0
  for the second; AR from `0x28A590(player, hand)`, chance from `0x1514700(AR)` (AR 0: empty
  text, no tooltip). Text = `charmonsterX` (10103, `"%s: %d%%"`, name, chance) + `newline` +
  `charavghit` (4159), keys via `0x5F4B90`. Stored bottom-up like every tooltip body, so it reads
  "Average chance to hit / Fallen: 92%". (`charmonsterX`/`charavghit`: verified by length and the
  panel's string block; `newline`: 7-character key, hypothesis.)
- Defense: `0x14E8220(panel, out)` = `0x15149F0()` then `0x1514CA0(chance, buf)`, which clamps
  5..95 and uses id **10105** when the block chance (`0x347DA0`, fallback `0x1514E00`) is above 0,
  else **10104** (both by id through `0x5F4A50`). 10105 bottom-up: "Average chance X will hit
  you" above "Chance to Block".
- Which widgets: the left/right skill blocks' `Rating` row and the defence row; a branch on
  `PanelManager+0xB9` (`0x846210`, probably the controller-layout flag) tests one level deeper.
  Exact child names are not readable from this image (hypothesis).
- The message: its name hash is seeded with `'M'`; of the names tried, only `MouseMove` is an immediate in the image
  (`0x681C0F1F0A501EB1`, dispatcher `0x88BBE4`) — hypothesis, strong. So the
  keyboard cursor (`MenuMouse::Place` moves the real pointer) should produce the text; pad focus
  alone probably does not (the `Tooltip` child shows only `CharacterStatsTooltip*`). Live check.

**The monster (verified, code).** Hover array `0x2A41630` (8 × `0x10`, `native_hover.cpp`):
int16 class at **`+0xC`** for the attack side (getter `0xF1860`) and **`+0xE`** for the defence
side (`0xF18B0`). Both start as **19** (`fallen1`, "Fallen") — static init `0x351F0`, reset
`0xF1BA0` — and `UpdateHover 0xF2B40` rewrites them for a hovered monster that is not an NPC
(`MonStats+0x3D` against a mask, hypothesis), not friendly (`+0x87 != 1`) and passes two `MonStats2` mode-byte tests
(`+5`, `+6`; `+6` keeps `+0xE` unchanged). The name is the **base** `MonStats+0x36` string
(`0x5F4A50`), with a leading `[xx]` gender tag skipped (`0x1E1CF0`), never the unique name. There
is always a monster, so the "level %d monster" strings (4160–4162) are dead: no code loads them.

**The numbers (verified, code; a reimplementation would be fragile).** `diff` = byte 0 of
`0x9A610`; `mlvl` = `max(MonStats.Level[diff]` at row `+0xFC+2*diff`, the current area level via
`0x34B440 → 0x2EFC10 → 0x90BE0)`; the monster's defence/AR come from `0x3A26C0(bank, class,
0x8B040() (player count), diff, mlvl, mask)` (3,274 bytes, MonStats/MonLvl). Attack: def at
`out+8` (mask 2), `×10/12` when not friendly, byte 1 of `0x9A610` is 1 and `diff > 0`; AR gains
CharStats `+0x38` (ToHitFactor, hypothesis) unless `0x34B9D0(player)`; `2·100·AR/(AR+def)·clvl/
(clvl+mlvl)`, clamped 5..95. Defence: player def `0x347B10 + stat 33`; monster AR at `out+0xC`
(masks 8, 0x10, 0x20 until non-zero), `×2/3` on the same condition; `2·100·AR/(AR+def)·mlvl/
(mlvl+clvl)`. Negative values move to the other side of the ratio in both.

**Recipe.** Read `panel+0x198` (bounded engine string, after checking `*(panel) == base +
0x1FDCEF0`) while it is non-empty and walk its lines backwards like `TooltipText::Read`; this is
the game's own text. Do not compute. For pad focus, if the live check confirms the panel never
fills it, the read-only fallback is the class word plus the name; the percentages would need
`0x1514700`/`0x15149F0` called on the game thread.

```
{0x14E5820, 397, 0x16BCB29E6CC3D8CBULL},  // factory: size 0x250, vtable, +0x198 string, +0x238
{0x14E8300, 2149, 0xCC21893231598228ULL}, // hover: clears/sets +0x198, rect +0x238
{0x14E59B0, 1258, 0x0575D8D587A85A86ULL}, // draw: +0x1A0 > 0 -> show tooltip
{0x14E7FE0, 566, 0xDB7C39187976DAF3ULL},  // attack text
{0x14E8220, 220, 0xFF21BA0BBC012516ULL},  // defence text
{0x1514700, 749, 0xE556344C97C425D1ULL},  // chance to hit
{0x15149F0, 687, 0x5265C26DBD44DD92ULL},  // chance to be hit
{0x1514CA0, 343, 0x67BA149E4C0ADB47ULL},  // clamp + 10104/10105
{0xF1860, 66, 0xDC510325BA7BE405ULL},     // hover +0xC
{0xF18B0, 66, 0x36C5B8F7CE3FC793ULL},     // hover +0xE
{0x351F0, 39, 0x1D2FB78F6F754057ULL},     // init both to 19
{0xF1BA0, 179, 0xBB26270476FEBAB0ULL},    // reset both to 19
{0xF2B40, 1195, 0xF11A8AE0085BFE60ULL},   // UpdateHover (already in native_hover.cpp)
```

**Shipped 2026-10-25: a diagnostic only.** The stick walks the rows without moving the game's own
focus, so the panel's hover text belongs to wherever the game's cursor is, not to the row read.
`CharacterStatsReader` logs that text when it changes (`[stats] hover text "..."`, after the vtable
check), to confirm `+0x198` live and learn whether controller focus fills it. Speaking the hit
chances on the rows needs the game's own calculators (`0x1514700`, `0x15149F0`) and the hover array's
monster, a call into game code decided after that live check.
