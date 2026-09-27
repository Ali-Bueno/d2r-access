# Keyboard play

Playing the game from the keyboard alone, with no controller connected. This
is the state of the work after the first live test, 2026-09-27: what is built,
what that test measured, and what has to be built next. How the character is
made to act is in `keyboard-actions.md`.

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

**The reading keys above are about to move.** The user has given PageUp,
PageDown, Home and End to tab switching in menus; see "What is asked for next".

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

The item-by-item and nearest-skill steps (`src/keyboard_menu.cpp`) wait for the
game to move its own cursor and then finish the move. In keyboard mode the
game has no cursor to move, so they never ran: the session has no `[keys]`
line. They are built on the wrong assumption for this mode and have to be
driven by the mod's own cursor instead.

## What is asked for next

The user, 2026-09-27, after the test:

1. **The arrow keys follow the screen that is open.**
   - *"on the title screen, they need to allow the player to move between all
     elements of the screen. EG: left from the character list should jump to
     the play button. left from that should jump to the menu with things like
     cinematics, options, credits, exit"*
   - *"in any other menu, the arrow keys must navigate that menu. EG: the
     escape menu, the inventory, the skill tree etc. right now they are only
     moving the character regardless of what UI element is active"*
   - *"make sure the arrow keys navigate the quest log as well"*
2. **Keys to switch tabs.**
   - *"page up and page down to switch between tabs on the options screen"*:
     the tabs inside a menu that has them.
   - *"home and end to switch between the broader tabs like options,
     inventory, quest log etc"*.
3. **I must bring up the inventory**, which it does; what is missing is that
   the mod says so and lets the arrows move in it.

Standing requirements that bear on this, from the same day:

- In a grid of items the arrows move by item, not by cell: onto an item's
  top-left cell, then past the rest of its cells; an empty cell is one step.
  In the skill tree they go to the nearest skill. *"be absolutely sure no
  items or skills are skipped"*. The logic is built and proved
  (`src/grid_jump.h`, `SkillGrid::Jump`); it needs the mod's own cursor.
- Every function the controller reaches has to be reachable: selling,
  repair all, sorting, picking up and placing, spending points.
- Nothing spoken gives a position in a list or a count.

## Where the next session starts

### 1. A cursor of the mod's own, for keyboard mode

The game gives no focus, so the mod keeps one: the widget the arrows are on,
per open panel. What exists to build it from:

- `NativeUi::Snapshot(panel, focus)` and `NativeUi::FindControls(panel, kind)`
  list a panel's controls, hidden ones included; `NativeUi::Visible` and
  `NativeSkillNavigation::FocusRect` give what is shown and where.
- `MenuNavigationPolicy::Move(nodes, selected, dx, dy)` already steps through
  controls by their rectangles. It was written for the loader's multiplayer
  form (`src/menu_navigation.cpp`) and is the model.
- `GridJump::Move` for item grids, with `NativeUi::GridSize` and
  `NativeUi::GridItemAt` to read the grid; `SkillGrid::Jump` with
  `SkillNavigation::Nodes` for the skill tree.
- The readers take a `NativeUi::Focus {panel, widget}`. Fed the mod's cursor
  instead of `NativeUi::CurrentFocus()`, they would read it as they read the
  controller's. Where `CurrentFocus` is called is the list of places to change.

The mouse follows this cursor and clicks for it; see section 2, which the
user decided.

**OPEN, and the first thing to measure:** whether
`NativeSkillNavigation::Select` and `SelectAt` give a widget focus in keyboard
mode. READ: they place the game's cursor (`0x14F690`, documented as the
controller cursor) and call `FocusableWidget::Select` (`0x876CA0`). If that
works without a controller, the game itself then holds the focus and the
readers need no change. The pause menu did show a moving focus in keyboard
mode, so a focus can exist there. One call and one log line settle it.

### 2. Acting on what the cursor is on: the mouse (DECIDED)

**The user, 2026-09-27:** *"the mod can just control the mouse like the diablo
hellfire mod does. that should be the easiest."* This settles the question
that was open here. The mod keeps its own cursor, puts the game's mouse on
what that cursor is on, and clicks. The game's internal press-a-button
function is not used and the question about it is closed.

**How the Diablo mod does it (READ, `D:\Games\Dev\Custom\Diablo`):**

- It keeps its own place, `Slot`, and moves it with the arrows
  (`InventoryMoveFromKeyboard`, `Source/controls/plrctrls.cpp:2408`).
- After each move it puts the mouse on the middle of what it landed on:
  `SetCursorPos(mousePos)` (`plrctrls.cpp:1336`, and a dozen more call sites
  for the stash, the spell list and the buttons). For an item of several cells
  that is the middle of the whole item, found from its first slot
  (`FindFirstSlotOnItem`, `plrctrls.cpp:1327`).
- `SetCursorPos` (`Source/diablo.cpp:2794`) sets the game's own
  `MousePosition` and, in keyboard-and-mouse mode, moves the real pointer too
  (`SDL_WarpMouseInWindow`).
