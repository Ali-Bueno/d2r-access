# Keyboard play

Playing the game from the keyboard alone, with no controller connected: what
is built, what the two live tests of 2026-09-27 measured, and what the next
has to measure. How the character is made to act is in `keyboard-actions.md`.
What is not built is in `keyboard-unbuilt.md`; every key is in
`keyboard-keys.md`.

**MEASURED** means the running game or its log showed it. **READ** means the
code or the game's data says so. **OPEN** means not established.

## The keys, as built

In play:

| key | what |
|---|---|
| arrows (held) | walk |
| A / W (held) | fight with the left / right skill; a ranged fight keeps its distance |
| Y | swap weapons |
| Shift+S | the list of skills for the left hand; the game's S opens the right hand's |
| Tab, Shift+Tab | next / previous monster |
| D | walk up to the nearest usable thing and use it |
| Shift+Home | walk to what the scanner named |
| H | walk to the nearest unexplored room |
| Ctrl+R | retreat: walk back to the way you came into the level, and stop beside it |
| End | stop |
| Ctrl+Y | the ways out of this room |
| Z, X | life, mana. There is no key for both |
| Ctrl+G | radar on or off |

Everywhere:

| key | what |
|---|---|
| PageUp, PageDown | the pad's right stick: objects in play; detail and controls in a menu |
| Ctrl+PageUp, Ctrl+PageDown | its other axis: categories in play; skill lines and item size in a menu |
| Shift+PageUp, Shift+PageDown | compare: the previous, the next tooltip the game draws (highlighted item first, then each equipped one; stops at either end) |
| Home | say it again |
| Ctrl+S, then PageUp / PageDown, Space | the settings list |

In a menu:

| key | what |
|---|---|
| arrows | move the mod's cursor: to the nearest control that way, and by item inside a grid. A held arrow repeats |
| left, right | on a slider or an option that steps: change its value. On a box of text: move in what is typed |
| A | left click where the cursor is |
| D | right click where the cursor is (W until the user moved it, 2026-09-27) |
| Shift+A, Ctrl+A, Shift+D | the same click with the key held, which the game sees: Shift + left click equips, Ctrl + left click sells, moves or drops |
| Y | swap weapons |
| Shift+S | the list of skills for the left hand, or shut it |
| backslash | say it again |
| Ctrl + up, down | the line before, the line after, of what the cursor is on |
| Ctrl + left, right | its first line, its last line |
| PageUp, PageDown | the tab before, the tab after |
| Home, End | the menu before, the menu after: character, inventory, skill tree, quest log, game menu |
| Ctrl+PageUp, Ctrl+PageDown | as before: skill lines, item size |
| Shift+PageUp, Shift+PageDown | as before: compare |
| Ctrl+R, Shift+R | repair all, repair (a merchant who repairs) |
| Alt+T | transmute (the Horadric Cube) |
| Alt+S | sort the grid the cursor is in; the player's inventory when it is in none |
| Alt+F | fill the belt from the inventory, while an inventory is open |

