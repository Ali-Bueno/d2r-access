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
| Tab, Shift+Tab | next / previous monster |
| D | walk up to the nearest usable thing and use it |
| Shift+Home | walk to what the scanner named |
| H | walk to the nearest unexplored room |
| Ctrl+R, Shift+R, Alt+R | retreat to the nearest way out / the way in / the nearest corridor or doorway |
| End | stop |
| Ctrl+Y | the ways out of this room |
| Z, X | life, mana. There is no key for both |
| Ctrl+G | radar on or off |

Everywhere:

| key | what |
|---|---|
| PageUp, PageDown | the pad's right stick: objects in play; detail and controls in a menu |
| Ctrl+PageUp, Ctrl+PageDown | its other axis: categories in play; skill lines and item size in a menu |
| Shift+PageUp, Shift+PageDown | compare: the item in full, the one equipped |
| Home | say it again |
| Ctrl+S, then PageUp / PageDown, Space | the settings list |

In a menu:

| key | what |
|---|---|
| arrows | move the mod's cursor: control to control, and by item inside a grid. A held arrow repeats |
| A | left click where the cursor is |
| D | right click where the cursor is (W until the user moved it, 2026-09-27) |
| Shift+A, Ctrl+A, Shift+D | the same click with the key held, which the game sees: Shift + left click equips, Ctrl + left click sells, moves or drops |
| Y | swap weapons |
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
| A click on the character's row, the fifth, started the game | `[cursor] left click on ListItem0` at 12:19:20.036, `"going to Game"` 400 ms later. Why not the first four is OPEN |
| **FAULT, fixed `ee44174`: in the game, the game was never in play** | `[radar] status=dialogue` from 12:19:22 on with no menu open; `[survey] request ignored: not in play`; A and D logged as `left click on where the pointer is`. `HireablesPanel`, the mercenary's portrait on the HUD, was in the list of menus of the play gate, and it is open for the whole game |
| **FAULT, fixed `f841d56`: a click went to the wrong button** | the main menu's bar of tabs is drawn at y = -130; the cursor began on it, the pointer stayed at 1719,937, and A clicked what was there: `[cursor] left click on Tabs`, then `"going to CharacterCreate"` |
| **FAULT, fixed `a946d64`: panels of play were in the cursor's menu** | `[cursor] menu: HireablesPanel MessageLogPanel_800x600 CandidateListPanel -- 3 things`, the three a scroll bar and its arrows |
| A place with no reader is named by the cursor, by its layout name | `[cursor] no reader named it; said "UpArrow"`, and "Druid", "Offline", "Online" |
| The pointer is put between the skill buttons when a menu closes | `[cursor] play goes on: the pointer to 962,1042`. 17 seconds later: `World target -> "Experience: 31,952 / 32,886"`. OPEN whether the place is over the experience bar |

Not pressed in this test: Y, Alt+F, Alt+S, backslash, the tab keys, Home and
End in a menu. No inventory, skill tree, quest log or options panel was
opened, so **the cursor in a grid of items, the belt and the readers of items
are still unmeasured.**

## The menus: how they are built

The user's decisions, 2026-09-27:

- *"the mod can just control the mouse like the diablo hellfire mod does."*
- Left click is A and right click is W.
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
| the cursor's logic | `src/menu_cursor.*` | pure, tested. A step inside a grid is `GridJump::Move`; a step past its edge, or from a control, is `MenuNavigationPolicy::Move` over everything else; a step into a grid enters at the cell nearest to where it came from |
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
| lines | `InputDirection::PostLineStep`, `ReadLines` in `src/interaction_reader.cpp` | Ctrl with an arrow walks an item's or a skill's tooltip a line at a time; on the character panel and Advanced Stats it is the right stick, which walks their lines already |

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

The next live test is the first with the game in play and the menus built.
Copy the log before the next launch; it does not always append.

| question | the log line |
|---|---|
| Is the game in play with no menu open? | no `[survey] request ignored: not in play`, no `[radar] status=dialogue`, and `[actions] run to` when an arrow is held |
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
- Sliders and dropdowns: a click opens a dropdown, whose rows are then
  controls. A slider has no key to change it yet (`keyboard-unbuilt.md`).
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
