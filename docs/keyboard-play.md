# Keyboard play

Playing the game from the keyboard alone, with no controller connected: what
is built, what the first live test of 2026-09-27 measured, and what the second
has to measure. How the character is made to act is in `keyboard-actions.md`.

**MEASURED** means the running game or its log showed it. **READ** means the
code or the game's data says so. **OPEN** means not established.

## The keys, as built

In play:

| key | what |
|---|---|
| arrows (held) | walk |
| A / W (held) | fight with the left / right skill; a ranged fight keeps its distance |
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

In a menu (built after the first test, not yet tried in the game):

| key | what |
|---|---|
| arrows | move the mod's cursor: control to control, and by item inside a grid. A held arrow repeats |
| A | left click where the cursor is |
| W | right click where the cursor is |
| Shift+A, Ctrl+A, Shift+W, Ctrl+W | the same click with the key held, which the game sees: Shift + left click equips, Ctrl + left click sells, moves or drops |
| Ctrl + up, down | the line before, the line after, of what the cursor is on |
| Ctrl + left, right | its first line, its last line |
| PageUp, PageDown | the tab before, the tab after |
| Home, End | the menu before, the menu after: character, inventory, skill tree, quest log, game menu |
| Ctrl+PageUp, Ctrl+PageDown | as before: skill lines, item size |
| Shift+PageUp, Shift+PageDown | as before: compare |
| Ctrl+R, Shift+R | repair all, repair (a merchant who repairs) |
| Alt+T | transmute (the Horadric Cube) |
| Alt+S | sort the grid the cursor is in; the player's inventory when it is in none |

In play PageUp, PageDown, Home and End are what they were (the user,
2026-09-27: they are tab keys "when a menu is open. otherwise they are still
the navigation keys"). **"Say it again" has no key in a menu now**: Home was
its key.

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
| the gate | `RadarUi::MenuPanel` | the inventory, skill tree, stash and the rest are menus by name; in keyboard mode no `UISwitcher` opens with them |
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

The second live test is the first time any of this runs. Copy the log before
the next launch; it does not always append.

| question | the log line |
|---|---|
| Does the pointer land where it is put? | `[cursor] on <name> ... placed=1 pointer X,Y`: the pointer's own position after the move |
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
  controls. A slider has no key to change it yet.
- The belt is the HUD's. Its bottom row is added to the menu while an
  inventory is open; the rows above it are not.
- The pointer stays where the last menu left it when play goes on.

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

The belt's "hold to fill" (`ControllerPromptHoldToPopulateBelt`) is the
controller's alone too. Its work is written inside the belt's handler
(`0x2283E0`), with no function of its own to call. Not built.

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
