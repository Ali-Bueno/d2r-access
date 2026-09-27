# Keyboard play: what is not built

Written 2026-09-27 for the next session. The user, the same day: *"those are
very important and need to be built before actual play."* So this list is the
work, not a wish list. Nothing here is optional.

**MEASURED** means the running game or its log showed it. **READ** means the
code, the layouts or the game's image say so. **INFERRED** means reasoned and
not checked. **OPEN** means not known.

What is built is in `keyboard-play.md`; every key is in `keyboard-keys.md`.
None of the menu work has run in the game yet, so the second live test may add
to this list. Its log lines are listed in `keyboard-play.md`.

## Decisions that are the user's, asked 2026-09-27 and not yet answered

| question | why it is open |
|---|---|
| Which key swaps weapons? | the game's key is W, and W is the mod's |
| Which key says it again in a menu? | Home was its key and is a tab key in a menu |
| Does choosing the left skill get a key, and which? And the right skill? | see item 2 |
| Are Ctrl+R (repair all), Shift+R (repair), Alt+T (transmute) and Alt+S (sort) right? | Claude chose them |
| Which keys for the belt's fill and for a slider's change, once built? | items 4 and 5 |

Keys no one uses: J, K, L, U, Y, Insert, Delete, and most keys with Alt.
Taken with Ctrl: B, D, F, G, H, I, J, K, L, M, N, O, P, R, S, T, V, Y and the
numpad's 2, 4, 6, 8. **Check `keyboard-keys.md` before choosing a key**: Ctrl+T
was chosen for transmute on 2026-09-27 and was the radar's already.

## 1. Swap weapons

**The state:** no key at all. The game's key is W (READ, `Gordan.key`, action
44). The mod takes W in play (the right skill) and in a menu (right click),
and the first live test MEASURED that the game's W does not fire then.

**What is known:**

- READ, `docs/weapon-swap.md`: **`0x1E33A0` is the swap's entry, the one "the
  key, controller and UI handlers call".** It reads body locations 4, 5, 11
  and 12; in a local game it calls the local swap `0x15B920`, which takes the
  items off, puts them back swapped and flips the active-set flag; otherwise
  it sends the four items to the server.
- READ, `re/actions/send/c2s_msgs.txt`: the server's side is message `0x50`
  (`sWeaponSwap`, handler `0x4BEA50`). It refuses a dead character, a
  character in the middle of a skill, and a non-expansion character.
- READ, `PlayerInventoryExpansionLayoutHD.json:158, 164`: the inventory has
  two focusable areas, `weaponswap_right_arm` and `weaponswap_left_arm`. The
  layout gives them no message. INFERRED: a click on one swaps. If so the
  cursor reaches them already, in the inventory only.
- The swap is announced already (`src/weapon_swap_service.cpp`).

**OPEN:** what `0x1E33A0` takes as arguments. `src/native_weapon_set.cpp`
names `0x15B920` but calls neither; whether `0x1E33A0` has a fingerprint
anywhere was not found. Read its callers: the one the W key runs is the call
to copy, as the grid's handler was for the sort.

**To build:** a key, in play and in a menu, that calls `0x1E33A0` as the W
key does. `src/native_sort.cpp` is the model: check the function byte for
byte, call under a guard, log one line.

## 2. Choosing the skill for each hand

**The state:** no key. A and W fight with whatever skill is on each hand, so
a player who cannot change it has one attack for the whole game.

**What is known:**

- READ, `HUDPanelHD.json`: the HUD has `LeftSkillButton` and
  `RightSkillButton`, both `SkillSelectButtonWidget`. A click on one opens the
  list of skills for that hand.
- MEASURED, first live test: a panel named `SkillSelect` opened twice, with no
  focused widget. `SkillSelect` is in the gate's list of menus
  (`src/radar_ui.cpp`), so once it is open the cursor moves in it.
- READ, `Gordan.key`: the game binds S (action 13) and F1 to F8 (actions 14 to
  21). INFERRED from the order of the game's key binding screen: S opens the
  skill list and F1 to F8 choose the skills bound to them. Which hand S
  serves is OPEN.
