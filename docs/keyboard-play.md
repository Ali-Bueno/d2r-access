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

**OPEN, and the first thing to measure:** whether
`NativeSkillNavigation::Select` and `SelectAt` give a widget focus in keyboard
mode. READ: they place the game's cursor (`0x14F690`, documented as the
controller cursor) and call `FocusableWidget::Select` (`0x876CA0`). If that
works without a controller, the game itself then holds the focus and the
readers need no change. The pause menu did show a moving focus in keyboard
mode, so a focus can exist there. One call and one log line settle it.

### 2. Acting on what the cursor is on

**OPEN, and the user's decision.** The arrows can move and the readers can
read without it, but choosing a button, picking up an item, selling, spending
a point all need an act. The ways known:

- The loader's `WidgetService::dispatchUiAction`, which sends a panel message
  such as `TradePanelMessage:Confirm`. The layouts name the message each
  button sends. No game code is called. Covers buttons, not item grids.
- The game's own click path, as the walking and fighting use it.
- The game's internal press-a-button function (`0x1401452E0`), which its
  tests use in controller mode. It imitates no device, but it maps a key to a
  pad button, which the user ruled out on 2026-09-20. **Asked twice, not
  answered. Not to be built without a yes.**

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

The tab keys themselves need the act of section 2: a tab is switched by the
message its button sends.

### 5. Carried over, not yet tried in the game

Tab and Shift+Tab, H, the three retreats, End, Shift+Home, Ctrl+Y, Z and X,
Ctrl+G, the settings list, and the ranged fight's falling back. All built and
deployed; none pressed in the first test, or not told apart in it.

Known limits, unchanged: a shut door blocks a walk; the reach of a ranged
attack is one distance for every skill (24 subtiles); the second skill bar's
reader and the belt's automatic potion name read controller-mode state.
