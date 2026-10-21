# Tutorial for a new character (built 2026-10-21; unverified live)

The user's idea: when a new character enters the game, a hands-on tutorial teaches the mod's
commands, above all how to find your way with the radar and the scanner, through the Den of Evil,
then falls silent. Every step asks for one real action in the place the player stands and moves on
when the mod sees it done.

## Decided by the user (2026-10-21)

- Works with the **controller** as well as the keyboard: every step has both texts.
- The tutorial teaches **walking yourself**: scanner, route, beacon, radar. The user finds keyboard
  play too automatic (D walks up to things, H, Shift+Home and Ctrl+R walk for you), so the steps
  never rely on those (PRINCIPLES §9, player agency); the keyboard's farewell names H and Shift+Home
  once, as shortcuts.
- **Two sounds**, made for it: `sounds/one shot sounds/tutorial/message.ogg` (two soft notes: a
  message begins) and `step done.ogg` (a rising arpeggio). Sine partials with a soft attack, peak
  -9 dBFS, 48 kHz.
- **Message history** with BACK + right stick and BACK + D-pad up (below); keyboard K, L, Shift+K.

## Messages and their controls

The tutorial's messages are a list. A new message plays its sound, is spoken and moves the cursor
to it, so a step that changes always leaves the player on the newest message.

| | Pad | Keyboard |
|---|---|---|
| previous message | BACK + right stick left | K |
| next message | BACK + right stick right | L |
| first message | BACK + D-pad up | Shift+K |

- `InputDirection::TakeBackWithRightStickSide` / `TakeBackWithDpadUp`; BACK + right stick up and
  down stay the beacon service's (area words, route).
- **The game never gets BACK + D-pad up during play** (no potion drunk by accident): `PadFilter`
  keeps it like BACK's face buttons, decided on the press's first frame (`pad_filter_tests`), and
  the press counts as no ordinary D-pad edge for the mod's other readers.
- Browsing works in play only, interrupts, and says "First message" / "Last message" before the
  text when it cannot move further. New messages queue (speech kind `Tutorial`, "waits its turn").
- The history is rebuilt from the step reached when a game resumes the tutorial, so only the step
  is saved.

## When it starts and stops

- Decided once per game, after `LocalPlayerReady`, while the switch `tutorial` (Alerts group of the
  Ctrl+S list, default on) is on: the character's name and the roster (`ReadRosterCorpses`); with
  other players in the game it is not offered. A character with a record resumes at its step; one
  without a record begins only at character level 1 (`PlayerExperience`).
