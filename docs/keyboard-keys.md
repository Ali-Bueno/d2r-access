# Every key, in one place

The keys of keyboard play on the `keyboard-local` branch, 2026-09-27, after
the third session of that day. The mod's keys are READ from what the code
registers with the loader (MEASURED 12:18: `[keyboard] 28 of 28 keys
registered`); the game's own are READ from the character's `.key` file. What
each of the game's actions is called is from the order of the game's key
binding screen and was not checked against it. How the menus work is in
`keyboard-play.md`.

**Every key below was chosen or confirmed by the user on 2026-09-27.** Ask
before adding one, and read this file first.

## In play

| key | what |
|---|---|
| arrows, held | walk |
| A, held | fight with the left skill |
| W, held | fight with the right skill |
| Y | swap weapons |
| Tab, Shift+Tab | next monster, previous monster |
| D | walk up to the nearest usable thing and use it |
| H | walk to the nearest unexplored room |
| Shift+Home | walk to what the scanner named |
| Ctrl+R | retreat to the nearest way out |
| Shift+R | retreat the way you came in |
| Alt+R | retreat to the nearest corridor or doorway |
| End | stop |
| Z, X | life, mana |
| PageUp, PageDown | scanner: previous object, next object |
| Ctrl+PageUp, Ctrl+PageDown | scanner: previous category, next category |
| Home, backslash | say the scanner's object again |
| Ctrl+Numpad 4, Ctrl+Numpad 6 | scanner: previous object, next object |
| Ctrl+Numpad 8, Ctrl+Numpad 2 | scanner: previous category, next category |
| Ctrl+B | follow or release what the scanner named |
| Ctrl+Y | the ways out of this room |
| Ctrl+O | the terrain overview |
| Ctrl+T | the radar's state |

Comma and full stop were the scanner's category keys and were retired by the
user on 2026-09-27.

## In a menu

| key | what |
|---|---|
| arrows | move the cursor; a held arrow repeats |
| A | left click: choose, pick up, put down |
| D | right click: use, read, open, buy, drink |
| Shift+A | left click with Shift: equip, unequip, a potion to the belt |
| Ctrl+A | left click with Ctrl: sell, drop, move to the stash or the cube |
| Shift+D | right click with Shift |
| W | nothing: it is kept from the game, whose W swaps weapons |
| Y | swap weapons |
| backslash | say it again |
| Ctrl+Up, Ctrl+Down | the line before, the line after |
| Ctrl+Left, Ctrl+Right | the first line, the last line |
| PageUp, PageDown | the tab before, the tab after |
| Home, End | the menu before, the menu after |
| Ctrl+PageUp | a skill's lines, opened and closed |
| Ctrl+PageDown | an item's size; a skill's points left |
| Shift+PageUp, Shift+PageDown | compare: the item in full, the one equipped |
| Ctrl+R | repair all |
| Shift+R | repair |
| Alt+T | transmute |
| Alt+S | sort |
| Alt+F | fill the belt from the inventory (with an inventory open) |

Ctrl+D is not a click: it is the whole description's switch, below.

**Decided by the user, not yet built:** with the cursor on a slider or on an
option that steps through values, the left and right arrows change the value,
"the same as the controller would". See `keyboard-unbuilt.md`.

## Everywhere

| key | what |
|---|---|
| Ctrl+S | the list of the mod's switches, open and shut |
| PageUp, PageDown, Space | with that list open: previous, next, change |
| Ctrl+D | the whole description on every move, on and off |
| Ctrl+H | the controls read with it, on and off |
| Ctrl+G | radar |
| Ctrl+F | footstep filter |
| Ctrl+N | NPC announcer |
| Ctrl+M | room announcements |
| Ctrl+V | exit finder |
| Ctrl+I | speech for items nearby |
| Ctrl+L | sounds for items |
| Ctrl+P | sounds for ways out |
| Ctrl+K | sounds for chests |
| Ctrl+J | the automap's diagnostic watch |

Every switch that has no key of its own is in the Ctrl+S list.

## The game's own keys, from `Gordan.key`

| key | the game's action | reaches the game? |
|---|---|---|
| A, C | character screen | C does. A is the mod's |
| I, B | inventory | yes |
| T | skill tree | yes |
| Q | quest log | yes |
| O | mercenary | yes |
| P | party | yes |
| M | message log | yes |
| Esc | game menu; shuts what is open | yes |
| Space | shuts every panel | yes, but not while the Ctrl+S list is open |
| S | the list of skills for the RIGHT hand (READ in the image) | yes |
| F1 to F8 | skills 1 to 8; with a list of skills open, binds the skill under the pointer | yes |
| 1 to 4 | the belt's potions | yes |
| grave accent | show the belt | yes |
| R | run or walk | yes |
| Ctrl, held | run | yes |
| Shift, held | stand still | yes |
| Alt, held | show items on the ground | yes |
| Enter | chat | yes |
| N | clear messages | yes |
| Numpad 0 to 7 | the spoken calls | yes |
| F9 to F12, V | automap options | yes |
| PrintScreen | screenshot | yes |
| **W** | **swap weapons** | **no: the mod's. Y swaps** |
| Tab | automap | no: next monster |
| H | help screen | no: explore |
| Z | portraits | no: life |
| F, G, E | three later actions, not identified | yes |

## What has no key

| what | why | what it would take |
|---|---|---|
| choose the left skill | the game has no key for the left hand's list; with the mouse it is a click on the HUD's left skill button | the user's decision: a key, or the button put in the cursor's menu. Asked 2026-09-27, answered with a question; see `keyboard-unbuilt.md` |
| move, where an item has two places to go | it is Ctrl+Shift+left click, and the loader takes one modifier | INFERRED to work already through the key watch: Ctrl+Shift+A. Not measured |
| change a slider, step an option | not built | the arrows, as decided |

Keys no one uses: J, K, L, U, Insert, Delete, comma, full stop, and most
keys with Alt.