In play PageUp, PageDown, Home and End are what they were (the user,
2026-09-27: they are tab keys "when a menu is open. otherwise they are still
the navigation keys"). In a menu backslash says it again: Home was its key.

## What the first live test measured

Session of 2026-09-27 09:40, keyboard mode, no controller, loader 1.3.0-beta.
The log of that session is kept in the workspace, outside this repository, as
`re/logs/keyboard-first-live-test-2026-09-27.log`.

| finding | evidence |
|---|---|
| The game's functions were verified and the hook installed | `[actions] Verified; the game's own commands are logged.` |
| The loader accepted all 21 keys, including the six the game has bound itself | `[keyboard] 21 of 21 keys registered`; no `refused` and no `is bound to` line |
| **The arrow keys walk the character** | the user; `[actions] run to: 5820,4716` and onward |
| Our commands and the game's run on the same thread | `game command 0x03 ... thread=10000 ui_thread=10000` |
| A walk goes through the game's own click path | `game command 0x03 ... from RVA 101F78`, which is inside `MoveToPoint` |
| A begins a fight and **the game's own A does not fire** | `[keyboard] fight: hand=left skill=0` twice at 09:41:04-05, and no `CharacterStatsPanel` opened |
| W uses the right skill and **the game's own W does not fire** | `skill 104 is not an attack: used once`, `command 0x0C`; no `[weapon-swap]` line |
| D walks up to an NPC and talks | `use: nearest id=11 ... Warriv`, `command 0x2F a=1 b=11`, then `OpenPanel NpcDialogPanel` and `"Warriv. talk"` |
| **I does open the inventory; nothing says so** | `OpenPanel PlayerInventoryExpansionLayout` seven times, followed by `[focus] no focused widget while "PlayerInventoryExpansionLayout" is active` and no `[window]` line |
| **In keyboard mode almost no panel has a focused widget** | 27 `no focused widget while ...` lines: title screen, main menu links, inventory, mercenary, skill select, skill tree, mini menu |
| Two screens do have one | character select (`ListItem0`), the NPC menu (`ListItem0`), and the pause menu, where focus moved only between `LootFilter` and `Chronicle` |
| **The arrows walk the character with a menu open** | `OpenPanel SkillsTreePanel` at 09:41:51.5, `[actions] run to` at 09:41:52.6 |

So consumption works: a key the mod handles does not reach the game, for the
two keys that were pressed. Tab, H, Z and Space were not pressed in that
session and are OPEN.

### Why the inventory is silent and the arrows walk through menus

Both come from the same thing. The mod was built for controller mode, where
the game keeps a focus cursor in every panel and opens a menu switcher
(`UISwitcher`) with the menu tree. In keyboard mode the game does neither
(MEASURED, above):

- The readers speak what has focus. With no focused widget there is nothing
  to speak, so a panel opens in silence.
- `RadarUi::InPlay` (`src/radar_ui.cpp`) calls the game "in play" unless a
  `UISwitcher`, a panel that classifies as Menu or Narrative, a `PauseLayout*`
  or `SettingsPanel` is open. `PlayerInventoryExpansionLayout` and
  `SkillsTreePanel` are none of those in keyboard mode, so the play keys stay
  live behind them (READ, and MEASURED for the skill tree).

The item-by-item and nearest-skill steps of that build waited for the game to
move its own cursor and then finished the move. In keyboard mode the game has
no cursor to move, so they never ran: the session has no `[keys]` line. They
were replaced by the mod's own cursor, below.

## What the second live test measured

Session of 2026-09-27 12:18 to 12:21, keyboard mode, no controller, the build
of `f64d6ff`. Its log is kept in the workspace, outside this repository, as
`re/logs/keyboard-second-live-test-2026-09-27.log`. **Three faults it showed
were fixed afterwards and have not been tested.**

| finding | evidence |
|---|---|
| The sort, the swap, the belt's rows and the belt's fill all match the running game | `[sort] Verified.`, `[swap] Verified.`, `[belt-rows] Verified.`, `[belt-fill] Verified.` |
| The loader took every key | `[keyboard] 28 of 28 keys registered` |
| **The pointer lands where it is put, at once** | every `[cursor] on ... placed=1 pointer X,Y` has the middle of its rectangle, read in the same call |
| The cursor takes the menu as it opens, and lists it | `[cursor] taken: a menu is up and no pad is connected`, `[cursor] menu: MainMenuPanel ... -- 17 things`, `18 places, 0 not reached` |
| **The readers read the cursor as they read the focus** | `[focus] CharacterSelectPanel/CreateCharacter (ButtonWidget) -> "Create New, button"`, and the character's row as "Gordan, Level 7 Paladin" |
| **On the main menu the loader does not deliver A**; the key watch did | `[menu-keys] left click came by watching the key: the loader did not deliver it here (menu: MainMenuPanel ...)` |
| Five clicks on the character's row began no game; **Enter did** (the user) | `[cursor] left click on ListItem0` five times, the last at 12:19:20.036; `CharacterSelect:SelectCharacter` at 12:19:20.415, then `FrontEndNavigation:ToGame`. READ: one click only chooses a row; two within 500 ms, or Enter, act on the row chosen |
| **FAULT, fixed `ee44174`: in the game, the game was never in play** | `[radar] status=dialogue` from 12:19:22 on with no menu open; `[survey] request ignored: not in play`; A and D logged as `left click on where the pointer is`. `HireablesPanel`, the mercenary's portrait on the HUD, was in the list of menus of the play gate, and it is open for the whole game |
| **FAULT, fixed `f841d56`: a click went to the wrong button** | the main menu's bar of tabs is drawn at y = -130; the cursor began on it, the pointer stayed at 1719,937, and A clicked what was there: `[cursor] left click on Tabs`, then `"going to CharacterCreate"` |
| **FAULT, fixed `a946d64`: panels of play were in the cursor's menu** | `[cursor] menu: HireablesPanel MessageLogPanel_800x600 CandidateListPanel -- 3 things`, the three a scroll bar and its arrows |
| A place with no reader is named by the cursor, by its layout name | `[cursor] no reader named it; said "UpArrow"`, and "Druid", "Offline", "Online" |
| The pointer is put between the skill buttons when a menu closes | `[cursor] play goes on: the pointer to 962,1042`. 17 seconds later: `World target -> "Experience: 31,952 / 32,886"`. OPEN whether the place is over the experience bar |

Not pressed in this test: Y, Alt+F, Alt+S, backslash, the tab keys, Home and
End in a menu. No inventory, skill tree, quest log or options panel was
opened, so **the cursor in a grid of items, the belt and the readers of items
are still unmeasured.**

## What the third live test measured

Session of 2026-09-27 18:12 to 18:20, keyboard mode, no controller, the build
of `472d872`. Its log is kept in the workspace, outside this repository, as
`re/logs/keyboard-third-live-test-2026-09-27.log`. **Nothing was built after
it.** The whole account, with the table of where each arrow leads on
character creation, is at the top of the workspace's `HANDOFF.md`.

| finding | evidence |
|---|---|
| Every native module matches the game, the five new ones among them | `Verified.` after `[skill-hand]`, `[skill-icons]`, `[values]`, `[classes]`, `[rows]`; `[keyboard] 29 of 29 keys registered` |
| **The character walks** | 69 `[actions] run to` lines; no `not in play` line in the log |
| The character fights, picks a monster with Tab, walks to what the scanner named, retreats | `fight: hand=left skill=0`, `left skill on: command 0x06`; `target id=16`; `walk to id=2 ... ended: arrived`; `retreat: to Blood Moor` |
| D talks to an NPC and the NPC's menu works | `use id=11 ... name=Warriv`, `left click on ListItem0`, the speech read |
| Y swaps, Z and X say life and mana | `[swap] the game's swap key handler: ran`, `[weapon-swap] set=1`; `[vitals] report life=40 mana=100` |
| **A on a character begins the game** | `left click, twice on ListItem0`, `CharacterSelect:SelectCharacter` 277 ms later |
| **The hand of the list of skills is said, and A puts a skill on it** | `the list of skills is open for the right hand; said "Right Mouse-Button Skill"`; `left click on ButtonTemplate3`, then `fight: hand=right skill=97` |
| The list's icons hold the skills the code said they hold | `skill id=220 "Tome of Town Portal"` and seven more, each by its name |
| **FAULT: on character creation seven of fifteen places were never reached**, the name box among them | the `[cursor] on` lines: ToggleLegacy 14 times, the info button 10, the dropdown 9, Druid 8, Warlock 5, Sorceress 4, Create 3, Paladin 1; none for the rest |
| **FAULT: a class is silent for 450 ms** and only "Druid" was ever said | `[cursor] no reader named it; said "Druid"` three times, 430 to 460 ms after the landing; no stay on a class lasted over 590 ms |
| **FAULT: the game version is not read** | `"Game Version, dropdown"` with no value; `"Create a '%s' version of your character., button"`; `[value] ... GameVersionDropdown -> ""` |
| **FAULT: after a dropdown closes the cursor is on the screen's first place** | `[cursor] on ToggleLegacy` after each of three |
| NOT A FAULT (the user, 2026-09-28): the arrows walk the character while the mod's settings list is open. The list steps with PageUp and PageDown | `[keyboard] settings: Settings. Auto detail off` at 18:19:54.772, `[actions] run to` at 18:19:56.088 |
| FAULT: a skill of the list is read as the number on its icon, or not at all | `[interaction] SkillSelect -> "4"` on the Tome of Town Portal |
| FAULT: scroll bars and their arrows are places | the cursor on `UpArrow`, `ScrollBar`, said by those names |
| OPEN: a weapon swap the mod was not asked for | `[weapon-swap] set=2` at 18:19:38.713, no `[swap]` line before it |
| OPEN: what was said where the log has no line | the cursor's "nothing that way" and some readers speak without a log line |

## The menus: how they are built

The user's decisions, 2026-09-27:

- *"the mod can just control the mouse like the diablo hellfire mod does."*
  After the second test: *"I absolutely love the mouse pointer system, that
  was even better than I expected. it makes controls that aren't even
  accessible with the controller accessible"*.
- Left click is A and right click is D (W at first; the user moved it).
- PageUp, PageDown, Home and End are tab keys only while a menu is open.
- *"we don't actually care whether the game's cursor works in keyboard mode ...
  the mod should simply control the mouse like the diablo hellfire mod does,
  placing it on each item as that mod does"*, for the inventory, the skill
  tree and the stash, missing no slot.
- The buffers stay: the character panel, the mercenary, items and skills line
  by line, on Ctrl with the arrows.
- Sort, repair all, selling and whatever else the controller offers get a key
  of the mod's where the game expects a click on an icon.

### The pieces

| piece | file | what it does |
|---|---|---|
| the cursor's logic | `src/menu_cursor.*` | pure, tested. A step inside a grid is `GridJump::Move`; a step past its edge, or from a control, lands on the nearest thing that way (below); a step into a grid enters at the cell nearest to where it came from |
| the steps made | `MenuCursor::Mend` | a place the arrows would miss is given a step of its own from the nearest place that is reached, and a place that does not lead back is given one to a place that does. The log names each: `[cursor] a step made for what the arrows miss` |
| the hand of the list of skills | `src/native_skill_hand.*` | asks the game which hand its list of skills is open for (`0x238240`) |
| the skills' icons | `src/native_skill_icons.*` | the skill an icon of the skill tree or of the list stands for, and its name in the game's language |
| values | `src/native_value_controls.*` | a slider's value and its two arrows, an option's state and its two arrows |
| the classes | `src/native_class_choice.*` | whether a class of character creation is chosen, and the description the screen shows |
| rows that are chosen, then acted on | `src/native_list_rows.*` | a character's row: one click chooses it, two act on it. The cursor chooses the row it comes to, so Enter acts on that row, and A clicks twice |
| where skills stand | `tools/gen_skill_tree.py`, `src/generated/skill_tree.h` | the page, row and column of every class's skills, from the game's tables; the cursor's tests walk every page |
| the cursor in the game | `src/keyboard_menu.*` | lists the open menu's controls and grids, moves the cursor, places the pointer, clicks, switches tabs and menus |
| the mouse | `src/menu_mouse.*` | the pointer on a rectangle; a click as a press and a release some frames apart (`SendInput`) |
| the stand-in for focus | `NativeUi::SetCursor` | `CurrentFocus` and `FocusSelection` give the mod's cursor while it is set, so every reader reads it as it reads the controller's focus |
| the gate | `RadarUi::MenuPanel` | the inventory, skill tree, stash and the rest are menus by name; in keyboard mode no `UISwitcher` opens with them. **A panel that is open in play must never be in its list** (the second test) |
| the weapon swap | `src/native_swap.*` | Y runs the handler of the game's own swap key (key action 44, `0x11BF00`); 700 ms later a set that has not changed is said to be unavailable |
| the belt's rows | `src/native_belt_rows.*` | the slots of the belt worn, whether it is drawn in full, the game's "show belt" turned on while the belt is in the menu, the belt's own hit test for the point to click |
| the belt's fill | `src/native_belt_fill.*` | Alt+F makes the calls the belt's handler makes when the controller's hold completes |
| the keys in the hints | `src/click_words.*` | pure, tested. "Ctrl + Left Click to Sell" is said as "Ctrl + A to Sell" while the keyboard has the menu, in the game's thirteen languages |
| the pointer at rest | `Park` in `src/keyboard_menu.cpp` | when a menu closes and play goes on, the pointer is put between the HUD's two skill buttons |
| a click refused | `Land`, `Click` in `src/keyboard_menu.cpp` | a place drawn off the screen is not a place; a click is refused, and said to be unavailable, when the pointer did not go to where the cursor is |
| lines | `InputDirection::PostLineStep`, `ReadLines` in `src/interaction_reader.cpp` | Ctrl with an arrow walks an item's or a skill's tooltip a line at a time, with the game's Ctrl view (an item's ranges) on; Shift with an arrow walks it as the game draws it with Shift held, the regular tooltip; on the character panel and Advanced Stats it is the right stick, which walks their lines already |