- INFERRED: a skill is bound to F1 to F8 by pressing that key with the
  pointer on the skill in the list. The mod's pointer is on the skill the
  cursor is on, so this may work as it stands.

**To build:** two keys that click the HUD's two buttons. The HUD is not part
of the cursor's menu, so `KeyboardMenu::Press` does not find them:
`FindNamed(hud, "LeftSkillButton", ...)` and `ClickWidget` in
`src/keyboard_menu.cpp` are the pieces. The keys must work in play, where the
menu keys are off today (`g_active` is false in play).

**To measure:** what `SkillSelect` holds (`[cursor] thing` lines), whether a
skill in it reads its name, and whether a click chooses it.

## 3. Say it again, in a menu

**The state:** no key in a menu. In play Home still does it.

**To build:** one key calling `GameTextReader::RequestRepeat()` in a menu, as
Home did (`Read` in `src/keyboard_service.cpp`). Minutes of work once the user
names the key.

## 4. The belt

**The state:** the bottom row of the belt is in the cursor's menu while an
inventory is open. The rows above it are not. Filling the belt is not built.
The game's own keys 1 to 4 drink from the belt and reach the game.

**What is known:**

- READ, `src/native_ui.cpp`: a belt slot is the selection's x alone, 0 to 15.
  The bottom row is 0 to 3.
- READ, `Gordan.key`: the grave accent key is the game's "show belt".
  INFERRED: the upper rows are drawn only while the belt is shown, so their
  rectangles may not exist otherwise.
- READ, `re/actions/sort/notes.txt`: the controller's "hold to fill"
  (`ControllerPromptHoldToPopulateBelt`) is the belt's handler `0x2283E0`
  (vtable `0x1CE2FA0` +0x20). It checks controller mode on entry, then
  `CanPopulateBelt` (`0x226060`), builds its plan with `0x386FF0`, and sends
  message `0x5D` through `0x0ECA70`. **The plan is built inside the handler;
  there is no function of its own to call.** The server's side is `0x4AC160`.
- INFERRED: with the mouse, a potion goes to the belt by Shift + left click
  or Ctrl + left click on it in the inventory. The item's own tooltip says
  which, and the mod reads that line.

**To build:**

- The upper rows: how many the belt has now (4 to 16 slots, by the belt
  worn), and their rectangles. `AddCells` in `src/keyboard_menu.cpp` takes the
  size; the belt's is fixed at 4 by 1 today (`BeltColumns`).
- The fill: either the handler's plan written again in the mod and sent with
  `0x0ECA70`, or the handler called with a message made to look like the
  completed hold. The first is more work and safer. Decide after reading
  `0x2283E0` in full.

## 5. Sliders and dropdowns, in the options

**The state:** the cursor lands on them. A slider has no key to change it. A
dropdown opens on a click, but its rows are not read.

**What is known:**

- READ, `src/native_ui.cpp`: sliders, dropdowns and carousels are
  `Kind::ValueControl`. The mod reads a value when the game says it changed
  (`src/focus_tracker.cpp`) and takes the direction from the key pressed.
- READ, `src/focus_tracker.cpp:886, 977, 984-996`: a dropdown's rows are read
  only through the controller's focus indicator, which keyboard mode does not
  have. So an open dropdown is silent.
- INFERRED: a click on a slider's track sets it to that place.

**To build:**

- A slider: left and right arrows with a modifier, or two keys, that click a
  step to the left or right of where the value is. The slider's rectangle is
  read already; where its value sits in it is OPEN.
- A dropdown: its rows are `DropdownListContents`, a panel of its own. Add it
  to the cursor's menu when open and read the row the cursor is on.
- A carousel has two arrows; whether they are controls of their own is OPEN.

## 6. Rows in a list: characters, waypoints, the NPC's menu

**The state:** the cursor lands on a row and A clicks it. Landing does not
click. Not measured at all.

**What is OPEN:**

- On character select and in the NPC's menu the game keeps a focus of its own
  in keyboard mode (MEASURED, first test) and may move it with the arrows
  itself. Then two cursors move at once, the game's and the mod's.
