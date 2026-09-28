# Keyboard play: what is not built, and what is built and not measured

Written 2026-09-27 after the fourth session of that day, for the next one. The
user, the same day: *"those are very important and need to be built before
actual play."* So this list is the work, not a wish list. Nothing here is
optional.

**MEASURED** means the running game or its log showed it. **READ** means the
code, the layouts or the game's image say so. **INFERRED** means reasoned and
not checked. **OPEN** means not known.

What is built is in `keyboard-play.md`; every key is in `keyboard-keys.md`.
The working files of the static passes are in the workspace, outside this
repository: `re/actions/swap`, `belt`, `widgets`, `skilltree`, `charcreate`,
each with a `notes.txt` that has the offsets, the functions and their
fingerprints. **Read the notes before reading the image again.**

## THE THIRD LIVE TEST, 18:12: READ THIS FIRST

The test ran after this file was written; the rest of the file is as it was
before it. The findings, with the log lines, are in `keyboard-play.md`
("What the third live test measured"). In short:

**Works, MEASURED:** walking, fighting, Tab, Shift+Home, D on an NPC and the
NPC's menu, the retreat, Z and X, Y, the pause menu, the game's S with the
hand said, a skill put on the hand with A, A on a character beginning the
game, a dropdown opened and a row chosen.

**Broken, MEASURED, and the work of the next session, in this order:**

1. **The arrows on character creation.** The user: *"completely broken, not
   all elements are reachable. namely class select, name entry"*. Seven of
   the screen's fifteen places were never landed on in 73 seconds, the name
   box among them. Nothing is out of the arrows' reach; the way to it
   cannot be found by ear: up from every class is the "Toggle" button, the
   name box is below four of the eight classes only, and a step and the step
   back do not match. **The arrows have to go by rows on a screen like this,
   which is the user's to agree to first.** The table of where each arrow
   leads is in the workspace's `HANDOFF.md`.
2. **A class says nothing for 450 ms.** No reader names it, and the cursor
   waits that long before it does. Only "Druid" was ever heard. The same
   wait holds for a dropdown's rows and for the skills of the list of skills.
3. **The game version is not read.** The dropdown is "Game Version,
   dropdown" with no value, and the button beside it is read with a raw
   `%s`. The user: *"game version does not read correctly"*.
4. **After a dropdown closes the cursor is thrown to the screen's first
   place**, which on character creation is the "Toggle" button.
5. ~~The mod's settings list does not take the arrows.~~ **Not to be
   built.** The list steps with PageUp and PageDown and changes with Space;
   the user did not know that, and once told, 2026-09-28: *"no need to fix
   the arrow keys"*. The documents say so now.
6. **The log does not say what was spoken.** Several paths speak without a
   line, so what the user heard on a row or a skill cannot be read back.
7. Scroll bars and their arrows are places and are called "UpArrow",
   "ScrollBar"; the character panel has sixteen places called "Tooltip".
8. In the list of skills the Tome of Town Portal is read as "4", the number
   on its icon.

**Not pressed in the test:** Shift+S, the inventory, the belt, the skill
tree, the options and their sliders. Items 1, 2 and the skill tree's part of
5 below are still "built, not measured" for those.

## The state before the third test

Two live tests had run, the second at 12:18. The user's word on the
second test, 2026-09-27: the pointer system works *"even better than I
expected"*, and the keys are taken from the game as they should be. What the
fourth session built was READ from the game's code and tested outside the
game.

## What was done in the fourth session

| was | now |
|---|---|
| the character could not move (second test) | the fix of the third session (`ee44174`) is now held by a test that puts every panel the second test had open in play through the play gate (`8e503cb`). Not walked in the game yet |
| 1. choosing the skill for each hand | **built**: Shift+S, the user's choice, clicks the HUD's left skill button (`b3eaf91`). The hand is said as the list opens, for the game's S as well |
| the arrows, from thing to thing | **built anew** (`9637755`): the nearest thing that way, and a step made for anything the arrows would miss. Every page of every class's skill tree is walked in the tests |
| 5. the skill tree's reader | the skills ARE buttons in keyboard mode (READ; the third session's "no button template" was a misreading). The cursor names a skill by its icon when no reader does (`d0fff69`) |
| 2. sliders, options that step, dropdowns | **built** (`5f7d516`): left and right change the value; the rows of an open dropdown are the menu |
| 9. character creation | **built** (`af807aa`): typing, the class's name, the chosen class said with its description. One question is open, item 9 below |