### The nearest thing that way

The user, 2026-09-27: every element is to be reached by single presses of an
arrow, the cursor snapping to the nearest: *"on the controller you have to
move diagonally ... right arrow from row 1, column 1 should go to row 2,
column 2"* when row 1 has no skill in column 2. That is the Paladin's combat
page: Sacrifice, then Holy Bolt a row lower in the next column, with Smite
further along Sacrifice's own row.

The rule (`Nearest` in `src/menu_cursor.cpp`), in this order:

1. Only what lies that way at all: its middle is further that way than the
   middle of where the cursor is.
2. What lies **more that way than to one side** comes first. The space
   between the two things along the way is set against the space between
   them across it.
3. Of those, a thing of the **group** the step is made from comes first. The
   skills of a skill tree are a group, and so are the skills of the list for
   a hand: the tabs of the skill tree are drawn right over its first row,
   and without this a step to the right from the first skill lands on a tab.
4. Then the nearest, by the two spaces together.

So right from Sacrifice is Holy Bolt, and down from Sacrifice is Zeal, the
next skill of its own column: Holy Bolt lies more to the side than down.
**The same two steps do not always lead back**: left from Holy Bolt is Zeal,
not Sacrifice, because the third row is drawn a little nearer to the second
than the first is. No memory of the last step is kept to make it lead back:
with one, a place reached only by that step could be missed.

