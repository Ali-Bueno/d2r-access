# Every key, in one place

The keys of keyboard play on the `keyboard-local` branch, 2026-09-27, after
the fourth session of that day. The mod's keys are READ from what the code
registers with the loader (MEASURED 12:18: `[keyboard] 28 of 28 keys
registered`; Shift+S was added after that, and J on 2026-09-28, so the next
log has to say 30 of 30; MEASURED 2026-09-28 03:35: 32 of 32, and with [ and ] the next says 34 of 34); the game's own are READ from the character's `.key` file. What
each of the game's actions is called is from the order of the game's key
binding screen and was not checked against it. How the menus work is in
`keyboard-play.md`.

**Every key below was chosen or confirmed by the user on 2026-09-27.** Ask
before adding one, and read this file first.

## In play

| key | what |
|---|---|
| arrows, held | walk |
| A, held | fight with the left skill: only a monster on screen, as a click would (`22e2739`) |
| W, held | fight with the right skill: only a monster on screen |
| Y | swap weapons |
| Shift+S | the list of skills for the left hand (the game's S opens the right hand's) |
| Tab, Shift+Tab | next monster, previous monster, on screen |
| full stop, comma | next item on the ground, previous one, on screen, nearest first; its name, how far and which way. **Every object counts as an item: chests, urns, barrels, dead bodies, shrines, levers, doors, the stash, and a player's corpse** (the user, 2026-09-28). D takes a corpse as it opens a chest: the game's own message for it, `0x13` (READ in the game's click code, not pressed yet). The next D picks up or opens that one and no other; if it has gone, D says so and takes nothing |
| right bracket, left bracket | next NPC, previous one, on screen, nearest first, as full stop and comma do; the next D talks to that one (the user, 2026-09-28: an NPC close to other things D could take) |
| D | walk up to the nearest usable thing on screen and use it |
| H | walk to the nearest unexplored room |
| Shift+Home | walk to what the scanner named. An exit is taken: stairs are used, an area border is walked on across (`9269536`) |
| Ctrl+R | retreat: walk back to the way you came into the level -- the stairs, waypoint, portal or area border you arrived by, or the place you first stood if none stands there -- and stop beside it (the user, 2026-10-02: "collapse retreat to control+r"). Shift+R and Alt+R retreat no more; in play they are the game's |
| End | stop |
| Z, X | life, mana |
| PageUp, PageDown | scanner: previous object, next object |
| Ctrl+PageUp, Ctrl+PageDown | scanner: previous category, next category |
| Home, backslash | say the scanner's object again |
| Ctrl+Numpad 4, Ctrl+Numpad 6 | scanner: previous object, next object |
| Ctrl+Numpad 8, Ctrl+Numpad 2 | scanner: previous category, next category |
| Ctrl+B | follow or release what the scanner named |
| Ctrl+E | the route to what the beacon follows or the scanner named (docs/route-description.md) |
| Ctrl+Y | the ways out of this room |
| Ctrl+O | the terrain overview |
| Ctrl+T | the radar's state |
| K, L | the tutorial: previous message, next message ([tutorial](tutorial.md); the user, 2026-10-21) |
| Shift+K | the tutorial: its first message |

"On screen" is a 16:9 view around the character, INFERRED from the user's
1920x1080 and not measured (`CombatTarget::OnScreen`). The scanner lists
items on the ground and every chest as well since 2026-09-28.

Comma and full stop were the scanner's category keys and were retired by the
user on 2026-09-27; on 2026-09-28 the user gave them to choosing an item on the ground.

## In a menu

| key | what |
|---|---|
| arrows | move the cursor to the nearest thing that way; a held arrow repeats |
| left, right, on a slider or an option that steps | change its value; the cursor stays |
| A | left click: choose, pick up, put down. On an option that steps: its next state. On a slider: its value is said. On a character of character select: two clicks, which begin the game |
| Enter | the game's own: on character select it begins the game with the character chosen, which is the one the cursor is on |
| D | right click: use, read, open, buy, drink |
| Shift+A | left click with Shift: equip, unequip, a potion to the belt |
| Ctrl+A | left click with Ctrl: sell, drop, move to the stash or the cube |
| Shift+D | right click with Shift |
| Ctrl+D | right click with Ctrl. Until 2026-09-28 it was autodetail's switch, which is now only in the Ctrl+S list (the user's choice) |
| W | nothing: it is kept from the game, whose W swaps weapons |
| Y | swap weapons |
| Shift+S | the list of skills for the left hand, or shut it |
| backslash | say it again |
| J | the description of what the cursor is on, as the pad's right stick up does: for play with autodetail off (in the Ctrl+S list). **On the character screen it reads the game's tooltip of the stat line the cursor is on, and never moves the lines** (the pad cannot reach that tooltip at all). Added 2026-09-28 at the user's asking, not pressed yet |
| Ctrl+Up, Ctrl+Down | the line before, the line after, with the game's Ctrl view on: an item's ranges, "+(1-3) to Sacrifice" |
| Ctrl+Left, Ctrl+Right | the first line, the last line, the same way |
| Shift+Up, Shift+Down | the line before, the line after, as the game draws the tooltip with Shift held, which is the regular one: "+2 to Sacrifice" (the user, 2026-09-29; the game shows ranges only with Ctrl, the user, 2026-09-30). After the compare key reads the equipped item they walk that item's lines instead, until the compare key reads the highlighted one again or the cursor moves |
| Shift+Left, Shift+Right | the first line, the last line, the same way |
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

