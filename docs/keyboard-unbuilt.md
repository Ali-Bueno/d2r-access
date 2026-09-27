# Keyboard play: what is not built

Written 2026-09-27 after the third session of that day, for the next one. The
user, the same day: *"those are very important and need to be built before
actual play."* So this list is the work, not a wish list. Nothing here is
optional.

**MEASURED** means the running game or its log showed it. **READ** means the
code, the layouts or the game's image say so. **INFERRED** means reasoned and
not checked. **OPEN** means not known.

What is built is in `keyboard-play.md`; every key is in `keyboard-keys.md`.
The working files of the static passes are in the workspace, outside this
repository: `re/actions/swap`, `re/actions/belt`, `re/actions/widgets`, each
with a `notes.txt` that has the offsets, the functions and their
fingerprints. **Read the notes before reading the image again.**

## What was done since this list was first written

| was | now |
|---|---|
| 1. swap weapons | **built**, Y (`0958e46`). Verified at load; not pressed in a test |
| 3. say it again, in a menu | **built**, backslash (`786d14e`). Not pressed in a test |
| 4. the belt | **built**: every row (`d69d0b2`), the fill on Alt+F (`f64d6ff`). Verified at load; not tried in a test |
| 8. the controls in the mod's keys | **built** for an item's hints (`0b27dce`). Not heard in a test |
| 10. the pointer, when play goes on | **built** (`f3d5ea8`); where it rests is in question, item 7 below |
| the menu's right click | moved from W to D by the user (`7ceb6e1`) |
| comma and full stop | retired by the user (`a99cc8c`) |

The second live test (12:18, three and a half minutes) found that **the game
was never in play**: the mercenary's portrait was counted as a menu. That is
fixed (`ee44174`), with two more things the same log showed (`a946d64`,
`f841d56`). **No test has run since those three fixes**, so play itself, with
the menus built, has still not been tried. `keyboard-play.md` has the
findings.

## Decisions that are the user's

| question | state |
|---|---|
| How is the left hand's skill chosen? | **open.** Asked 2026-09-27; the user answered with a question (item 1). Put the facts to them and ask again; do not choose |
| Should the scroll bar and its two arrows be places of the cursor? | not asked yet (item 3) |
| Is "Row 1, column 2" after "Empty" wanted on an empty belt slot? | built unasked; told to the user 2026-09-27, no answer yet |

## 1. Choosing the skill for each hand

**The state:** no key of the mod's. A and W fight with whatever skill is on
each hand.

The user's question, 2026-09-27: *"is this necessary? how are skills assigned
to a hand by the UI? you can bind skills to a hotkey, are you sure that left
click isn't attack and right click isn't use skill instead of being
skill-per-hand?"*

**What is known:**

- MEASURED, first live test: the character has a skill on each hand
  (`[keyboard] fight: hand=left skill=0`, and W used skill 104). Skill 0 is
  Attack: the left hand attacks because Attack is the skill on it.
- READ, `re/actions/widgets/notes.txt` section D: the game's key S (action 13)
  opens and shuts the list of skills **for the right hand only. No key action
  of the game opens the left hand's list.** With the mouse it is a click on
  the HUD's `LeftSkillButton`; `RightSkillButton` opens the right hand's.
- READ: the list is the panel `SkillSelect`. Its skills are copies of
  `ButtonTemplate` made at run time under `SelectionTray`, each a button with
  a rectangle, its skill id at +0xC08. A click on one puts that skill on the
  hand the list was opened for and shuts the list.
- READ: F1 to F8 pressed while the list is open bind the skill under the
  pointer to that key. INFERRED (0.8): for the hand the list is open for.
- MEASURED, second test: both HUD skill buttons give a rectangle
  (`[cursor] play goes on: the pointer to 962,1042, between the skill
  buttons`).

**So:** the right hand can be changed today with the game's S, the arrows and
A, once `SkillSelect` is seen to work with the cursor. The left hand cannot.

**To build, once the user has chosen:** either a key that clicks
`LeftSkillButton` (it has to work in play, where the menu keys are off), or
the two HUD buttons put in the cursor's menu while the skill tree is open, as
the belt is while an inventory is. `FindNamed(hud, LeftSkill, ...)` and
`ClickWidget` in `src/keyboard_menu.cpp` are the pieces.

**To measure:** what `SkillSelect` holds (`[cursor] thing` lines), whether a
skill in it reads its name, and whether a click chooses it.