Tested (`tests/menu_cursor_tests.cpp`): every skill, tab and button of every
page of every class is reached from every other, at four sizes of icon, with
no step made for any; sixty inventories beside a stash filled at random; four
hundred menus strewn at random, mended where the arrows alone fall short.

### READ in the game's code (3.3.93787), none of it measured live

- **The pointer.** `0x14F690` clamps only in controller mode (`0x77E10` and a
  flag). Otherwise it stores the point as the game's mouse position
  (`0x2A886F0`), turns it into window pixels (`0x7F210`) and calls `0x120A280`,
  which is `ClientToScreen`, `LogicalToPhysicalPoint` and `SetCursorPos` (the
  three import slots matched against user32's exports). So it is the game's
  own "set the mouse position", the same thing the Diablo mod's `SetCursorPos`
  is, and it takes the coordinates the focus rectangles are in.
- **Every focusable class uses the base select.** 51 widget vtables hold
  `0x876CA0` at +0xA0 and none overrides it, the inventory grid (`0x1CF3800`),
  the equipment slot and the belt among them. So the rectangle read
  (`NativeSkillNavigation::RectAt`) accepts them all.
- **A grid cell's rectangle** (`0x2C47B0`) is one cell's. With an item in hand
  it is moved by the item's size, to where the pointer has to be for the item
  to begin on that cell. An item of several cells gets the pointer in its
  middle, from its first and last cell.
- **Sort has no button.** No layout names it; the only trace is the string
  `ControllerPromptHoldToSortInventory`. It is a function, below.

### READ in the layouts and string tables

| act | mouse mode |
|---|---|
| pick up, put down | left click |
| use, read, open, buy | right click |
| equip, unequip | Shift + left click |
| sell, drop, move to the stash or the cube | Ctrl + left click (the item's own tooltip says which) |
| repair, repair all | the buttons `button_repair`, `button_repair_all` of `VendorPanelLayout` |
| transmute | the button `convert` of `HoradricCubeLayout` |
| withdraw gold, drop gold | the buttons `gold_withdraw`, `gold_button` |
| open a menu | the mini menu's buttons in `HUDPanel`: `MiniMenuCharacter`, `MiniMenuInventory`, `MiniMenuSkillTree`, `MiniMenuQuestLog`, `MiniMenuPauseMenu` |

Ctrl + Shift + left click (move, where there are two places to move to)
cannot be a key of the loader's, which takes one modifier. OPEN.

### OPEN, and what in the log answers it

The next live test, the third, is the first with the game in play and the
menus built. Copy the log before the next launch; it does not always append.
**At load the log has to say** `[keyboard] 30 of 30 keys registered` (29 before J, 2026-09-28), and
`Verified.` after each of `[skill-hand]`, `[skill-icons]`, `[values]`,
`[classes]` and `[rows]`. An `Unsupported layout` line names what is switched
off.

| question | the log line |
|---|---|
| Is the game in play with no menu open? | no `[survey] request ignored: not in play`, no `[radar] status=dialogue`, and `[actions] run to` when an arrow is held |
| Does Shift+S open the list for the left hand? | `[cursor] left skill: the HUD's button is clicked`, `OpenPanel SkillSelect`, `[cursor] the list of skills is open for the left hand; said "..."` |
| Is a skill named, in the list and in the tree? | `[cursor] skill id=.. ".."` on each move; then an `[interaction]` line with the tooltip, or `[cursor] no reader named it; said "<the skill>"` |
| Are the skills of the tree places? | `[cursor] thing SkillButton3 0,0 (ButtonWidget) rect ...`, ten to a page; the size of the rectangle is the icon's |
| Was a step made for a place? | `[cursor] a step made for what the arrows miss: from X right to Y`. None is expected in the skill tree |
| Does an arrow change a slider or an option? | `[cursor] value of X: 50 -> 55 by the game on the key` or `by a click on its arrow`, and the value heard once |
| Which character does Enter begin with? | with two characters: the arrows to the second, `[cursor] row 1 is chosen as the cursor comes to it; the list had 0`, Enter, and the character the game begins with |
| Does A begin the game on a character? | `[cursor] left click, twice on ListItem0`, then `CharacterSelect:SelectCharacter` in a `[ui]` line |
| Does A choose a class? | `[cursor] left click on Paladin`, then `[cursor] class Paladin chosen after N ms; said "..."`. `was clicked and is not chosen` means the click was lost |
| Is an "a" typed into the name by the click? | `[field] focus name="InputText" ... text="..."` when the cursor comes to the name box: the text has to be empty. See `keyboard-unbuilt.md`, item 9 |
| Does Enter create the character? | `CharacterCreate:Create` in a `[ui]` line, then `going to Game` |
| Does the pointer go where it is put? | MEASURED yes. `[cursor] the pointer did not go to ...` names a place it did not reach, and a click there is refused |
| Does Y swap? | `[swap] the game's swap key handler: ran`, then `[weapon-swap] set=...`; `[swap] the set in use did not change in 700 ms: refused` means the game or the server said no |
| Are the belt's rows there? | `[belt-rows] show belt: ran, shown 0 -> 1`, then `[cursor] thing Belt (BeltWidget) ... cells 4xN`, and for each move `[cursor] belt slot N; the belt takes the point for slot N`: the two numbers have to be the same |
| Does Alt+F fill? | `[belt-fill] sent, N planned`, the game's own sound, and the potions in the belt afterwards; `nothing to fill` and `refused` are the game's answers |
| Is a hint said with the mod's keys? | "Ctrl + A to Sell" heard on an item at a merchant |
| Does the game show a tooltip for what the pointer is on, and is it captured? | an `[interaction]` line with the item's name after the `[cursor] on` line; `[cursor] no reader named it; said "..."` means no reader spoke within 450 ms |
| What is in each menu, and is everything reached? | `[cursor] menu:`, one `[cursor] thing` per control, then `[cursor] N places, M not reached`; `NOT REACHED by the arrows` names what the arrows miss |
| Does the loader deliver the keys with a menu open, and on the title screen? | `[menu-keys] ... came by watching the key` means it did not, and the game's own use of that key was not held back |
| Do the tabs switch? | `[cursor] tab: click on ...`, then a `[tabs]` or `[window]` line |
| Do Home and End find the mini menu? | `[cursor] menus: open N, to M (...), button found / not shown / has no rectangle` |
| Does a click do what the mouse does? | `[cursor] left click ... on <name>`, then what the readers say next |

INFERRED, to be listened for:

- A quest is clicked as the cursor lands on it, since the quest log fills its
  text on a click. A list row (a character, a waypoint) is not clicked on
  landing: A does that.
- On character select, the NPC menu and the pause menu the game keeps a focus
  of its own in keyboard mode. The mod's cursor is read in its place while it
  is set. Whether the game also moves its own focus with the arrows there is
  not measured.
- Sliders and dropdowns: a click opens a dropdown, whose rows are then the
  whole menu. The left and right arrows change a slider and an option that
  steps (`keyboard-unbuilt.md`, item 2).
- The belt is the HUD's. It is added to the menu while an inventory is open.

### Sort

No layout has a sort button and none sends a sort message, in either mode.
The controller's "Hold to Sort" is in engine code. READ, confidence 0.95, not
measured; the working files are in the workspace, `re/actions/sort`:

- The string is used by the inventory grid's prompt updater (`0x2C8C80`,
  vtable `0x1CF3800` +0xE8). The same class's message handler (`0x2C6BD0`,
  +0x20) ends a completed hold of pad action 4 with three calls:
  `owner = GetOwner(grid)` (`0x2A7810`), `CanSortPage(owner, page)`
  (`0x15A680`), `SortPage(id, page)` (`0x160B00`); the page is the grid's byte
  +0x630 and the id its +0x5C4.