### With the cursor on a box of text

A name, a number of gold, a game's name: while the cursor is on the box, the
keys that type are the box's, alone or with Shift. A, D, W, Y, H, Z, X,
backslash, Home and End do nothing of the mod's there, and the left and right
arrows move in what is typed. **Up and down leave the box.** Keys with Ctrl or
Alt stay the mod's.

On character creation the name box takes what is typed wherever the cursor
is (READ), so **letters reach it only while the cursor is on it**: elsewhere
on that screen they are kept from the game, and A clicks without typing an
"a" (`dcc8e0e`, the user's report 2026-09-28; not pressed yet). Enter, Esc
and Backspace always reach the game. The log says `[menu-keys] kept 'a' from
the name box` for each one kept.

On character creation the game's own keys are: **Enter creates the
character** once a class is chosen and the name is allowed, and Esc puts the
chosen class back, or leaves the screen when none is chosen (READ in the
game's code, not pressed in a test).

## Everywhere

| key | what |
|---|---|
| Ctrl+S | the list of the mod's switches, in groups, open and shut: Page Up/Down step, Space opens a group or changes, Shift+Space lowers, Backspace returns to the groups. The pad's is BACK+RB, then the right stick: up and down step, right opens or changes, left returns; LB and RB lower and raise, kept from the game while the list is open (2026-10-13) |
| PageUp, PageDown, Space | with that list open: previous, next, change. The arrows are not the list's: in play they still walk the character. The user, 2026-09-28, once told PageUp and PageDown step the list: no change wanted |
| Ctrl+H | the controls read with it, on and off |
| Ctrl+G | radar |
| Ctrl+F | footstep filter |
| Ctrl+U | the scanner's filter for what cannot be reached: on, it lists only what the player can get to on foot from where they stand (the user, 2026-10-02: Ctrl+F if free, else the first Ctrl key that makes sense; Ctrl+F is the footstep filter). Off until switched on; also in the Ctrl+S list |
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
| S | the list of skills for the RIGHT hand (READ in the image); the mod says the hand as the list opens | yes |
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
| move, where an item has two places to go | it is Ctrl+Shift+left click, and the loader takes one modifier | INFERRED to work already through the key watch: Ctrl+Shift+A. Not measured |

Keys no one uses: U, Insert, Delete, and most
keys with Alt.