## 2. Sliders, options that step, and dropdowns

**The user's decision, 2026-09-27:** *"left and right should change a slider,
the same as an option with toggles, the same as the controller would."* So:
with the cursor on a slider or a carousel, the left and right arrows change
the value and do not move the cursor. Up and down still move it.

**What is known, all READ, `re/actions/widgets/notes.txt`:**

- **Slider** (`SettingsSliderWidget`): value at +0x6C4, minimum +0x6BC,
  maximum +0x6C0, increment +0x6B0 (0 means a step of 5 percent of the
  range). Its two arrows are widgets inside the slider object, `LeftArrow` at
  slider +0x970 and `RightArrow` at slider +0x1500, **not among its children**,
  so the cursor does not list them. A click on an arrow steps the value by
  one step. A click on the track sets the value to where the click was:
  **never click the track to step.** The setter is `SetValue(slider, value,
  1, 1)` at `0x8DC0C0`; keys, arrows and the mouse all go through it, and with
  the last argument set it tells the options panel.
- **Carousel** (`CarouselWidget`): three children, `CarouselText`,
  `CarouselLeftArrow`, `CarouselRightArrow`. A click on an arrow steps it; a
  click on its body does nothing.
- **Dropdown** (`DropdownListWidget`): a click opens the panel
  `DropdownListContents`. Its rows are the children of the row container of
  its list `Items`, in order, which `NativeUi::ListRows` reads already; each
  row has a `TextBox` child and a rectangle. A click on a row chooses it and
  shuts the list.
- With the pointer over a slider or a carousel the game itself steps it on
  the Left and Right arrow keys. OPEN: whether the mod's arrow handling keeps
  those keys from the game (the mod reads the arrows by `GetAsyncKeyState`
  and does not consume them). **If the game already steps the value when the
  arrow is pressed, a click by the mod would step it twice.** Measure before
  building: land on a slider, press right once, read +0x6C4 before and after.

**To build:**

- In `Step` of `src/keyboard_menu.cpp`: on a `Kind::ValueControl` that is a
  slider or a carousel, left and right act on the value. For a carousel,
  click the arrow child. For a slider, click the arrow inside it (its
  rectangle by `RectAt(slider + 0x970)` after checking its name and class) or
  call `SetValue`, fingerprinted as `src/native_sort.cpp` does.
- `Thin()` probably drops a carousel for holding its two arrows (INFERRED):
  the carousel has to stay the place, and its arrows must not be places.
- A dropdown's rows into the cursor's menu when the list is open
  (`DropdownListContents` is a panel of its own, so `Build` may take it
  already: look for it in the `[cursor] menu:` line), and the row the cursor
  is on read by its `TextBox`.
- The value said after a change. `src/focus_tracker.cpp` reads a value when
  the game says it changed; whether that runs with the cursor is OPEN.

## 3. Rows out of view: scrolling

**The state:** not built, and not investigated: the static pass on it was
stopped when the usage limit was reached, and left nothing.

READ in the layouts: the options panels, the key binding menu, the dropdown
list, the character list and others put their rows in a `ScrollViewWidget`
with a `ScrollControllerWidget` bar. A row scrolled out of view cannot be
clicked. MEASURED, second test: the bar and its two arrows are in the cursor's
menu as places of their own (`ScrollBar`, `UpArrow`, `DownArrow`), named by
their layout names, and a click on an arrow was made; what it scrolled is
not in the log.

**What is known, READ:** `0x1FACD0` is `GetScreenRect(widget, Rect*)` for any
widget, not only a focusable one (`re/actions/widgets/notes.txt`, shared
mechanics). `ScrollViewWidget` init `0x892F80` makes a child `View` (kept at
+0x90) and moves the children into it.

**OPEN, the questions the stopped pass was given:** where the scroll position
is kept and in what unit; whether a row's rectangle from the focus rectangle
virtual already has the scroll in it; whether the mouse wheel scrolls the
view; whether the game has a "bring this row into view" that the controller's
focus uses, and whether it depends on controller mode; how to tell a row
that is clipped.

**To build:** bring the row into view when the cursor lands on it, then put
the pointer on it. Until then a long list is reached only through the bar's
arrows.

## 4. Rows in a list: characters, waypoints, the NPC's menu

**MEASURED, second test, character select:**