- The controller check is in the handler alone. `CanSortPage` refuses with an
  item in hand, with the merchant's buy-and-sell cursor, and for a page that is
  not the player's own or a shared stash tab's.
- `SortPage` lays the page's items out again, does nothing if one does not
  fit or nothing moves, and sends the server one message (0x4E) with every
  item of the page and its new cell. The server refuses a count that does not
  match, so no item leaves its page.
- With a trade open it takes another path, which was not read through:
  `NativeSort::Sort` refuses while `TradeLayout` is open.

`src/native_sort.*` makes the same three calls with the same values from the
same grid, after checking all three functions byte for byte. The log line is
`[sort] grid <name> owner id=.. type=.. page=.. -> sorted | refused | failed`.
**The first press in the game is the measurement**: what the readers say is
under the cursor afterwards, and the `[grid]` census lines before and after.

### The weapon swap

READ, not pressed in a test; the working files are `re/actions/swap`.

- The game's table of key actions (`0x22A7930`, records of 0x18 bytes) holds
  `0x11BF00` for action 44, which the `.key` file binds to W. It takes no
  argument. It asks whether input is free (`0x13DE60`) or the inventory is
  open, and whether the character is free to act (`0x14B380`: not in the
  middle of a blow, a throw, a kick or a skill), reads the set in use from
  the registry and jumps to the swap with it.
- The swap is `0x1E33A0`, `void(uint8 set in use)`. **It is not without an
  argument**: the server compares the set it is given with its own and
  refuses one that differs. It sends message 0x50 in single player too. With
  a trade open it swaps on the client alone (`0x15B920`), as the game's key
  does. `docs/weapon-swap.md` calls the two paths "local game" and
  "otherwise"; the test is the trade window (`UIVisible(0x16)`).
- Neither function checks for a player: the mod looks the player up first.
- The controller check is in the dispatcher that calls the handler
  (`0x11FF30`), not in the handler.
- OPEN: whether the first question passes with a panel other than the
  inventory open. If Y says "Unavailable" in the skill tree, that is why.

### The belt

READ, not tried in a test; the working files are `re/actions/belt`.

- A belt has 4, 8, 12 or 16 slots by the belt worn (`belts.txt`);
  `BeltWidget::Update` (`0x228DB0`) writes the number to the widget's +0x6A4
  every frame. slot = column + 4 * row, and row 0 is the bottom row.
- The rows above the bottom one are drawn, and take a click, only while the
  belt is drawn in full (+0x6A0): while the pointer is over the belt, or
  while UI state 0x1A is on, which is what the game's "show belt" key
  toggles (`0x11B1D0`, no argument, nothing with no belt worn). The mod turns
  it on while the belt is in the cursor's menu and off when the menu closes,
  and leaves alone one the player turned on.