## Decisions that are the user's

| question | state |
|---|---|
| ~~A key for a click where a box of text takes every letter~~ | **not needed.** The fourth test showed A typing into the name; the user asked that typing reach the box only with the cursor on it, built `dcc8e0e` (keyboard-keys.md) |
| Should the scroll bar and its two arrows be places of the cursor? | not asked yet (item 3) |
| Is "Row 1, column 2" after "Empty" wanted on an empty belt slot? | built unasked; told to the user 2026-09-27, no answer yet |
| The hand of the list of skills is said in the game's words, "Left Mouse-Button Skill". Are other words wanted? | built; told to the user 2026-09-27 |
| Should it be said that the line of chat has opened? | not asked. MEASURED in the second test: Enter opened the chat twice, for nine seconds each, and nothing said so |

## 1. Choosing the skill for each hand: built, not measured

**The keys:** the game's S opens the list for the right hand; **Shift+S**
(the user, 2026-09-27: *"a key to open it is fine. shift+s"*) opens it for the
left, in play as in a menu, by a click on the HUD's `LeftSkillButton`. The
same key shuts it. In the list the arrows move from skill to skill, A puts
the skill on the hand and shuts the list, and F1 to F8 bind the skill under
the pointer to that key (the game's own).

**What the mod says:** the hand, as the list opens and when it changes to the
other hand, in the game's words (`StrHDHelpLeftMouse`, "Left Mouse-Button
Skill"). The cursor lands on the first skill after 1.2 seconds, or at once on
an arrow key, so the hand is heard out.

**READ:** the list is one panel, `SkillSelect`, for both hands. The hand is
not a field of the panel: the game keeps it in its UI registry and reads it
with `0x238240`, which `src/native_skill_hand.*` calls. The icons are copies
of `ButtonTemplate` named `ButtonTemplate0`, `ButtonTemplate1` and so on,
each with its skill id at +0xC08.

**To measure:** `[skill-hand] Verified.`; `[cursor] left skill: the HUD's
button is clicked`; `[cursor] the list of skills is open for the left hand`;
`[cursor] thing ButtonTemplate0 (SkillSelectButtonWidget)`; `[cursor] skill
id=.. ".."` on each move, and what is heard: the skill's name, or its whole
tooltip. After A on a skill: `[keyboard] status` (console `access-actions
status`) gives `left_skill=`.

## 2. Sliders, options that step, and dropdowns: built, not measured

**The user's decision, 2026-09-27:** *"left and right should change a slider,
the same as an option with toggles, the same as the controller would."*

**As built** (`src/native_value_controls.*`, `Adjust` in
`src/keyboard_menu.cpp`):

- On a slider (`SettingsSliderWidget`) or an option that steps
  (`CarouselWidget`) the left and right arrows change the value and the
  cursor stays. Up and down move the cursor.
- **The mod looks before it clicks.** READ: with the pointer over one the
  game itself steps it when the arrow key goes down. So the value is read,
  120 ms are waited, and the control's own arrow is clicked only if the value
  has not changed. The log says which: `by the game on the key` or `by a
  click on its arrow`. A value cannot be stepped twice either way.
- A on a slider says its value and clicks nothing: a click on the track sets
  the value to where the click was. A on an option that steps goes to its
  next state.
- The value is said by the cursor only when no reader said it first.
- A carousel's two arrows are no longer places of their own.
- The rows of an open dropdown (`DropdownListContents`) are the whole menu
  while it is open, as a modal is. A on the dropdown opens it, A on a row
  chooses it.

**To measure:** `[values] Verified.`; `[cursor] value of X: 50 -> 55 by ...,
shown "55"`; whether the value is heard once.

## 3. Rows out of view: scrolling. NOT BUILT, not investigated

READ in the layouts: the options panels, the key binding menu, the dropdown
list, the character list and others put their rows in a `ScrollViewWidget`
with a `ScrollControllerWidget` bar. A row scrolled out of view cannot be
clicked. MEASURED, second test: the bar and its two arrows are in the cursor's
menu as places of their own (`ScrollBar`, `UpArrow`, `DownArrow`), named by
their layout names, and a click on an arrow was made; what it scrolled is
not in the log.

**What is known, READ:** `0x1FACD0` is `GetScreenRect(widget, Rect*)` for any
widget, not only a focusable one. `ScrollViewWidget` init `0x892F80` makes a
child `View` (kept at +0x90) and moves the children into it.

**OPEN:** where the scroll position is kept and in what unit; whether a row's
rectangle already has the scroll in it; whether the mouse wheel scrolls the
view; whether the game has a "bring this row into view"; how to tell a row
that is clipped.

**To build:** bring the row into view when the cursor lands on it, then put
the pointer on it. Until then a long list is reached only through the bar's
arrows. **This is the first thing to build after the third test.**

## 4. Rows in a list: characters, waypoints, the NPC's menu

**MEASURED, second test, character select:** the cursor lands on the
character's row and the reader reads it. A on the row was pressed five
times, seconds apart, and began no game. The user, 2026-09-27: the five were
attempts to click the character, and **Enter began the game**. The log has
the game's own message for it, `CharacterSelect:SelectCharacter`, at
12:19:20.415, and `FrontEndNavigation:ToGame` 12 ms after it.

**READ** (`re/actions/charcreate/notes.txt`, E): one click on a row only
chooses the character. A second click on the same row within 500 ms sends
that message, and so does Enter, **for the row the list has chosen, wherever
the pointer is.** So the five clicks did what one click does, and the row
Enter acts on is the row last clicked, not the row the pointer is over.

**Built, fourth session** (`src/native_list_rows.*`, `Land` and `Click` in
`src/keyboard_menu.cpp`), not pressed:

- **A row is chosen as the cursor comes to it**, by one click, when the list
  has another row chosen. So Enter acts on the row the cursor is on.
- **A on such a row clicks twice**, 160 ms apart, which is the mouse's way to
  act on it. A on a character begins the game, as Enter does.
- The Play button, `QuickPlayButton`, begins it as well. A character who has
  finished Normal is asked the difficulty in a modal
  (`CharacterDifficultyModal`), which the cursor takes as any modal.

**To measure, with more than one character:** `[cursor] row 1 is chosen as
the cursor comes to it; the list had 0`, then Enter, and which character the
game begins with. `[cursor] left click, twice on ListItem1`, then
`SelectCharacter` in a `[ui]` line.

**Seen in the same log, OPEN:** every arrow key pressed on that screen is
followed by the game's `ScrollController:ManualScrollInput`. The game takes
the arrow keys there for something of its own; what it moves is not known.

**OPEN:** whether a waypoint travels on one click; the NPC's menu. None was
opened in a test.

## 5. The readers that were built on the controller

| reader | what is known | where |
|---|---|---|
| the skill tree | READ: the panel builds a button for each skill in both modes, `Tab<n> > SkillButtonContainer > "SkillButton<page> <column>,<row>"`, so the reader's shape holds. It waits for the game's tooltip to name the skill. INFERRED 0.85: the tooltip of a skill under the pointer is drawn through the path the mod captures. If it is not, the cursor names the skill itself after 450 ms | `src/native_progression.cpp`, `src/native_skill_icons.*` |
| the quest log | the mod clicks a quest on landing to fill the shared pane; the reader may say "Empty" first if the pane fills after 150 ms (INFERRED) | `src/ui_text.cpp:130-147` |
| the second skill bar | reads controller-mode state; in keyboard mode the skills bound to F1 to F8 stand in its place | `src/skill_bar_service.cpp` |
| the tab highlight | has never run: it builds an empty focus | `src/tab_highlight.cpp:20` |
| the character row's "A: Play, X: Delete" | written for the pad's buttons | `src/focus_tracker.cpp:140-143` |
| the scroll bar and its arrows | no reader: the cursor says "ScrollBar", "UpArrow" (MEASURED) | item 3 |
| `PluginIconTooltip` on the character's row | no reader: the cursor says its layout name (MEASURED) | the loader's own icon |

The cursor names a place itself when no reader does (`[cursor] no reader
named it; said "..."`). **Every such line in the log is a reader to fix.**

**A point in the skill tree:** A on a skill that can be raised spends a
point (READ: the button sends `UseSkillPoint`, and the panel a message 0x3B to
the server). **With Shift held the message differs** (its flag is 0xFFFF; what
the server does with it is OPEN), so Shift+A on a skill is not to be advised
before it is known.

## 6. Ctrl + Shift + left click

The loader takes one modifier, so this cannot be a key of the loader's. It is
the game's gesture for "move", where an item has two places to go. INFERRED:
it works already through the key watch. **To measure:** `[menu-keys] left
click came by watching the key` after Ctrl+Shift+A.

## 7. Where the pointer rests in play

Built, between the HUD's two skill buttons. In question: 17 seconds after the
pointer was put there the second test has `World target -> "Experience: ..."`,
the experience bar's tooltip. If a menu's closing is followed by
"Experience" every time, the place is wrong: try the HUD's click catcher
(`BGClickCatcher`) where no widget is drawn over it.

## 8. Home and End

Built on the mini menu's buttons in the HUD, not pressed in either test. The
log line is `[cursor] menus: ... button found / not shown / has no
rectangle`. The game's own keys open each menu: C, I, T, Q, O, Esc.

## 9. Character creation: built, with one question open

**READ** (`re/actions/charcreate/notes.txt`):

- **The name box takes what is typed at any time**, wherever the pointer is
  and with no click: its layout sets `alwaysAcceptsKeyInput`. Nothing begins
  the typing and nothing ends it.
- Out of controller mode the game never says a box is being edited (the
  global the mod's `TextEditing` reads has one writer, on the controller's
  path). INFERRED 0.85.
- A class is a rectangle: a click anywhere in it chooses the class. A click
  on the class that is chosen puts it back. A click while the figure steps
  forward or back is lost. Choosing another class puts the first back.
- **Enter creates the character**, the same as a click on Create, once a
  class is chosen and the name is allowed: 2 to 15 letters, no digits, no
  space, one hyphen or underscore at most and not at either end. Offline the
  game starts at once.
- Esc puts the chosen class back; with none chosen it leaves the screen.
- A name that is taken opens `DismissableModal` ("That character name is
  already taken"); a hardcore character asks in `ConfirmationModal`.
- The version dropdown offers Classic, Expansion and Reign of the Warlock,
  by the class chosen and by what is owned. `ToggleLegacy` switches to the old
  graphics and rebuilds the screen from the legacy layout: not to be pressed.

**As built:** with the cursor on a box of text the keys that type are the
box's and the left and right arrows move in the text; up and down leave it.
A class is called by its name in the game's language, "checked" after it when
it is chosen; A on it chooses it, and once the game has chosen it the class
is said with the description the screen shows. A on the class that is chosen
says so and clicks nothing.

**The way through, for the third test:** arrows to a class, A; the class and
its description are said; arrows to the name box ("Character Name, text
field"); type; Enter.

**OPEN, the question that decides a key:** does the loader hold the mod's
keys back from the game on this screen? Its README says the plugin's keys do
not run "while chat, another text field, or the binding control is active",
and this box is always active.

| what the third test shows | what it means |
|---|---|
| A on a class chooses it, and the name stays empty | the loader delivers A and holds it back. Nothing more is needed |
| A on a class chooses it, and an "a" is in the name (`[field] focus name="InputText" ... text="a"` when the cursor comes to the box) | the loader does not deliver A there; the key watch clicked and the game typed. **A key that types nothing is needed for the click on this screen, and it is the user's to choose** |
| A on a class does nothing | neither the loader nor the key watch acted; read `[menu-keys]` |

## 10. Sort, in a trade

`NativeSort::Sort` refuses while `TradeLayout` is open: `SortPage` takes
another path then, which was not read. Low priority; single player has no
trade.

## The order to build in

1. **The third live test**, by the list in `keyboard-play.md`.
2. Item 3, scrolling: without it a long list of options is not reached.
3. Item 9's key, if the test asks for it, once the user has chosen.
4. Item 5: whatever the log shows the cursor naming for itself.
5. Items 4, 6, 7, 8 as the log decides them.
