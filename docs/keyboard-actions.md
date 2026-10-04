# Keyboard actions: making the character act without a pad

How the keyboard walks, fights and interacts. No controller is imitated and no
input is sent to the game: the plugin calls the function the game itself calls
once it has decided what a click or a button means.

Everything here is for D2R **3.3.93787**, image base `0x140000000`. **READ**
means the disassembly shows it; **MEASURED** means the running game showed it.
Nothing in this file is MEASURED yet. The first live session decides the open
questions at the end.

## The command

`PlayerCommand(int cmd, Unit* player, int a, int b)` at RVA `0xFABE0`
(`D2Client\src\Unit\UnitMode.cpp`). READ: `mov r13d,ecx; movsxd r14,r9d;
movsxd r15,r8d; mov rbx,rdx`.

`cmd` is the client-to-server message number. `a`, `b` are a cell's x and y for
a command aimed at a place, a unit's type and id for one aimed at a unit.

| cmd | meaning | a, b |
|---|---|---|
| 0x01 / 0x03 | walk / run to a cell | x, y |
| 0x02 / 0x04 | walk / run to a unit | type, id |
| 0x05 / 0x0C | left / right skill at a cell | x, y |
| 0x06 / 0x0D | left / right skill on a unit | type, id |
| 0x16 | pick up an item | 4, id |
| 0x2F | talk to an NPC | 1, id |
| 0x40 | use a warp tile (stairs, entrance) | 5, id |
| 0x41 | use an object | 2, id |
| 0x42 | the controller's directional move | packed direction, 0 |

Unit types: 0 player, 1 monster, 2 object, 3 missile, 4 item, 5 tile.

READ, what it does: returns at once when `0xFFAC0(player)` says the character
cannot act; maps `cmd` to a local event; calls `0xFB1D0(event, player, ...)`;
and only if that accepts, calls `0xFC000(cmd, player, a, b)`, which builds and
sends the message. For the interaction commands `0xFC000` sends nothing: event
2 reaches `0xFA180(type, id)`, which sends the message for that unit type.

**The skill commands carry no skill.** READ: 5 and 6 use the player's left
skill (`0x34A540`), 0xC and 0xD the right (`0x34B400`).

### Why this function

Two independent static passes, one from the network side and one from the
input side, arrived at it separately. The game's own built-in test code calls
it directly (READ, from `names.txt` source attributions):

- `BVT\PlayerSkillsUtility.cpp` `0x140248DF0`: `ecx = right ? 0xD : 6; rdx =
  player; r8d = unit type; r9d = unit id`.
- `BVT\ProcessCombatBVT.cpp` `0x1402661F0`: `r8d = 4; r9d = item id; ecx = 0x16`.
- `BVT\ProcessGameSystemsBVT.cpp`: `ecx = 1, r8d = x, r9d = y`, seven sites.
- `BVT\ProcessAbilitiesBVT.cpp`: `r8d = 2; ecx = 0x41; r9d = object id`.

In **controller mode** that same test code presses pad buttons instead
(`0x1401452E0`, after `0x77D80` returns true). Whether the command works while
the game is in controller mode is therefore the first open question.

## A click's walk

`MoveToPoint(cmd, player, x, y)` at RVA `0x101EF0`, what a click on the ground
calls. READ: takes the player's path; unless the path is the controller's
directional kind (type `0x11`), sets the target (`0x342A50`) and has the game
find its own path (`0x340E70`); replaces x and y with where that path ends;
calls `PlayerCommand`. The plugin's moves go through it, so a keyboard walk is
the walk a click makes.

The click code refuses a cell 256 subtiles or more away on either axis
(`0x140100BB0`). The plugin's orders are at most `AutoWalk::Settings::reach`.

## The selected skills

READ: `[unit+0x100]` is the player's skill list; `[list+0x08]` the left hand's
skill, `[list+0x10]` the right's (`0x33D1A0`, `0x33DB60`); a skill's first
field points at its `skills.txt` record, which begins with its 16-bit id
(`0x33E080`). `src/generated/skill_attacks.h` says what kind of skill an id is.

## Interaction reach

READ in `0x140101910`: the game walks up first and only then sends the
interaction. An item is in reach at distance 4 or less (`0x140325140`); an NPC
at 2 or 5 by a MonStats flag. The server refuses one that is too far ("Item is
too far", "Player is too far"). The plugin walks its own route up to the unit
and sends the command once it is near.

## The message path, for reference

READ: all 32 message builders (`0x1400EC490`..`0x1400ED9FC`) end in a jump
through `0x143E2AD90`, a slot D2RLoader fills; in the dump it points into
`D2RCore.dll` (`+0x79BD10`), which drops a message identical to the previous
one in the same tick. In a local game the message is appended to an in-process
queue (`0x140640360`, mode 1). Walk and run messages are `0xD2` bytes and carry
the client's path; message `0x13` is corpse interaction here, not the generic
interaction it was in 1.13.

The server's table of handlers is at `0x141D2A790`, indexed by message number,
`0x67` entries (`PlrMsg.cpp:8501`, `bCmd < NUM_CCMDS`).

## Fingerprints

`src/native_actions.cpp` hashes each whole function before anything is called
and refuses the lot when one differs. The hash ignores the operand of relative
calls and jumps (`layout_check.h`), so a loader that redirects a call does not
break it.

## Open, and what settles each

| question | measurement |
|---|---|
| Does the command move the character while the game is in controller mode? | `access-actions run`, then the `[actions]` lines and whether the character moved |
| Which thread issues commands? | the hook logs `thread=` for the game's own and `ui_thread=` for ours |
| What does controller play send? | play with the pad; the hook logs each command the game issues |
| Does a walk work with the path in the controller's kind (`0x11`)? | `access-actions run` right after moving with the stick |
| How far does a ranged skill carry? | not in `skills.txt`; the reach is one policy distance until measured |

The working files of the two static passes are kept in the workspace, outside
this repository, under `re/actions/`.

## Skill-hotkey announcements

2026-10-04: a successful native keyboard skill hotkey announces the game's
localized skill name alone. A fresh press of the same skill speaks again;
leaving it selected, releasing quick cast, binding a skill in its menu, and
physical controller actions stay silent. The native selection/cast proceeds
with its original arguments and result. Remapped controls and all 16 binding
slots follow the game's dispatcher, without registering competing mod keys.

`NativeSkillHotkeys` observes `0x118D50` (bool selection, slot and default-attack
flag) only when called from the keyboard dispatcher return at `0x120373`.
For quick cast it observes `0x149550` (bool result, manager, player index, slot,
pressed flag) only on key down from `0x122F6A`. These return sites are derived
from complete `0x11FF30` / `0x122F30` functions. The name/hand are native binding
getters `0x117D20` / `0x117DE0`; controller mode `0x77E10` excludes physical pad
calls. All seven function fingerprints matched the user's running 1.3.1-beta
session in a read-only inspection, before installing these observers.

Hooks copy a bounded queue of ids. `SkillHotkeyService` resolves names on the UI
thread, drops events during menus, text entry or the mod settings list, then
queues PRISM and logging to the existing workers. It runs before vital alerts
so urgent life/mana speech keeps priority. `[native] enabled = false` disables
these observers along with the other hooks. Diagnostics: `[skill-hotkey]`.

`skill-hotkey-service` tests cover selected names, repeated distinct presses,
quick-cast binding identity, missing names, and menu/chat/settings suppression.
Native instruction/signature evidence is in workspace `re/skill-hotkeys/`;
live audible selection/cast remains a user-led check after deployment.