- A slot's rectangle holds the space between the cells and, drawn in full,
  the belt's cap, and a click counts only on the cell in its top left. The
  belt's own hit test (`0x226C60`) is asked which point is the slot's.
- In the cursor's grid the top row is row 0, so the cursor's cell is turned
  into the game's slot (`Selection` in `src/keyboard_menu.cpp`). An empty
  slot says "Empty" and where it is, its row counted from the bottom one.
- **The fill's plan is not written inside the handler**, as was first read:
  the handler `0x2283E0` calls `0x386FF0`, which makes the whole plan, and
  `0x0ECA70`, which sends message 0x5D. The controller check is the
  handler's first call and is in none of the functions the mod calls. The
  server makes the plan again with the same slot finder and refuses the
  whole message, before any potion moves, if one entry differs.
- With the mouse a potion goes from the inventory to the belt by Shift +
  left click (READ 0.8), which is Shift+A; a left click on a belt slot picks
  the potion up or puts the one in hand down, a right click drinks it, Shift +
  right click gives it to the mercenary.

## A far walk: the remembered map

The game keeps collision only near the player and frees it again
(`docs/room-routing.md`), so a walk that plans on the loaded floor alone
knows a different level at every room border.

MEASURED 2026-10-01, the Far Oasis, the walk to the Maggot Lair (13:03 to
13:07): the orders went north along x 5204 to about y 4355, then south to
about y 4402, then north again, over and over. The scanner's distance to the
lair shows why. Going south it was a walking distance (237, 221, 211, 207
steps): the lair's surroundings were loaded and the route was the true one,
back out and round. Going north it was the straight line (141, 125, 109
steps; 124, 100 on an earlier pass): they were not loaded, and the room
graph's guess led straight at the lair, into ground from which it is about 50
cells off in a straight line and 283 steps on foot (13:07:23, near 5222,4310). MEASURED OFFLINE on a fixture of that shape, the repo's own routing:
292 turns in 300 looks, never arriving.

`src/level_route.h` keeps every room's floor as the game shows it, for the
level (the last eight levels of the game being played), and finds the way to
a place over that:

1. over ground seen open only, while such a way exists;
2. failing that, over ground not seen yet as well: walking toward an unseen
   place;
3. never over ground seen shut.

`KeyboardService::Advance` uses it when the loaded floor does not reach the
thing walked to, and heads for the last cell of that way the loaded floor
reaches. The room graph leads only when the map has no way either. What was
seen stays seen, so a look can only rule ways out. The floor is learnt twice
a second in play, walk or no walk, so ground walked past with the arrows is
known to a later walk.

Log: `[keyboard] walk: led by the remembered map to NAME at x,y: N cells from
x,y, all of it seen|part of it ground not seen yet; now to x,y`, at most one
every three seconds.

Cost, MEASURED OFFLINE: learning 25 loaded rooms 0.17 ms; one search of a
400 x 400 level (the Far Oasis) 3.6 ms, made when the map or the place
changes, not on every look.

Not done: the beacon and the scanner's distances still know only the loaded
floor, so the scanner says the straight line to a thing whose surroundings
are not loaded, and that number jumps when they load (100 steps, then 250,
in the walk above). Nothing is kept from one game to the next.

## What cannot be reached: the filter and the look before a walk

MEASURED 2026-10-02, the Arcane Sanctuary (level 74), 07:06 to 07:16: its
walkways are joined by teleport pads alone. The scanner listed every pad
loaded (14, then 18, then 20) and 80-odd monsters. Of 62 walks begun, 45
ended in `no route`, 34 of them to a pad. One to the pad at 25336,5507, begun
at 07:10:06 from 25333,5461, ran to 25328,5507 and said "No way" there four
seconds later, eight cells from the pad. That pad and the one at 25336,5517
are a pair: after each use of one the other was at no steps (07:08:23 to
07:08:27). H ended in `no route` three times, to an unexplored room
(07:11:27, 07:11:28, 07:13:08).

Built the same day, on the remembered map above: `LevelRoute::Reach`, the
ground the player can get to on foot from where they stand, by one search of
the level's map from the player:

- over ground seen open, over ground not seen yet, and through shut doors (a
  door can be opened);
- a thing is **cut off** when no cell the body stands on was got to as near
  it as a walk counts as there (three cells and half its footprint); a room,
  when none of its cells was;
- a thing outside the level's own rooms, and everything when the player
  stands nowhere the body fits, is **unknown**, which is never treated as cut
  off.

So only what the game has shown to be shut all the way round is called cut
off. A place past ground not seen yet stays reachable until that ground is
seen; in a level only partly loaded some things that cannot be reached are
still listed. The search is made again only when the map changes or the
player is on other ground (past a teleport pad).

Used in three places (`KeyboardService::CutOff`, `NoWay`):

1. **The scanner's filter, Ctrl+U**, and in the Ctrl+S list; off until
   switched on, kept between sessions as `reach-filter`. On, the scanner
   leaves out what the loaded floor has no steps to and the map shows cut
   off. Log, when what is left out changes: `[survey] reach filter: N of M
   cut off from x,y and left out: NAME@x,y ...` (the first twelve by name).
2. **A walk, before its first step** (Shift+Home): cut off, it is not begun
   and says "No way to NAME". Log: `[keyboard] walk: not begun, no way on foot
   from x,y to NAME at x,y`. The same is asked at every look of a walk under
   way, so a thing found cut off as the map loads ends the walk there and not
   at the edge nearest it (`walk: no way on foot from ...`). Always on.
3. **H**: rooms cut off are passed over when the nearest unexplored room is
   chosen. With none left that can be reached it says "Nothing left to explore
   here". Always on.

D is unchanged: it already takes only what the loaded floor has steps to. A
walk with a shut door in the way begins as before.