- An action key then calls the game's own handler for a click, which acts on
  whatever is under the mouse: `PerformPrimaryAction` (`plrctrls.cpp:2862`)
  calls `LiftInventoryItem` -> `CheckInvItem`, `CheckChrBtns` for a stat
  point, `ActivateStashButton` for a stash button.

So the pattern is three steps: the mod's cursor moves, the mouse follows it,
a key clicks where the mouse is.

**What D2R offers for each step:**

| step | what exists | state |
|---|---|---|
| where a control is | `NativeSkillNavigation::FocusRect(widget)`, and `ReadRectAt` for one cell of a grid (`src/native_skill_navigation.cpp`) | READ; the inventory grid's own rectangle function is `0x2C47B0`, which takes column and row |
| put the mouse there | the game's `0x14F690`, already called by `SelectNative`; `docs/controller-navigation.md:57`: it "updates the native cursor and OS position" | READ. **OPEN: whether it works in keyboard mode**, and whether it clamps to bounds that only hold in controller mode. Windows' own `SetCursorPos` is the fallback, and then the rectangle has to be turned into screen coordinates |
| click | `SendInput` with a mouse button, as `tools/computer_use` does for its tests; or the game's own mouse handlers (table at `0x142293A70`: left down `0x140083FC0`, left up `0x1400839A0`, right down `0x1400839E0`, right up `0x140083B00`; these are the handlers for a click in the WORLD, from the static pass in `re/actions/input`, not re-checked, and a click on a panel may go another way) | READ, neither tried from the plugin. `SendInput` needs the game's window in front, which it is while the player plays |

**What comes for free with the mouse (INFERRED, to be measured):** the game
draws a tooltip for whatever the mouse is over, and the mod already captures
every tooltip the game draws (`src/native_tooltips.cpp`, a hook on the draw).
So an item under the mouse may read itself without any focus at all. The
hover also gives the game's own highlight, which is what a click then acts on.

**What the mouse covers:** choosing a button, switching a tab (a tab is a
button), picking up and placing an item, equipping, the belt, selling and
buying, repair and repair all, the stash and its gold, transmuting, spending a
stat or skill point, binding a skill, the quest log's tabs and entries, the
waypoint list, the title screen's buttons. In keyboard mode these are all
mouse actions to the game already.

**What to decide or measure when building it:**

- Left click and right click are different acts in keyboard mode, and a click
  with Shift or Ctrl held is another again. What each does in each panel has
  NOT been read or measured here; the game's own hint strings
  (`game-data/local/lng/strings/ui.json`) are where to read it. The keys for
  them are the user's to choose, with the reading keys of section 4.
- The real pointer moves, so a sighted helper would see it jump. Nobody has
  asked for it to be hidden or put back.
- Move the mouse only while a menu is up. INFERRED, not measured: in play the
  game picks the unit under the mouse as its own target (`SelectUpdate`), and
  the keyboard's walking and fighting do not use the mouse at all, so there
  is nothing to gain by moving it there and something to disturb.

### 3. The gate

`RadarUi::InPlay` has to say "not in play" for the keyboard-mode panels. The
panels seen open in the test: `PlayerInventoryExpansionLayout`,
`HirelingInventoryPanel`, `SkillsTreePanel`, `SkillSelect`, `NpcDialogPanel`,
`PauseLayoutGarden`. `MiniMenuPanel` is open during play and must not count.
One gate for every control that acts on the world is the rule of the project
(the user, 2026-09-21); the fix belongs in that function, not in each service.

### 4. The keys that have to move

PageUp, PageDown, Home and End are now tab keys in menus. That takes them
from the reading keys **in menus**; in play they can stay what they are.

| key | in play, as built | in a menu, as asked |
|---|---|---|
| PageUp, PageDown | scanner: previous / next object | previous / next tab of this menu |
| Home | say the scanner's object again | previous of the broader tabs |
| End | stop | next of the broader tabs |
| Ctrl+PageUp, Ctrl+PageDown | scanner: category | OPEN |
| Shift+Home | walk to the scanner's target | OPEN |

Left without a key in menus by this: detail, controls, skill lines, item size
and points, compare, and saying it again. The settings list also steps with
PageUp and PageDown. **Which keys these get is the user's to say.** Keys the
game has not bound (READ from the character's `.key` file): D, J, K, L, U, X,
Y, Insert, Delete, and every chord.

The tab keys themselves need the act of section 2: a tab is a button, and the
mouse clicks it.

### 5. Carried over, not yet tried in the game

Tab and Shift+Tab, H, the three retreats, End, Shift+Home, Ctrl+Y, Z and X,
Ctrl+G, the settings list, and the ranged fight's falling back. All built and
deployed; none pressed in the first test, or not told apart in it.

Known limits, unchanged: a shut door blocks a walk; the reach of a ranged
attack is one distance for every skill (24 subtiles); the second skill bar's
reader and the belt's automatic potion name read controller-mode state.