- A click on a character chooses it and a second click on the same one soon
  after may start the game with it. INFERRED from how a double click works.
- Whether a waypoint travels on one click.

**To build, after the log shows what happens:** one rule for rows. Either the
mod follows the game's focus on these screens and keeps its cursor out, or it
clicks on landing. Do not choose before the second test.

## 7. Ctrl + Shift + left click

**The state:** the loader takes one modifier, so this cannot be a key of the
loader's. It is the game's gesture for "move", where an item has two places to
go.

INFERRED: it may work already. The mod watches A directly beside the loader
(`Watch` in `src/keyboard_menu.cpp`) and clicks if the loader delivers
nothing, and the game sees the Ctrl and Shift that are held. Then the game's
own A is not held back, though with two modifiers held it may not fire.

**To measure:** `[menu-keys] left click came by watching the key` after
Ctrl+Shift+A, and what the readers say next.

## 8. What the controls are, in mouse mode

**The state:** "the controls" of what the cursor is on (Ctrl+Down on a plain
control) is empty. READ: the mouse layouts have button legends in four
panels only, none of them the inventory, the skill tree, the character panel,
the quest log or the merchant.

In mouse mode the game writes what a click does into the item's tooltip
("Ctrl + Left Click to Sell"). Those lines are in the item's lines already,
read with Ctrl+Down. **To build:** the same words with the mod's keys in
them ("Ctrl + A to sell"), so the player is told the key to press and not
the mouse button. The strings are `InventoryItemTooltipAppender*` and
`InventoryItemTooltipKeyCombo*` in `ui.json`.

## 9. The readers that were built on the controller

Each of these is silent or wrong in keyboard mode. READ in the code, not yet
heard:

| reader | what is wrong | where |
|---|---|---|
| the skill tree | reads `TabN > SkillButtonContainer > SkillButton`, a shape measured in controller mode only | `src/native_progression.cpp:56-61` |
| the quest log | says "Empty" unless the shared pane holds the quest the cursor is on; the mod clicks a quest on landing to fill it (INFERRED) | `src/ui_text.cpp:130-147` |
| the second skill bar | reads controller-mode state | `src/skill_bar_service.cpp` |
| the belt's automatic potion | returns nothing out of controller mode | `src/native_belt.cpp:25` |
| the tab highlight | has never run: it builds an empty focus | `src/tab_highlight.cpp:20` |
| the character row's "A: Play, X: Delete" | written for the pad's buttons | `src/focus_tracker.cpp:140-143` |

The cursor names a place itself when no reader does (`[cursor] no reader
named it; said "..."`). **Every such line in the log is a reader to fix.**

## 10. The pointer, when play goes on

**The state:** the pointer stays where the last menu left it. INFERRED: in
play the game takes what is under the pointer as its target and the mod reads
it as a world label, so a pointer left over the middle of the screen names
things the player did not ask about.

**To build:** put the pointer somewhere harmless when the menu closes. Where
is OPEN; a corner of the HUD is the first thing to try.

## 11. Home and End

**The state:** built on the mini menu's buttons in the HUD, not measured.
Character, inventory, skill tree, quest log, game menu, in that order. The
mercenary's panel and the party's are not in it.

OPEN: whether the mini menu is shown (it can be folded away), and whether its
buttons give a rectangle. The log line is `[cursor] menus: ... button found /
not shown / has no rectangle`. If they do not, the game's own keys still open
each menu: C, I, T, Q, O, Esc.

## 12. Sort, in a trade

`NativeSort::Sort` refuses while `TradeLayout` is open: `SortPage` takes
another path then, which was not read. Low priority; single player has no
trade.

## The order to build in

1. **Run the second live test and read its log** before building anything
   here. It decides items 6, 7, 9 and 11 and may add items.
2. Items 1, 2 and 3: the keys without which the game cannot be played.
3. Item 9: whatever the log shows the cursor naming for itself.
4. Items 4, 5 and 8.
5. Items 6, 10, 11 and 12.