A shut door is told by the DOOR bit (0x0800) with no WALL or NOPLAYER on the
cell, READ in `docs/collision-semantics.md`, not measured in the game: no log
kept so far holds a cell with that bit.

Cost, MEASURED OFFLINE: the search 2.6 ms on a 400 x 400 level, 13.8 ms on
920 x 920 all open (the Arcane Sanctuary's box is 900 x 900); asking about one
thing after it, nothing that can be measured.

**Not tested in the game.** Since the section below, all three count the ways
through the teleport pads as well.

## Teleport pads: the ones back across, where each leads, and routes through them

The user, 2026-10-02, after a town portal back into the Arcane Sanctuary: "I
could not find a teleport pad that took me to the next part of the dungeon.
the teleport pads simply looped around and around", and comma / full stop said
"Nothing to use here" at a pad the walk had just reached.

**MEASURED in the log of 08:37-09:52** (the `[radar-collision]` snapshots):

- Every pad used with D stands on open floor, all eight cells round it open.
  Seven pads stand on a shut cell (0001) at the very end of a walkway, the
  drop beyond, three cells open beside them: 25385,5447; 25265,5507;
  25207,5510; 25225,5537; 25145,5387; 25077,5500; 25085,5517. Not one was used.
- A gap is crossed by a floor pad on one side and an edge pad on the other:
  25396 landed by 25385, 25276 by 25265, 25207,5516 by 25207,5510, 25156 by
  25145. Going out from the waypoint takes floor pads; coming back takes edge
  pads. From 09:34 the player went round four pairs of floor pads; the walk
  reached the edge pad at 25145,5387 at 09:37:58 and full stop said "Nothing to
  use here" five times (the same at 25077,5500 from 09:35:34).
- H said "Nothing left to explore here" fourteen times, 14 rooms unexplored,
  `59 cut off`: the filter's search counted every room past a pad as cut off.

**Why the edge pads were not offered, READ:** D and comma/full stop took only
a thing with a cell within 1 that the body fits on; at a walkway's end there is
none (25144,5387 is open, but the body's cross needs 25145). The walk counts
within 3 as arrived, and the scanner, with no steps to it, says the straight
line ("2 steps").

**Where a pad leads, READ in the game** (`re/D2R_image.exe`, 3.3.93787): the
operate table at `0x1423A51C0`, entry 27 at `0x14058EAD0` (objects.txt
TeleportPad1-4, classes 192, 304, 305, 306, the only rows on handler 27). The
pad keeps no destination; it sends the player to another object of its class,
one in its own room first, onto a free cell within 3 of it (mask 0x1C09).
**READ in the level files** (`Act2/Arcane/sanct*.ds1`, extracted with
`tools/casc_extract.exe`; objpreset Act 2 rows 2-5 are TeleportPad1-4): 42 pads
in 53 files, only in the "2" variant of a room, always two of a class. The room
at 25060,5420 (`sanctNSW2.ds1`) holds the three pairs measured there, told
apart by class alone; `sanctEW2.ds1` holds 25156,5387 and 25145,5387 to the
cell. So a pad leads to the only other pad of its class in its room
(`src/pad_links.h`); where a room held other than one, nothing would be assumed
until the pad was used. What a pad was seen to do wins over that.

**Built:**

- **The edge pads** (`UseSteps`): D and comma/full stop take a teleport pad
  from as near as a walk counts as there (3 cells) when no cell within 1 holds
  the body. Everything else is counted as before.
- **The pads read** (`Entity::padClass`, set for handler 27) are kept with
  the level's map, from the scanner's read twice a second and the walk's looks.
  Log, when one is new: `[keyboard] pads: level L, N known, M with a
  destination, K of them seen used`.
- **"Teleported"**: after D uses a pad, a jump of more than 4 cells between
  two reads a quarter second apart, within 2 s, is said, and logged against
  where the pad was worked out to lead: `[keyboard] teleported: pad at x,y
  class C, landed x,y beside the pad at x,y; expected the pad at x,y, by the
  rule: as expected | NOT as expected | nothing to compare`. No jump:
  `[keyboard] pad at x,y class C used, and no teleport seen`.
- **The search through pads** (`LevelRoute::Reach` with links): ground got to
  within 3 of a pad gets to the ground within 3 of where it leads, a pad
  counting as one step; each cell keeps the first pad of the shortest way. Made
  only in a level with a pad whose way is known, and asked only of what walking
  alone does not get to.
- **A walk, H, Ctrl+R**: where only the pads get to a thing, the walk goes to
  the first pad of the way and stops there: "Arrived at Teleport Pad". The pad
  becomes D's choice, as if full stop had chosen it. D then uses it and the mod
  says "Teleported"; the walk key again goes on from there. The user: "the
  player should still have to stop and interact with it with d. but it's fine
  if the route has to split into legs accounting for each pad". The walk never
  uses a pad itself, and while under way it asks only walking alone, so it
  cannot run to the edge of a gap. H takes a room past the pads only when none
  is left that walking alone gets to, the nearest by the steps of the way. Log:
  `[keyboard] walk: by teleport pad at x,y, the first of the way to NAME at
  x,y`; `[keyboard] explore: room R by teleport pad at x,y, N steps`.
- **Ctrl+U** leaves out only what neither walking nor the pads get to: "if the
  teleport pads can reach other teleport pads, then display them all" (the
  user).
- A walk that arrives at any teleport pad makes it D's choice.

Cost, MEASURED OFFLINE: the search through 42 pad links 3.6 ms on a 400 x 400
level, 19.1 ms on 920 x 920 (walking alone 2.8 and 14.5 ms in the same run).

**NOT measured:** that the game uses an edge pad when D is pressed from the
walkway's end (it checks no floor in handler 27, and the edge pads are the only
way back, but no use of one is in any log); that the class read from a client
unit is the server's. The `teleported` lines answer both.

