# Every key, in one place

The keys of keyboard play on the `keyboard-local` branch, 2026-09-27. The
mod's keys are READ from what the code registers with the loader; the game's
own are READ from the character's `.key` file. What each of the game's
actions is called is from the order of the game's key binding screen and was
not checked against it. How the menus work is in `keyboard-play.md`.

## In play

| key | what |
|---|---|
| arrows, held | walk |
| A, held | fight with the left skill |
| W, held | fight with the right skill |
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
| Home | say the scanner's object again |
| Ctrl+Numpad 4, Ctrl+Numpad 6 | scanner: previous object, next object |
| Ctrl+Numpad 8, Ctrl+Numpad 2 | scanner: previous category, next category |
| backslash | say the scanner's object again |
| comma, full stop | scanner: previous category, next category |
| Ctrl+B | follow or release what the scanner named |
| Ctrl+Y | the ways out of this room |
| Ctrl+O | the terrain overview |
| Ctrl+T | the radar's state |

## In a menu

| key | what |
|---|---|
| arrows | move the cursor; a held arrow repeats |
| A | left click: choose, pick up, put down |
| W | right click: use, read, open, buy |
| Shift+A | left click with Shift: equip, unequip |
| Ctrl+A | left click with Ctrl: sell, drop, move to the stash or the cube |
| Shift+W, Ctrl+W | right click with the key held |
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
| S | the list of skills to choose from | yes |
| F1 to F8 | skills 1 to 8 | yes |
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
| **W** | **swap weapons** | **no: the mod's, in play and in a menu** |
| Tab | automap | no: next monster |
| H | help screen | no: explore |
| Z | portraits | no: life |
| F, G, E | three later actions, not identified | yes |

## What has no key

| what | why | what it would take |
|---|---|---|
| swap weapons | W is the mod's | a key of the user's choosing; in the inventory the two swap areas can be clicked |
| say it again, in a menu | Home was its key and is a tab key there | a key of the user's choosing |
| choose the left skill | the game's S key opens the right skill's list; the left one is a click on the HUD | a key that clicks the HUD's left skill button |
| move, where an item has two places to go | it is Ctrl+Shift+left click, and the loader takes one modifier | the mod watching the key itself |
| fill the belt | the controller's hold; the game has no function of its own for it | not built |
| change a slider | the mouse drags it | not built |
| the belt's upper rows | only the bottom row is in the cursor's menu | not built |

Keys no one uses: J, K, L, U, Y, Insert, Delete, and most keys with Alt.
