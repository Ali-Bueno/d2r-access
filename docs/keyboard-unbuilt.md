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

## Walking: the user's rule of 2026-10-01, and where the walk does not keep it - AUDIT ONLY, NOTHING HERE CHANGED

**The user, 2026-10-01:** "click to move should essentially be as smooth as a sighted player
clicking tiles. it should never click anywhere off screen, it should only click valid tiles the
player can move to and it should calculate the path to the objective in on-screen hops. ... if
it's not, don't rewrite it this session. document anything that doesn't work that way and we'll
fix it later."

How the walk works (READ, `keyboard_service.cpp`, `auto_walk.cpp`, `beacon.cpp`,
`native_actions.cpp`): the whole route is found first, over the floor the game has loaded (up to
150 cells round the character). It is then walked in hops. Each hop is one call of the game's own
click-to-move (`MoveToPoint`) on the farthest cell of the route, at most 20 cells off, that the
body walks to in a straight clear line. The next hop is given when the character has gone more
than 6 cells or is within 6 of the last one, about every 0.6 s at a run, while it is still running.

**1. Never off screen: kept, by a screen size that is a guess.**
- READ: no order is farther than 20 cells (a walk, a retreat), 10 (the arrows), 14 walking steps
  (a ranged fight's fall back), or 5 past an area border. The mod's own screen
  (`CombatTarget::Screen`) is 33 cells either side and 37 up and down along the screen's axes,
  and a point 20 cells off is inside it in every direction.
- Does not keep it: that screen size is INFERRED, never measured (its own comment says so), and no
  order is tested against it; they are inside only because 20 is small. The console's
  `access-actions run <dx> <dy>` orders any offset (a test command).

**2. Only tiles the character can move to: kept by the mod's own test, which is not the game's.**
- READ: every order is a cell of a route proved on the loaded floor with the body fitted, with a
  straight clear line to it (`AutoWalk::Straight`).
- Does not keep it, MEASURED 2026-10-01: 96 times in one session two logged orders of a walk were
  2.0 s and 8 cells apart, 24 of them in ten minutes by the dune in the Far Oasis. The character
  ran about 8 cells and stood (mode 1 at 13:05:16, 13:05:21 and 13:05:26) where an unbroken run
  covers 12 to 14 cells a second; the arrows crossed the same ground at that speed (13:05:32 to
  13:05:35). INFERRED:
  an order given while the character is running stops it there, and it stands until the walk
  gives the order again 1.2 s later. And each time that walk turned round, the game refused the
  order for a second or more with every cell round the character open (`stuck:` lines, 13:05:07
  to 13:07:45, the character standing between two cells).
- **What the game did with those orders is NOT KNOWN.** `MoveToPoint` replaces the cell ordered
  with where the game's own path ends and sends that; nothing logs what it sent. The measurement
  to make next: in `native_actions.cpp` the hook on the game's command already sees it, so log
  the cell the game sent whenever it is not the cell ordered, with the character's position.
- Does not keep it, READ: the straight line is tested on the terrain layer, which leaves out
  monsters, the mercenary and other players, and the game's own test does not.
- Does not keep it, READ: a walk orders up to 20 cells and the arrows 10. `collision-semantics.md`
  (from D2MOO, not checked in the D2R image) has the game use its own route search only for a
  cell within 18, and a straight trace with wall following beyond.
- Does not keep it, READ: `Beyond` orders a cell 5 past an area border that nothing tested; `Halt`
  is an order to the character's own cell; and when a place cannot be reached the route leads to
  the reachable cell nearest it, against the wall (the door task below).

**3. The path in on-screen hops: kept for the loaded floor; past it the way was a guess.**
- READ: hops are at most 20 cells and each is a straight clear line, so each is one click a
  sighted player could make.
- Did not keep it, MEASURED 2026-10-01: for a thing the loaded floor does not reach, the route
  led toward a room chosen by the room graph, a guess that changed at room borders; the walk to
  the Maggot Lair went back and forth for four minutes. `level_route.h` (built 2026-10-01 before
  the user's rule was given, `1c0fe18`, deployed 13:41, NOT tested in game) replaces the guess
  with the way over every part of the level the game has shown: `keyboard-play.md`, "A far walk".
  The hops themselves are unchanged by it.
- Does not keep it: the beacon and the scanner still use the room graph and the loaded floor only.

**"As smooth as a sighted player clicking": not yet.** The 8-cell runs and the refusals under 2
are what is not smooth, and their cause is the thing to measure first.

## Item lines: Shift read a jewel's ranges - 2026-09-30, FIX DEPLOYED 06:55, NOT TESTED IN GAME

The evidence is in the workspace's `HANDOFF.md`, top section.
- **The user:** Shift+arrows on a jewel read the ranges ("+(5-10)% Enhanced Damage"); only Ctrl
  should. The user's facts: the game never shows ranges with Shift, and Ctrl was held only after
  the bug was found. MEASURED (06:24 log): J read "+6%" before and after.
- READ: Shift read a separate copy of the tooltip kept from earlier, not the one on screen. What
  put the ranges into that copy is NOT KNOWN.
- **Built:** the copy is gone; Shift+arrows read the tooltip as drawn at the key, as Ctrl+arrows
  and the compare readout do. Shift+Right landing on the size is by design (the user asked for
  that fix to be reverted).

## NOT STARTED: a walk hops in place against a wall when a closed door is in the way - 2026-10-01

**The user:** "the player can become stuck against a wall if the mod can't find a clear route and will simply hop in place. ... this happens most often when there is a closed door and the mod doesn't account for that along the path but simply tries to path straight to the objective. It never stops until a manual movement key is pressed. the mod should account for closed doors and just alert the player that a door is in the way that needs to be opened instead of trying to path through a wall when the closed door path is rejected."
**Wanted:** a walk whose way is shut by a closed door stops and says a door is in the way and has to be opened; it never walks at a wall and never goes on until a key stops it.
**Where to start (READ, not a diagnosis):** `Beacon::FindRoute` returns the way to the reachable cell nearest the place when the place cannot be reached, which is a cell against the wall; a shut door's cells carry the DOOR bit, a wall to the router; `Advance` never asks whether the wall is a door; the room graph joins rooms by nearness. HANDOFF.md, top section of 2026-10-01, has the rest. Find the hopping in a log first.

## Hire mercenary screen in keyboard mode - 2026-09-30, REBUILT 2026-10-01, BEING TESTED BY THE USER

**The user:** "hire mercenary screen doesn't work with the keyboard support. reads only skill slots and doesn't even read those properly. mercenary name doesn't highlight correctly, stat buffer and compare stats buffers don't work." Asked for it to be documented as a priority first.
**MEASURED (04:40:42-04:40:52 log, `HireMenuPanel` opened from the NPC's "hire"):**
- `[cursor] menu: NpcDialogPanel HireMenuPanel -- 42 things`. The places are only leaves: `YourGoldTooltipWidget`, then for each of 8 rows (y 197..554, step 51) `Skill0/1/2 (HirelingSkillIconWidget)` 32x32 at x 411/445/478 plus `GoldCostTooltipWidget` at x 521, then `CloseButton`. **No place is the row itself**, so there is no place for the mercenary's name.
- Up/Down moved the pointer along the `Skill2` column (478,197 -> 478,554) and each landing spoke `native fallback HireMenuPanel/Skill2 -> "Skill2"`: the layout name, no reader. The pointer sits on a skill icon, not the name, which is why the game's own highlight looks wrong (INFERRED: the row highlights from the mouse hover over the icon's part of the row).
- No `[hire]` line at all in the session.
**READ (the cause of the buffers):** `src/hire_stats.cpp` `Tick` runs only when `NativeUi::Focus` is a `ListItem` in `HireMenuPanel` -- the controller's focus. Keyboard mode has no focus (`keyboard-mode-has-no-focus`), so the skills/stats buffer and anything keyed off the focused row never run. Its walk is the left stick with Y held (`PadFilter::ArmLeftStickWithY`), a pad-only gesture.
**Fix, in order:**
1. The cursor's places in `HireMenuPanel` are the ROWS (the ListItem each row's skill icons sit in), not their leaves; the pointer rests on the row's name part (left of the icons) so the game highlights that row. Keep Close and the gold line.
2. A reader for a row: the mercenary's name and what the row draws (cost, level), as the game writes it.
3. `HireStats` fed by the cursor's row in keyboard mode (not only the focus), and walked with the keyboard's buffer keys (Shift+arrows item lines, Ctrl+arrows the ranges view -- `keyboard-key-decisions`), plus the compare buffer against the current mercenary.
**Step 1 DEPLOYED 05:39, 95/95 ctest, NOT TESTED IN GAME (2026-09-30, `keyboard-local`, uncommitted):** `Collect` in `src/keyboard_menu.cpp` stops at a `ListItem` in `HireMenuPanel`, so the row is one place and `Thin()` no longer drops it for holding the icons. INFERRED, not measured: `NativeUi::CurrentFocus()` prefers the cursor, so with the row as the place `HireStats` and the row reader get the row as they do on the pad. The first deploy failed while the game was running; redeployed 05:39. **Next: test the hire list** and check the log for `[cursor] thing ... (ListItemWidget)` rows and what the row says. 
**Steps 2-3 (keyboard buffers) DEPLOYED 05:52, 95/95 ctest, NOT TESTED IN GAME (2026-09-30, uncommitted):** `HireStats::Tick` takes a `KeyboardTick` path while `KeyboardMenu::Active()`; the controller path is unchanged. Record 0 is the highlighted mercenary: the tooltip the game draws for the row if it draws one (NOT measured whether the mouse on a row draws one in keyboard mode), else the row's `HireName`/`HireLevel`/`HireCost`, then each skill as `MercenaryStats::Skills(row)` reads its icon's tooltip text (READ: same `HirelingSkillIconWidget` class and offsets as the mercenary panel; untested on the hire list). Any other tooltip record drawn beside it is record 1 and on. Shift/Ctrl+arrows walk the lines of the record read last; Shift+PageUp reads record 0 in full, Shift+PageDown the next record, and it stops at the last one; with only one record it says "none". Log: `[hire] record N, M lines: ...`, `[hire] compare record ...`, `[hire] record N line M -> ...`. **The log's `[hire] record` lines will show what the game actually draws.** The user plans to redo this properly later with mouse hover, as on the mercenary panel (user, 2026-09-30); this is a stopgap.
Nothing above is measured beyond the log lines quoted. Fix only this screen (`scope-fixes-to-the-screen-reported`).

**2026-10-01, BUILT, 95/95, NOT TESTED IN GAME (the user: "the player should be able to highlight the individual skill icons and read them like tooltips ... the confirm button to replace a mercenary after hiring one also can not currently be highlighted"):**
- A row's skill icons and its price are places again, beside the row: the row's own place is its name and level (left of the first icon), where the pointer now rests. Right from a row: Skill0, Skill1, Skill2, the price. An icon is said by its skill's name (the whole tooltip with details read unasked), at once; Shift or Ctrl with an arrow walks that tooltip's lines. The two gold lines say the game's own words and the amount. Coming to an icon chooses its row. A on an icon or a price says it again and clicks nothing; A on the row hires.
- MEASURED (12:19:28 log): the question asked before a mercenary is replaced is two rows of the NPC's dialogue, 480 wide at 169,282 and 169,306, drawn over the hire list. While the dialogue has places and the hire list is open, only the dialogue's are the cursor's. What the two rows say is not measured; the next log's `[cursor] thing` and `[text] NpcDialogPanel` lines will show.
- HANDOFF.md, session 2026-10-01, section 3 has the evidence.

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