**Not tested in the game.**

## The retreat: Ctrl+R, back the way in

MEASURED 2026-10-02 08:39, the Arcane Sanctuary, come into by its waypoint at
25448,5448, life at 48 percent: Shift+R said "The way back is not known" (it
looked only for stairs to the level left, 40, and a waypoint leads to none);
Ctrl+R said "No way out found" (it looked only at the rooms round the player,
and the waypoint was some 110 cells off, on the scanner's list all along);
Alt+R said "Retreating to passage", `0 cells`, and "Arrived" where the player
already stood.

The user, the same day: "collapse retreat to control+r autowalks immediately
to the way the player entered from. everything else is dangerous and failure
prone. this includes if the player entered via waypoint, control+r should
retreat to the waypoint. the goal is to simply give the player some breathing
room to heal, not to attempt to perform complex calculations".

Built:

- **The way in** is taken at the first look in each level (`NoteWayIn`):
  where the player first stands, and the way out standing within 10 cells of
  that place -- stairs, a waypoint, a portal, an area border. One that leads to
  the level just left is taken before one that merely stands nearer. Looked for
  again twice a second while the player is still there and nothing was found.
  Log: `[keyboard] way in: NAME at x,y, N cells from where the player came in
  at x,y (level L, from F)`.
- With nothing found there, **the place itself** is the way in, called "the
  entrance". So there is always somewhere to go.
- **A hop** -- the player more than 20 cells from where they stood at the last
  look, half a second before -- is a teleport pad taken (or the Teleport skill).
  Where the player lands is kept as the way onto that ground, with the thing
  within 3 cells of it, the pad itself. Log: `[keyboard] hop from x,y to x,y`,
  then `[keyboard] way in, hopped onto: Teleport Pad at x,y ...`. MEASURED the
  same day at 08:44: from 25378,5446 the filter called the waypoint cut off, the
  pads at 25385 and 25396 between.
- **Ctrl+R** walks there as Shift+Home walks to what the scanner named: the
  same look at the whole way first, the same legs, the same remembered map
  (`SetOut`, `Walk`). It stops beside it and does not take it; D does that. An
  area border is walked back to where it was crossed. A portal that has closed
  is walked to where it stood. Where only the pads get back to the way in, it
  walks to the first pad of the way (the section above). When the way in is cut
  off on foot after a hop and no pad is known to lead back, it walks to the pad
  hopped onto instead (`[keyboard] retreat: no way on foot
  from ... the way in; to the way onto this ground`); D on that pad goes back.
  Log: `[keyboard] retreat to NAME at x,y, what the player came in by|what the
  player hopped onto|...nothing found there`.
- **Taken out:** Shift+R and Alt+R as retreats, and the retreat planner that
  priced the ground by the monsters and looked for corridors
  (`src/retreat.cpp`, its test). Shift+R is still repair in a merchant's menu,
  and Ctrl+R repair all; in play Shift+R and Alt+R are the game's.

Known limits: only the last hop is kept, so the way back is one pad at a time,
which is what D on the pad does anyway. A hop is a jump of more than 20 cells,
and about half the pad pairs of 2026-10-02 are nearer than that (6 to 18 cells):
past one of those no hop is kept. The walk never takes a pad itself. If
the map does not show the way in cut off yet when Ctrl+R is pressed, the walk
heads for it and stops with "No way to NAME" once the map does; a second Ctrl+R
then goes to the pad. Ctrl+R in the first half second of a level, before the
first look, says the terrain is unavailable.

**Not tested in the game.**

## Found on the way

- `TabHighlight::Tick` builds an empty focus (`NativeUi::Focus()`) where it
  means `NativeUi::CurrentFocus()`, so it has never run (`src/tab_highlight.cpp:20`).
  Not changed here: it is the controller's reader.
- A compare key was lost about two times in three: a posted left-stick push
  was dropped by the next poll, 11 ms later, and its reader looks every 33 ms.
  A posted push is now kept for 150 ms.

## Carried over, not yet tried in the game

Tab and Shift+Tab, H, the three retreats, End, Shift+Home, Ctrl+Y, Z and X,
Ctrl+G, the settings list, and the ranged fight's falling back. All built and
deployed; none pressed in the first test, or not told apart in it.

Known limits, unchanged: a shut door blocks a walk; the reach of a ranged
attack is one distance for every skill (24 subtiles); the second skill bar's
reader and the belt's automatic potion name read controller-mode state.

## Retreat through a monster pack — 2026-10-03

The user's own log records Ctrl+R at 11:24:53, targeting the way in,
`To The Canyon of the Magi` at 22722,14430. Native run orders repeated a
destination at 22695,14313 while the player remained near 22677,14312.
The collision/stall report includes monster footprints and blocked centers;
the walk reported blocked at 11:24:57, before the death screen at 11:24:59.
This establishes an engaged retreat and failed movement, not a missed key.

Previously both routing and straight movement legs used terrain collision,
which deliberately strips transient actors. That kept choosing a line through
the pack. Retreat now uses the same `Beacon::FindRoute` and `AutoWalk` with
their additive Movement-layer option. The target is still the remembered
entrance or first teleport pad. No alternative escape destination or weighted
retreat planner is introduced. Terrain-only exploration and reach filtering
keep their existing defaults.

Retreat rejects diagonal corner squeezes and checks the whole native body.
When a monster blocks an issued destination, the next permitted order can
choose an open detour without waiting for the 1.2-second stall timeout. The
250-ms order limit remains; actual game animation/hit recovery still controls
when movement can occur. A completely surrounded body has no proved route.

`auto-walk` regression coverage includes a moving blocker, a surrounded
character and the anonymous 57x57 collision capture from 11:24:53. The capture
is a nearby route test, not proof of survival or the complete distant entrance
route. User-led live confirmation remains necessary.