- The cursor lands on the character's row and the reader reads it
  (`[focus] CharacterSelectPanel/CharacterTemplate...#Gordan, Level 7 Paladin
  row 0 -> "Gordan, Level 7 Paladin"`).
- A on the row was pressed five times (12:19:03 to 12:19:20). The game
  started 400 ms after the fifth. OPEN: why the fifth and not the first four;
  whether another key was pressed is not in the log.
- The panel has a play button, `QuickPlayButton`, which the cursor reaches.

**OPEN:** whether the game moves its own focus with the arrows on these
screens; whether a waypoint travels on one click; the NPC's menu. None was
opened in the test.

## 5. The readers that were built on the controller

READ in the code, not yet heard in keyboard mode:

| reader | what is wrong | where |
|---|---|---|
| the skill tree | reads `TabN > SkillButtonContainer > SkillButton`, a shape measured in controller mode only. The mouse layout has no button template: the class and the parent of a skill button in mouse mode are OPEN | `src/native_progression.cpp:56-61` |
| the quest log | the mod clicks a quest on landing to fill the shared pane; the reader may say "Empty" first if the pane fills after 150 ms (INFERRED) | `src/ui_text.cpp:130-147` |
| the second skill bar | reads controller-mode state; in keyboard mode the skills bound to F1 to F8 stand in its place | `src/skill_bar_service.cpp` |
| the tab highlight | has never run: it builds an empty focus | `src/tab_highlight.cpp:20` |
| the character row's "A: Play, X: Delete" | written for the pad's buttons | `src/focus_tracker.cpp:140-143` |
| the scroll bar and its arrows | no reader: the cursor says "ScrollBar", "UpArrow" (MEASURED) | item 3 |
| `PluginIconTooltip` on the character's row | no reader: the cursor says its layout name (MEASURED) | the loader's own icon |

The belt's automatic potion (`src/native_belt.cpp:25`) answers nothing out of
controller mode, and that is right: it is the controller's alone.

The cursor names a place itself when no reader does (`[cursor] no reader
named it; said "..."`). **Every such line in the log is a reader to fix.**

## 6. Ctrl + Shift + left click

**The state:** the loader takes one modifier, so this cannot be a key of the
loader's. It is the game's gesture for "move", where an item has two places
to go.

INFERRED: it works already. The mod watches A directly beside the loader
(`Watch` in `src/keyboard_menu.cpp`) and clicks if the loader delivers
nothing, and the game sees the Ctrl and Shift that are held (READ: the game
reads both from the real key state when the click arrives).

**To measure:** `[menu-keys] left click came by watching the key` after
Ctrl+Shift+A, and what the readers say next.

## 7. Where the pointer rests in play

**The state:** built, between the HUD's two skill buttons. MEASURED: the
place is found and the pointer is put there (`the pointer to 962,1042`).

**In question:** 17 seconds after the pointer was put there the log has
`[interaction] World target -> "Experience: 31,952 / 32,886"`, the experience
bar's tooltip. Whether the resting place is over the experience bar, or the
mouse was moved by hand, is OPEN. If a menu's closing is followed by
"Experience" every time, the place is wrong: try the HUD's click catcher
(`BGClickCatcher`) where no widget is drawn over it.

## 8. Home and End

**The state:** built on the mini menu's buttons in the HUD, not pressed in
either test. OPEN: whether the mini menu is shown and its buttons give a
rectangle. The log line is `[cursor] menus: ... button found / not shown /
has no rectangle`. The game's own keys open each menu: C, I, T, Q, O, Esc.

## 9. Character creation

MEASURED, second test: the panel's eight classes are places
(`HDCharacterSelectorWidget`), the cursor named one itself ("Druid"), and the
name box, the hardcore checkbox, the version dropdown and the two buttons are
reached. Not tried: choosing a class and making a character.

## 10. Sort, in a trade

`NativeSort::Sort` refuses while `TradeLayout` is open: `SortPage` takes
another path then, which was not read. Low priority; single player has no
trade.

## The order to build in

1. **A live test first**, with the three fixes in: walk, fight, open the
   inventory, move in it, the belt, Alt+F, Y, the skill tree, the options.
   The log lines to read are listed in `keyboard-play.md`.
2. Item 2, sliders and options, and item 3, scrolling: without them the
   options cannot be set.
3. Item 1, once the user has chosen.
4. Item 5: whatever the log shows the cursor naming for itself.
5. Items 4, 6, 7, 8, 9 as the log decides them.