- Records: `<pluginDirectory>/d2r-access/tutorial.txt`, `name@created=step`, 16 meaning finished;
  `created` is when the character's save (`<Saved Games>/Diablo II Resurrected/<name>.d2s`) was
  made, in Unix seconds, so a character deleted and made again under the same name starts afresh
  (the user, 2026-10-21: the new one resumed at the old one's Akara step). The game keeps a save's
  creation time as it writes it (MEASURED: yarkey.d2s created 2026-09-14, written 2026-10-20). A
  record of the same name under another time, or a bare `name=step` from before, is dropped when
  that name plays. Without a local save the key is the name alone.
- **Deleted characters:** made again within 15 s, a save keeps the deleted one's creation time
  (Windows's file name tunnelling; MEASURED 2026-10-21, the new arwen.d2s carried 10:04:23, the old
  one's), and the new character resumed the old one's step. So outside a game the service looks for
  each recorded character's save every second and drops the record of one gone on two polls in a
  row (`DropDeleted`, log `[tutorial] NAME@TIME was deleted`): character select leaves the name
  without a save for longer than that.
- **News waits for the tutorial** (`Tutorial::Busy`): while a message is under way (its length at
  60 ms a character) or still queued, the radar's new places (`TerrainService`, held as on arrival)
  and the NPC announcer wait, then speak (the user, 2026-10-21: the new exits broke in).
- Turning the switch off stops it; on again, the next poll resumes it.
- New messages wait outside play (menus, NPC dialogue, pause) and while the game's own text is
  read (`uiOwnsSpeech`). Not built: waiting while enemies are in sight; a "skip this step" entry.
- **Arrival:** the first message waits `ArrivalMs` (3 s), and every message (its chime) waits until
  no speech has begun for `QuietMs` (1.5 s, `ScreenReader::LastSpoken`). The first live test
  (2026-10-21): the NPC announcer named Warriv at 0.5 s, the welcome was queued behind it, and the
  area's name ("Entrando en el Campamento de las Arpías", which interrupts) cut the welcome at 2.3 s;
  the radar's new places followed at 4.5 s. Queued behind such news, the message is no longer cut.
- **The radar alone:** until the world's sounds have had their own lesson (steps 0 to 2) they keep
  quiet: NPC loops and the "!" (`EntityCueService`), the stash, exits and other ground-item loops
  (`GroundItemService`), unexplored rooms (`ExplorationCueService`), all through
  `Tutorial::QuietWorld`. The user, 2026-10-21: Warriv's and the stash's sounds played over the radar's
  lesson, and "people have to hear well what each sound means". Speech (the NPC announcer) is not
  silenced; the tutorial's own one-shot sounds are a separate channel. Their lesson (step 2) begins
  `SettleMs` (4 s) after the wall that ends the radar's, so the two do not mix; it plays each sound
  after its name, as the radar's step does (`ItemAudio::Preview`, `EntityCueAudio::Play`, the sound
  glossary's players), then lets the world in.
- **The radar's words** (the user, 2026-10-21): its message says the tutorial plays the sounds and
  nothing need be pressed; each sound comes after its name and meaning ("Floor: you can walk that
  way; further on there may be anything, a passage too"), and the task after them is only to walk
  until a wall. A sound waits for its words at 45 ms a character, at least 1.2 s (`ReadMs`); the
  first, and the world's sounds let in, wait so for the message before (it was 60 ms: "a little
  shorter", the user).
- **Place names** are the game's own (`NativeEntities::LevelName`, tokens `{den}` and `{moor}`): Spain's
  Spanish says Cubil del Mal and Landa de Sangre, Mexico's Guarida del Mal and Páramo Sangriento
  (`game-data/local/lng/strings/levels.json`). The texts avoid an article before them.
- Logs: `[tutorial] begins|resumes at step N for NAME`, `step N reached`, `finished for NAME`.

## Steps

Pad buttons are said by the game's glyph names (`ControllerText::Button`, tokens such as `{view}`
in `tutorial_text.cpp`), so they follow the language. The game's own actions are written as their
default buttons (A talks and picks up, B closes).

| # | Where | Teaches | Done when |
|---|---|---|---|
| 0 | Camp | welcome, the message controls; life and mana (BACK+X / Z, X) | the vitals readout was asked |
| 1 | Camp | the radar: its five sounds, each after its name, then walking until a wall | the radar plays a wall cue |
| 2 | Camp | the world's sounds, each after its name (NPC, NPC with "!", stash, stairs or portal, area border, unexplored room), then let in around the player | 8 s after they are let in (`ListenMs`) |
| 3 | Camp | the scanner's categories and objects | the scanner names Warriv, the NPC nearest the start |
| 4 | Camp | talking (A / D beside him), walking to him yourself | his menu is open with him the nearest town NPC, or the Den's quest runs |
| 5 | Camp | the scanner again, for someone farther | the scanner names Akara |
| 6 | Camp | the route (BACK + right stick down, or BACK+A with `pad-route-on-a` / Ctrl+E) | a route to Akara is asked |
| 7 | Camp | the beacon (BACK+LB / Ctrl+B), walking it yourself | the beacon arrives at Akara |
| 8 | Camp | the first quest | her menu is open with her the nearest town NPC, or the Den's quest runs |
| 9 | Camp | the same tools for a way out | the player is in the Blood Moor |
| 10 | Blood Moor | ways out of this area (BACK+Y / Ctrl+Y), terrain overview (BACK+B / Ctrl+O); the pad's also names the area's words | both asked |
| 11 | Blood Moor | enemy sounds and attacking; potions; picking up | gold taken outside the camp |
| 12 | Blood Moor | finding a place not yet seen | the player is in the Den of Evil |
| 13 | Den of Evil | room sounds, the monster counter | the Den's quest: reward pending, goal done or completed |
| 14 | Den of Evil | the town portal scroll, or walking back | the player is in the camp |
| 15 | Camp | the reward and the skill tree | the Den's quest completed |
| 16 | Camp | farewell: the settings list | — |

Step 1's sounds (2026-10-21): passage, unexplored passage, floor, unexplored floor, wall, door, small
obstacle, each after its name and meaning; the unexplored ones are the radar's double pip
(`Radar::Marks`), as the sound glossary plays them.

**The level-up lesson** (the user, 2026-10-21: menus, attributes, tooltips and the skill tree once
the character levels up, but not in a fight, and without having to shut one menu to reach the
next) runs beside the steps, once per character. From level 2, once no enemy has been on screen
(`CombatTarget::OnScreen`) for 3 s (`CalmMs`) and the tutorial is not talking, it says in play to
open the character sheet (pad: Menu, the button the game's own "Press Menu to assign Stat Points"
names; keyboard: C), then follows the player through the menus (`WatchLesson`, every poll):

| Where | Said (pad; keyboard) |
|---|---|
| `CharacterStatsPanel` opens | on Strength's "+": D-pad up/down the attributes, hold A spends (`controllerRequireHold`), right stick up/down the lines and right the points left; arrows, A, J and Shift with an arrow |
| its points line (`CharacterStats::Points`, `HasPoints`) had points and has none | RB until the skill tree, each menu says its name (INFERRED: the top tabs `UISwitcherTabs` cycle on LB/RB, the glyph table's "cycle left/right"; not pressed yet); T opens the tree beside the sheet |
| `SkillsTreePanel` opens | LT/RT the branch, the D-pad the skills, right stick up the description, A spends (no hold in `Controller/SkillsTreePanelHD.json`), right stick right the points left, B closes; Page Up/Down, arrows, J, A, Ctrl+Page Down, Escape |
| in the tree, its choices line (`CharacterStats::SkillChoices`) had a point and has none | how to put a skill on a button: X on it, then the button (the footer's "X: Bind Skill" and the game's "Press button to rebind skill", docs/skill-actions.md); keyboard S / Shift+S, arrows, A, F1-F8. Said in play before "Done" if never said in the tree |
| back in play from the sheet, the tree not seen | Menu again opens the tree once the attributes are spent (the game's "Press Menu to assign Skill Points"); T |
| back in play from the tree | given (`<key>#levelup=1`): "Done", and the current step's message again |

Words for a menu are said there (`Action::menu`, put at the front of the queue, still behind a
pause in speech); they are dropped if the menu shut before they were said. Only play counts as
shut, since switching menus can show neither panel for a moment. Log: `[tutorial] level-up lesson
begins|given`. Not checked live.

**The inventory lesson** (the user, 2026-10-21: walking the inventory and equipping once something
is picked up, and that an unidentified item cannot be equipped) runs beside the steps the same way,
once per character (`<key>#inventory=1`). In play, the player's items are read every poll through the
loader's inventory service (backpack and worn); an item in the backpack never seen before whose class
is a weapon, armour, ring or amulet (`NativeGroundItems::ClassIsType`: weap, armo, ring, amul) arms
it, and at the next calm moment it says to open the menus (pad: Menu, which opens them on the
inventory, INFERRED from the level-up log, where Menu opened `UISwitcher` and the inventory before
the sheet; keyboard: I). As the inventory opens (`PlayerInventoryExpansionLayout` or
`PlayerInventoryOriginalLayout`): the D-pad (arrows) cell to cell through the backpack and what is
worn, right stick up (J) the description, right stick down the item's buttons, equip among them
(`ControlHints`; the game's own prompts, `ControllerPromptItemEquipOnSelf`), Shift+A on the keyboard,
and that an unidentified item cannot be worn until identified with a scroll of identify from Akara
(the user's words; no string of the game's says so). When that item is worn (container Equipment):
"You have it on". Back in play: "Done", and the current step's message again. A lesson followed
through its menus holds the other back; one only said in play does not. Log: `[tutorial] inventory
lesson begins|given`.

**Words in a menu** (fixed 2026-10-21): the first level-up test heard nothing in the sheet. In a
menu the reader of what has focus always owns speech (`GameTextReader::Tick` returns true while the
interaction reader has focus), so a menu message waited for `uiOwnsSpeech` to clear, which it never
did. Menu messages now wait only for a pause in speech.

**No spoilers** (the user, 2026-10-21): the welcome says "until the first quest is done", Warriv's
step only to walk up to him and listen to what he has to say, and Akara "has a quest for you"; the Den is named only once she has given it. **Finding the Den** says the
map is made anew each game, so the entrance may be near or far: follow the radar (or the wall tones
if on), its new-exit news and then the scanner's exits, and the unexplored room category if lost.

Warriv comes first because he stands a few steps from where a new character begins and greets
them (the user, 2026-10-21); the route and the beacon are taught on Akara, who is far enough for
them to matter.

An **event** (a readout asked, a scan, a wall heard, gold taken, Akara's menu) counts only once its
step is current, so each step asks for its own action. A **state** (where the player is, the quest's
progress) counts while it holds, so a quest already further on passes its steps at once.

The radar's sounds are audition cues (`CueSource::audition`), which no radar reading drops.

## Signals and their sources

| Signal | Source |
|---|---|
| Vitals, RoomWays, Terrain | `Tutorial::Post` at the top of `VitalsService` `Report`, `RoomService` `Layout`, `TerrainService` `Overview` |
| Wall | `RadarService` after a generated cue set with `State::Blocked` |
| ScannedWarriv, ScannedAkara, RouteToAkara, AtAkara | `TutorialService::Scanned` (after `BeaconService::Offer` in the scanner), `Routed` (`DescribeRoute`), `Arrived` (the beacon's arrival when reached); an NPC is known by its `placeId`, `TownNpcs::Identity(row)`: Akara's row is 148 and Act I's Warriv's 155 in `game-data/global/excel/monstats.txt` |
| TalkedToWarriv, TalkedToAkara | `NpcDialogPanel` open with that NPC the nearest town NPC (`NativeEntities::Read` with town NPCs, which gives their identity), or the Den's quest running or done |
| InTown, InBloodMoor, InDen | `LevelChanged`: Levels.txt rows 1, 2 and 8 (`src/generated/level_flags.h`) |
| DenCleared, DenDone | `NativeQuestFlags::Read(1)`: row 1 inferred from the save's order (the Tower is 5, Act II begins at 8); bits 1 and 13, and `QuestDoneBits` |
| Gold | `NativeGroundItems::PlayerGold()` rising outside the camp |

## To check live

- The welcome and step 0 on a new level 1 character, with the pad and with the keyboard; both
  sounds' loudness against the game.
- BACK + D-pad up reads the first message and drinks nothing; BACK + right stick left/right browse.
- The radar's five sounds land after their names, not on the message.
- ~~The Den's quest row and bits~~ MEASURED 2026-10-21 (Morga, the whole tutorial played through):
  row 1 is the Den. It stays 0000 after Akara gives the quest, turns 201e as the cave is cleared
  (step 14 reached 13:27:24) and 2001 with the reward (finished 13:28:20). Row 0 turns 0001 the moment
  Akara's dialogue opens (twice: 12:31:47 and 13:00:21), not at Warriv's, so it is now also a state
  for both talks (`IntroDoneBit`). The Den's row is never "running" before it is cleared, so the
  quest-running fallback for the talk does not fire in a fresh game.
- Warriv's and Akara's menus are `NpcDialogPanel` with them as the nearest town NPC.
- The welcome is heard whole on arrival, after the area's name and the NPC announcer.
- Whether the scanner lists the Den's entrance before it is seen (step 9's words cover both).

## Open

- Whether it ends at the Den or also teaches the Cold Plains waypoint.
- Translations beyond Spanish and English.
