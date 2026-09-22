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

Identifying an item is the same carry-and-release mechanic on controller (hold
to pick the scroll up, release it on the target), so it gets the same notice;
the mouse path goes through `ContextMenu`'s runtime-filled `OptionsList`, which
the mod does not read at all yet.

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

The Mercenary-tab log only reaches player inventory slots, never a hireling slot;
equipment navigation still needs a live check with an available mercenary. See
[the tooltip/menu evidence](native-tooltips.md#compared-items-and-menu-regressions-2026-09-22).

## Hiring rows (2026-09-22)

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
The action and mercenary equipment still need controller listening validation.
