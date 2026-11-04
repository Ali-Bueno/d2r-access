# Decisions log

Dated log of design decisions and explicitly rejected alternatives, newest first. Short entries —
the *how* lives in the linked docs, this page only records the *why* and what was turned down.

## 2026-11-04 — Guidance can follow the scanner

The user: a shortcut or a setting so that what is scanned becomes the beacon's target, as BACK+LB does,
and scanning something else moves it. Both are built: `beacon-follow-scan`, off, and BACK+A or Alt+B
to flip it, the combination the passage finder left free. Said as the cue alone: words would cut off
the scanner naming the thing. Turned down: a pause before the lock, as a beacon only pings and a
walk was not what was asked; keeping the old lock when the new target is refused, as it would lead
to what the player has just moved on from.

## 2026-11-04 — The pad can walk to its target as the mouse does

The user wanted an attack to walk to the enemy, as in a modern ARPG, and asked whether Nexus mod
1345 could be adapted. It could not: its 280 ms gate is not on the stick's path. The cause was the
game's own rule, a stand-still attack unless the target is within reach plus the docking distance
([input commands](input-commands.md#why-a-pad-attack-stands-still)). The user then asked for the
same with NPCs, "more or less according to the mouse", as something a player turns on.

Chosen: `pad-approach`, **off**. Hooks on the docking test and on Interaction's reach test, and an
NPC reach raised only while A has nothing else to take. Turned down: raising the docking distance to
48, the first version, as a click has no line test and it reached every category's Interaction
target; editing `controllersettings.json`, which could not be switched off in play; raising the NPC
reach for good, as A would then leave a stash at hand or a fight for a far NPC; limiting it to
town, as quest NPCs stand outside it and the empty slots already say what town would. While raised,
aim counts as for monsters: the game's NPC score is distance alone, and the user wants "the nearest
one I am looking at".

## 2026-11-03 — Arcane Sanctuary pads: where each leads, not which were used

The user, caught in a loop of pads, asked to know which were used and how near the closest
unexplored area is, without taking the chaos from the place. A pad looks the same before and
after use and the automap does not draw pads (objects.txt), so a "used" mark would give more
than the game; what a sighted player has is the automap and the platform in view. Chosen: a pad
says, in words and by its loop, whether it leads where the player has stood, and an unexplored
area past pads is measured along the way through them in exploration mode too (an exception to
2026-09-24, as straight lines across the void said nothing). Rejected: naming the pad to take,
guiding through pads, telling the Summoner's arm. `docs/keyboard-play.md#where-a-pad-leads-said`.

## 2026-11-01 — No key turns the radar on or off (the user)

Ctrl+G is gone: the radar is switched in the settings list (Ctrl+S, BACK+RB), as everything without a
key of its own is. Ctrl+T still says its status; `access-radar on|off` stays for the console.

## 2026-11-01 — Holding Y says how to compare

The user pressed Y and used the right stick, as a tester did on 2026-10-13. Built and unit-tested.
- **Holding Y says "Comparing" and how the left stick reads it** (`compare-hint`, on by default)
  ([native tooltips](native-tooltips.md)). Compare stays on demand, the user's choice of
  2026-10-03. Rejected: reading the comparison on Y alone (that is `compare-aloud`, off), and
  the hint for the keyboard, which compares with Shift+Page Up/Down.

## 2026-10-31 — A pad plugged in late, and a closed dropdown's option

Two bugs the user found. Built and unit-tested only.
- **A pad used gives the menu back** ([keyboard play](keyboard-play.md#a-pad-used-takes-the-menu-back--2026-10-31)):
  any press on a pad releases the keyboard's cursor. Rejected: releasing on the pad's connection
  alone (a pad lying on the desk would take the menu from a keyboard player while the game stays
  in keyboard mode), and the game's `InputMethodChanged`, which fires both ways and says neither.
- **A dropdown says the option it shows** ([native UI](native-ui.md#a-closed-dropdowns-option-2026-10-31)),
  read from the widget before the `Settings.json` state. Rejected: a per-screen table for
  character creation's game version.

## 2026-10-30 — The tester's notes on build test-2026-10-30-2

From `D2R Access test build 2026-10-30-2 notes.txt` and a new `descriptions.txt`. Confirmed by the
tester: character titles in the main menu. Built and unit-tested only.
- **The difficulty window is "Select Difficulty"**, the game's own `strselectdiff`, not its layout
  name ("Character Difficulty Modal"). Rejected: the mod's own words, which the game's string
  already gives in every language.
- **The endgame areas' words are the tester's placeholders**, kept until they have seen them; the
  cows' text gets one comma fixed ([descriptions](descriptions.md)).
- **BACK+Y in hand-made maps: nothing new**; the note predates test-2026-10-30-3, which answers in
  every inside level. Left out: the Chaos Sanctuary, which the game marks outside and open ground,
  until the throne's logs show the chambers right.

## 2026-10-30 — The tester's notes on build test-2026-10-30

From `D2R access-2026-10-30.7z` (notes, the `no-slayer` logs). Confirmed by the tester: the Secret
Cow Level's portal in the scanner as it opens. Built and unit-tested only.
- **BACK+RT says the other players first** (`party-first`, on): Raine's ask, for summoners in
  multiplayer. Alone it changes nothing. Rejected: a separate key for the party.
- **Act V's wells loop** (`WellOperateFn`): the well handler, OperateFn 22, marks all 14 wells; the
  SubClass bit only five. Rejected: matching the Name column "well".
- **The Throne of Destruction's passages** ([rooms](rooms.md)): a preset level's streamed rooms
  are the generator's chunks and cut across open floor, so BACK+Y splits an inside preset level
  into chambers at the gaps in its walls (`Rooms::Chamber`). First released as a sentence saying
  the area is one hand-made map; the tester: the missing passages were the problem, not the
  wording. Not announced on entering: a chamber's bounds move as rooms stream in. Rejected: the
  streamed chunks as rooms.
- **The end screen reads itself** like the death screen (`SelfReadScreens` in `modal_reader.cpp`),
  its boxes in order; the expansion's has no "Vanquished". **Character rows say their title**
  (the row's `Title` box, "Slayer Tank"). Neither seen live: the log never reached the end screen.
- **"The Arreat Summit" answering too is by design** (the banner's form, [descriptions](descriptions.md)).
  Five areas have no words yet; the live reading of `descriptions.txt` stays until the user says.

From `D2R Access 2026-10-28 notes.txt` and a new `descriptions.txt`. Confirmed by the tester: the
Pandemonium Fortress's inside and outside, a Harrogath town portal and the route to it, Nihlathak
from anywhere in town, the combatants as "Barbarian", Terror Zones in the waypoint menu. Built and
unit-tested only.
- **The eclipse is never cut off** (`SpeechPriority::SayWhole`): after its wait for a pause it cuts
  in and holds for as long as its words take (`SpokenMs`, the tutorial's pace), whatever the Quest
  updates level but "waits its turn", which still queues it. In a fight the wait ran out and the
  enemies' names cut it off; it cannot be had again without a new character. Rejected: a new
  priority kind for it, saying it again on entering each area.
- **Anya not found from the town portal by a second player: not changed**; the tester struck it
  as a hiccup until a new log. The rule takes her rescue's reward (row 37 bit 0 or 15) from the
  player's own flags.
- **The tester's Act V words are taken** with "Arreat Summit" as the game names it, and four typos
  fixed.

## 2026-10-28 — The tester's notes on build test-2026-10-26-3, "no route" in the Outer Cloister

From `no route_d2r-access.log` and `D2R Access 2026-10-26-3 notes, descriptions and Act V fix
requests.7z` (notes, `descriptions.txt`, the `act-v` and `portal confusion` logs). Confirmed by the
tester: the comma before the hit chance (NVDA, SAPI 5, Zira), the Cott4 cottage's creak. Built and
unit-tested only.
- **Unexplored areas with no way in are not listed** (`LeaveOutShutRooms`): the "no route through
  explored areas" refusals were right, the listing was wrong. Eight Outer Cloister rooms were solid
  roof. Left out by the kept map's whole-level reach, in every mode, rather than by the Ctrl+U reach
  filter (which also drops things not walled off) or by a special route message. Rejected: letting
  the route guess through known walls.
- **The Fortress's interior follows its walls**: its HD layout is 22 subtiles off its DS1, measured
  from walls, fireplace and braziers against the log's collision; a per-file offset in the generator.
  Rejected: redrawing the volume by hand.
- **The Hellforge and the pentagram are map landmarks**, from the DS1 objects that mark them
  (`SoulStoneForge`, `DiabloStart`), not from HD models (the star's model sits 7 subtiles off). The
  pentagram is listed; the Hellforge, already listed as its object, only gets its words.
- **A portal never moves to an area border** (`Crosses`), and objects drawn with a portal's art
  are portals (`PortalArtObject`, from objects.txt Tokens). Rejected: a list of class names.
- **Nihlathak is listed from the game's own rule**, decompiled: the Prison of Ice data is reset each
  game, so the mod keeps the levels entered this game itself; level 112 counts as gone even when
  the game would keep him (never a phantom NPC), and multiplayer gives no answer. Rejected: the
  Betrayal of Harrogath row (the game never tests it).
- **Travincal's revealed door is named from a warp beside its room**; catapults are ranged by class
  (their `rangedtype` is empty); the battlefield's combatants take the game's "Barbarian".

## 2026-10-26 — The tester's notes on build test-2026-10-25-2

From `D2R access test build 2026-10-25-2 notes.txt` and a new `descriptions.txt` (no logs).
Confirmed by the tester: Jerhyn and Natalya from anywhere in town, far unexplored areas with "parts
of it crosses ground not seen yet", the speech priority menu, routes through Act III's jungles.
Built and unit-tested only.
- **The area hint is a switch of its own, off** (`description-hint`): the tester wants the lore met
  at the player's pace, and the manual will tell of BACK + right stick up, which stays on. The
  `descriptions` switch keeps the landmark words. Rejected: turning `descriptions` off by default (it
  would take the Hangman's Tree's words with it, and a saved "on" would keep the hint).
- **A comma after the number before the hit chance**: the right stick already said "Attack Rating.
  602. Fallen: 95%...", and the tester heard the stop after "Rating" but none after "602", so the
  voice reads a period after a number without one (ordinal or decimal, unconfirmed). Rejected:
  speaking the chance as a second phrase (queued phrases run together just as much).
- **Unfolded stairs take their temple's name**: the Kurast temples' entrances are the OpenWarp
  objects `UnfoldingStairLeft`/`Right` (objects.txt 194, 195, key "stair"), all "Stairs". Once in
  mode 2, the object is named like a warp after the level its room's walk-through warp leads to (row
  61, "Act 3 Kurast to Temple"), and the remembered exit takes the name. The rule is every OpenWarp
  object's, so Lut Gholein's trapdoor, once open, reads "To The Sewers Level 1". Rejected: naming them
  before they unfold (the game hides which temple until then) and a per-row special case.
- **Words for Act III and the Pandemonium Fortress** ship in `data/descriptions.txt` (96 areas and the
  Hangman's Tree); Kurast's sewers keyed by Id 92 and 93 like Lut Gholein's 47–49.

## 2026-10-26 — The tester's roundup of older issues (build test-2026-10-25-2)

From `D2R Access, older issues revisited.7z` (the notes, five log pairs: death and trading, npcs,
storytime, trade, dungeon). Closed by the tester: the walked-distance click, the low-life alert with
its heartbeat, the Forgotten Tower's quest exits, the trade gold window, the tower ruin, stables, Lut
Gholein's sewer grate, the sewer descriptions. Built and unit-tested only.
- **A shop's buy confirmation is a pop-up too**: it is a list in `NpcDialogPanel` while the shop
  stays open, not a "...Modal", so the 2026-10-17 rule missed it and every buy said "Ormus. Misc."
  again. A dialogue panel over a shop still open counts; closing the shop does not (its panel is gone
  first). Rejected: every `NpcDialogPanel` focus (leaving the shop and coming back to the same cell
  would then say nothing).
- **Life 0 says nothing**: "Life 0 percent" interrupted "You have died..." 60 ms after it. Death is
  the death screen's to say. Rejected: queueing the alert (a dead character's life is no news).
- **Jerhyn and Natalya from anywhere in town**: they are no monster preset but marker objects whose
  InitFn spawns them; the mod lists them at the marker the game's own test picks, read from the
  player's quest flags (Jerhyn has two places, Hratli two). Nihlathak and Larzuk keep "once seen": no
  flag tells their test. In a town every loaded NPC now counts as seen, wherever it stands. Rejected:
  remembering their places between games (Jerhyn moves with the quests).
- **Item panning's 100 is the old 50**, the testers' choice; the setting's key changed so a saved 50
  is not halved again.
- **Every unexplored area** (`unexplored-everywhere`, on): the tester wants to head for far
  unexplored ground on purpose, and the user wants it the default (2026-10-26), replacing the
  2026-09-24 rule (only the edge of the known map, so the list does not lay out the level). Off
  brings that rule back. Released first off as test-2026-10-26, then on as test-2026-10-26-2.

## 2026-10-25 — The tester's list on build test-2026-10-25

From `d2r access test build 2026-10-25 notes.7z` (the notes, six log pairs, two Flayer screenshots;
the "sewers_2" log it names is not in it). Closed by the tester: the black square (the game's chat
background), the sewer descriptions, the palace entrance, "Open sockets". Built and unit-tested only.
- **A stair is taken within 4 subtiles of it, from any side**: the pad's room-tile target
  (`controllersettings.json` RoomTile, distance only). Five positions in the log fit it; the route's
  end box (corners at 4.2) and "Arrived" within 3 of the way's end (up to about 6) did not. Routes to
  a stair now end on cells wholly within reach and "Arrived" waits for it. Rejected: per-tileset
  "open side" tables (the walls only matter because they keep the body out of reach).
- **A label that carries an exit's whole name is the exit's**: a Flayer behind a wall hid "To The
  Flayer Dungeon Level 1" at its stairs. Rejected: dropping the wall gate for labels (it stays right
  for monsters).
- **Hit chances from the game's own builders**, called as its hover does, not reimplemented (the
  monster, the difficulty and MonLvl make a copy fragile). Rejected: moving the game's focus with the
  stick (it would change what the pad's own navigation does next).
- **No unexplored room the scanner lists means 100**, as the tester asked; each room's drawable count
  is kept in the archive (v4), so a reload no longer drops 98 to 88.
- **The sewer stairs are told when they open, once a game**, and still not stored: a new game shuts
  them until the lever is pulled again.
- **Trade grids say whose offer alone**; the window's title before it was cut off by the next step.
  Kept: "Your offer" rather than the player's own name.

## 2026-10-25 — The tester's list on build test-2026-10-24

From `d2r-access test build 2026-10-24 notes.7z` and its addendum (the list, four log pairs, three
Flayer screenshots, the spell-checked descriptions). The black square is the loader's, not the mod's
(the tester, with the plugin removed). Built and unit-tested only.
- **The explored share is drawn cells over drawable cells**, from the game's own AutoMap table and
  each streamed room's tiles (offline RE, eight new fingerprints). Rejected: the walkable ratio
  (saturates at 30-50 % where AutoMap.txt draws walls only); tiles within a radius of a cell (a hall's
  middle is never near one); the tile's "processed" flag (not saved, so a new game reads 0). Fixed
  outright: the old number was simply wrong.
- **Objects named after a level are exits**, Tal Rasha's Chamber first. Rejected: a list of object
  classes (the Levels key already says it, and covers the Act V portals).
- **Level critters are left out**, not filed as NPCs: the larva is the only one that leaked, and a
  named "NPC" crawling in Duriel's lair would confuse as much.
- **Town landmarks from anywhere**, like exits: the palace hall is never charted from outside.
  Rejected: a "palace gate" landmark of its own (no gate model in the layout).
- **Folds stay off walls**: a long-legs fold whose straight line crosses a wall or object cell is not
  made. The body may brush a door frame (the 2026-10-22 trade), so `crossable`, not the body test.
- **Cott4's door**: one edge of a doorway face may stop at an inner corner. Rejected: one open cell
  each side (four false doorways on level 2 offline).
- **Dead ends need the corridor walled across**, not one cell on the player's line: all six logged
  "dead ends" went on. Rejected: following the corridor's centre line (a slanting wall still stops it).
- **Descriptions by Levels.txt Id**, `Name [47]`, as the tester asked; the name stays the common
  case. The area hint, silent since it looked the banner up as a name, now uses the level.
- **"Open sockets"**, the community's word, in English only; the other languages keep theirs.
- **Stairs' approach**: no routing change. Eight logged uses fit "within about 3 subtiles, any side"
  and no LvlWarp offset; the next log carries each stair's row (`[exit]`). Rejected for now: routing
  to unit + ExitWalk or + Offset (contradicted by the samples).

## 2026-10-24 — The tester's list on build test-2026-10-23-2

From `D2R Access test build 2026-10-23-2 notes and bug list.7z` (the notes, three log pairs, two
screenshots, the tester's Act I and II descriptions). Built and unit-tested only.
- **The death screen says itself**: its layout name ("You Died Modal") is no longer announced on
  open or close, so "You have died…" interrupts at once. Fixed outright, not behind a switch: a
  layout name is not a title, and the body already says the death.
- **The tester's descriptions come with the mod**, embedded in the DLL and read under the player's
  own `descriptions.txt`, which still wins line by line and is never shipped. English only, keyed by
  the game's English names. Rejected for now: moving them into the strings layer (68 long texts
  times 13 languages); a loose shipped file (an update would overwrite a player's edits to it).
- **The "black square" is the HUD notice list** with empty rows (see
  [native UI](native-ui.md#hud-notice-rows-without-text-2026-10-24)). The mod only reads that list,
  so nothing is hidden yet: the next build logs what the empty entries hold, and the tester is
  asked whether it appears with the plugin removed. Rejected: hiding the list's background (alters
  the game's UI before the cause is known).

## 2026-10-23 — The tester's list on build test-2026-10-23

From `D2R Access test build 2026-10-23 notes.7z` (the list, five log pairs, a log bridge
screenshot). Built and unit-tested only.
- **Stairs keep the game's 45-degree retry** where the player threads a passage (walls on both
  sides across the push); the eight-way fallback and any push into an open wall still stop, as the
  user asked on 2026-09-22. Behind `narrow-slide`, on: it is the fix for the tester being stuck,
  and the switch gives back the old rule. Rejected: diagonal retries only (left the Palace
  Cellar's 3-wide gaps stuck); turning the whole prevention off by default.
- **Log and plank bridges are "bridge"**, one word as the tester asked; "log bridge" and "stone
  bridge" were not worth the extra word. Their footprint is their collision mesh, as a log's model
  sits at one of its ends.
- **The palace entrance is routable as its hall**, listed whole and as wide as it, so the route
  ends at its door. Rejected: a new "palace gate" landmark (no gate model in the layout).
- **The Moldy Tome's piece is no tower ruin**; the tome is a quest object already.
- **A doorway's flank may be a building's corner** (two cells of wall turning into a tile of wall):
  Cott3's door. Rejected: two cells of flank anywhere (would take gaps between pillars).
- **Digits one step down from "a"** on the typing reel, in the tester's order 1 to 0. Rejected:
  LB/RB group jumps (the settings list takes the shoulders every update).
- **"Drop Gold closed" dropped after a confirmed transfer**; a cancel still says it.

## 2026-10-22 — The tester's list on build test-2026-10-22-2

From `D2R Access test build 2026-10-22-2 notes.7z` (the list, four log pairs, the basin from both
sides). Built and unit-tested only.
- **The cistern is the DS1's `BloodFountain` object**, not the model: the HD layout has no basin
  model, and `ritual_platform01` is the dais of her chest. The generator reads DS1 object lists
  (`objpreset.txt`) for it. Rejected: the tester's TombPool/ID 118 (objects.txt 118 is a well).
- **Tristram's fountain tracked** as the tester's way out of the burning houses; the houses'
  doorway creak is left (no collision of one to tune the gap rule against; "not essential").
- **"Character deleted"** only when character select sent `DeleteCharacter` before the close.
- **Turns 5 units apart by folding short legs** (`route-long-legs`, on, as the tester asked to try
  it): the way is walked slightly off its cells. Rejected: a minimum in the leg search, which
  detoured to twice the length to keep exact alignment.
- **Steps are the click's interval** (`distance-steps`, off): "one click equals one step", so the
  step follows `step-click-interval`. Off by default: it changes every distance said.
- **The crosshair in the tutorial**: its sound in the camp's monster sounds, its meaning in the
  fight slide, as the tester asked.

## 2026-10-22 — The tester's list on build test-2026-10-22

From `d2r access test build 2026-10-22 notes.7z` (the user's Downloads: the list, five logs, the
Countess's vessel and a secret door before and after). Built and unit-tested only.
- **"Paladin Aura" at loading** was the focus reader's fallback reading the HUD's aura widget, which
  the game focuses while loading a paladin's game. HUD panels (`PanelState::Hud`) are never read by
  it. Rejected: silencing the fallback for the whole load screen (it names real menus elsewhere).
- **The delete countdown is the mod's own**, from the press, five seconds as the tester counted; the
  Confirm button's text is logged to learn whether the game writes it there.
- **One "stairs" per cliff stairway**, at the tester's suggestion: halfway, wide enough that either
  end counts as arrived. Rejected: fixing the route to the bottom landing alone, the bend stays.
- **Doorways listed with the doors** and **in Act I's buildings and Tristram**. Outdoors only inside
  a building's map piece, since the gap rule would take a gap between trees for a doorway. They
  are not told as found: a crypt has dozens.
- **The cistern** is the only Countess-specific model in her layouts (`ritual_platform01`);
  unconfirmed that it is the vessel. Named as the tester wrote it, in every language.
- **Portals by owner only**, the tester's own fix, plus one portal per owner in the beacon too
  (`Recreated`). Rejected: polling the roster for the new portal (its ids never refreshed on the
  client in this log).
- **Secret doors as a draft, not a door**: never listed while shut, felt once within 10 units. Not
  built: routing through them (their collision is unread) or hiding the loot behind them.
- Not acted on: the tester's note that legs under 5 units are hard to walk; they will keep watching.
- **Jail doors marked as objects** count as doors where a shut door unit stands. Rejected: every
  OBJECT-only cell as openable (a route would run into chests, racks and statues).
- **The passage finder is removed** (the tester's proposal, the user agreed): the room line, BACK+Y
  and the running announcements already say passages. Its combination is left free rather than
  given a new job now; `pad-route-on-a` stays, so the route keeps the button each player chose.

## 2026-10-21 — The tester's addendum on builds 2026-10-20 and 20-2

From `tutorial and builds -20 and -20-2 feedback.7z` (the user's Downloads: the addendum, three
logs, Tower Cellar doorway screenshots; its tutorial list was the one already done). Built and
unit-tested only.
- **"Unexplored area", not "room"**, in every language: the user's choice on the tester's case
  (outdoors there are no rooms; one hall of the game is several streamed rooms). The room
  announcer's passages keep "room".
- **Unexplored areas one per direction in turn**, behind `unexplored-spread` (on): only the order
  changes, so nothing past explored ground is revealed. Rejected: a different set per direction.
- **Doorways from the walls' gaps**, not the streamed rooms' seams, which run through halls and
  corridors. Rejected: the tester's DS1 offset (the mod reads no DS1; the seams, not a scale, were
  wrong) and the automap's yellow doorway marks (their cell numbers are unknown; worth a look if
  the collision rule misplaces arches).
- **Mod+B counts drawn walkable tiles**, not rooms: one drawn tile charted a whole room, so a new
  level read 20 % and a walked maze 100 % with blank tracts. Rooms never streamed are estimated
  from the others' density.
- **Map landmarks (outdoor stairs) are routable**: the route took only unit ids and silently kept
  the last offer. A scan that cannot be routed now clears the old offer.
- **Town NPCs without a preset (Jerhyn) are remembered once seen**; the presets do not place him.
- **A party member's portal: replaced by its owner's new one, dropped when the roster names
  another**. Whether a client's roster ids follow the host is unverified; the drop is logged.
- **The eclipse waits for a pause in speech** rather than a new priority level.
- Not built: synthetic 5-unit footsteps (the tester's mid-term idea, for the user to decide).

## 2026-10-21 — The tester's list on build 2026-10-20

From `d2r access test build 2026-10-20 notes, feedback and logs.7z` (the user's Downloads: list,
three logs, screenshots of the Tower Cellar grave doorway). Built and unit-tested only.
- **The grave was the route follower, not the planner.** The four Grand Charm routes all went
  round the grave; the follower said the second leg at once, as a first leg of 3 units was within
  its turn lead, and that cut the route off. Rejected: adding object collision to the planner
  (it already blocks the grave's cells) and a collision check of the legs (they already fit the
  body). A turn now waits for half its leg.
- **A room is arrived at anywhere in it by the route too**, as the beacon's. Rejected: a wider end
  distance (an L-shaped room would still miss it, and a point target would arrive early).
- **Arrivals hold 1.5 s**, a new speech kind with that default, rather than quieting the cursor's
  hover names (they are what a sighted player reads at the door).
- **The click a sound of its own, the beep kept behind `step-beep`**; the click is the default
  because the beep was ours, not the user's choice. Clicks stay off by default until the tester
  has used them at 5 units (they suggested on, once the settings menu is reworked).
- **Own volumes past the mix headroom**, for the step, the doors and the doorways. Rejected:
  raising `MixGainLimit` (it would lift every loud pile and the chest's +4 dB, tuned by ear).
- **The main menu's news behind a switch, on by default**: it is what a sighted player sees.
- **"Act 1" from the game's levels table** rather than our own words, so it follows the language.
- **Doorway recording trimmed here, not replaced by the stash sound**, as the tester allowed.
- **"Unexplored room" kept** for now; "area" is the tester's vocabulary question, left to the user (became "area" in the next list).

## 2026-10-20 — The tester's list on builds 2026-10-19 and 19-3

From `D2R Access build 2026-10-19 and 19-3 feedback and bug list.7z` (the user's Downloads: list,
screenshots of a Tower Cellar doorway, the Inner Cloister and the Outer Cloister fountain, a
creaking-wood recording; no logs). Built and unit-tested only.
- **Doorway sounds from the room openings, crypt tile set only**: the gaps between two streamed
  rooms the room announcer already reads (`Rooms::Doorways`), in levels of LvlTypes row
  "Act 1 - Crypt", as the tester asked to start. Rejected: reading the automap's doorway marks
  (nothing reads them yet, and the collision gives the same gaps) and every maze level at once
  (cave openings are no doorways). On by default, behind `doorway-sounds`.
- **Opened doors keep sounding only by choice** (`open-door-sounds`, off): silence on opening
  was the earlier choice and stays the default.
- **The step click on the item mixer's own one-shot channel**, not the cue synthesizer: that
  plays one cue at a time, and a click every unit would have taken enemy cues' turns.
- **A shut door at an area crossing keeps the exit there** rather than at a corridor's mouth
  (the Cathedral's doors past the colonnade). Accepted risk: the Monastery Gate's entry, moved to
  its corridor's mouth on 2026-10-18, goes back to its inner door if that is a door object.
- **The eclipse said only on its start in the Lost City**: a joined game's first flags in a town
  are no eclipse. Rejected: saying it on every entry while it lasts (a sighted player sees it
  once).
- **Discoveries in a level's first 2.5 s wait for the banner** instead of being spoken under it.
- **Route grave case: logged, not guessed.** No log came with the screenshots; each route now logs
  the collision around the player, and the planner is unchanged until a log shows the cause.
- **The fountain as a map landmark from its object** (`RogueFountainClass`), not a place word:
  the place names come from the HD layouts, which do not hold it.
- **Natalya: a log line, not a fix**: three causes fit, and the new `[survey]` presets column
  tells them apart.

## 2026-10-19 — Route legs along the stick's eight ways

The tester's "Fundamentals: Route finder (Mod+A)" (the user's Downloads: text, screenshots of a
cottage door, the Den of Evil, the Barracks and a Crypt doorway, painted with the scanner's line,
the route's and the right one, and the `wf_` logs, on `test-2026-10-19`): every way in D2 is
walked at 45 degrees, and a route of any-angle legs cannot be. In the logs, "east, 22" toward the
Den of Evil was a leg of (15,-22) walked as the stick's east (1,-1), past the cave mouth; the
Crypt's "north, 18" a leg of (-16,-9) that the stick's north (-1,-1) walks into the wall beside
the doorway, where the tester's line is northeast, then northwest. Built and unit-tested only.
[Route description](route-description.md).
- **Each leg one of the eight ways**, found by a search that charges a turn 3 units, within 8
  cells of the planner's way; a diagonal step needs the body to fit on both cells beside it, so a
  doorway is crossed along its axis. Rejected: splitting each straightened leg into a diagonal
  and an axis part (its turn would fall wherever the split puts it, not at the doorway), and
  dropping the corner test for diagonals (it allows the diagonal into a doorway that the tester
  showed walks into the frame).
- **A route ends anywhere it counts as there** (the beacon's arrival area), and a door is opened
  from beside it: no last jog onto the planner's own cell.
- **Arriving is walked** (`LevelRoute::Arrival`, the planner's goal too): only cells of the
  arrival square walked to from the place, over open ground and no shut door, count. The Den of
  Evil route "arrived" 7 units from the mouth, which rocks close on three sides, and the tester
  needed two more routes to get in. Not for a border whose crossing is unknown, nor for a room
  (anywhere inside it).
- **The turn said half a second ahead at the player's pace**, not 3 units at any pace: on a leg
  that ends on a doorway's line, early is the frame.
- **Laid out 96 cells at a time**: the search over a 720-cell way took 24 ms in one piece, 4.5 ms
  in stretches; the beacon's ticks already reach 35 ms in the logs.
- **The burning star told across its piece** (`Places::Reach`): it was told when almost on its
  points. Only that landmark: the forge stays a tile, as the user asked for passing through it.
- **Unit ticks not built yet**: the tester asked for an optional click every unit walked, to
  judge the distances. Following a route no longer needs them (the turn is said, then the
  arrival); offered, behind a switch.

## 2026-10-19 — The route finder into walls and after other players

The tester and a second player, in chat (no log yet): BACK+A "leads into walls but not through
openings", says east or west where doorways are always crossed NE/SE/SW/NW, and in multiplayer
said "TankMan, 19 units west" with the ping at "11 units northeast" until the scanner named him
again. Built and unit-tested only. [Route description](route-description.md).
- **Compass points as walked**, everywhere (the user, so the ping and the scanner agree with the
  route): the stick's and the arrows' eight ways (measured 2026-09-25: the game turns them 45
  degrees into the world), not the screen's 2:1 projection. This ends the 2026-09-25 rule that
  bearings keep the screen's directions. Clock hours unchanged (the projection's 2/4/8/10 axes).
- **Legs the body walks straight**: straightened with the body's footprint, and a short leg folded
  only when the leg it joins stays clear, so the step through a doorway is said.
- **Live target**: the route re-reads what the scanner named before planning, and while it is
  followed re-plans for a target that walks on, saying the leg only when it changed. Rejected:
  a full route spoken on every re-plan (a moving player would bury the speech).
- **Item and place panning** (`item-pan-25`, the user's choice): a second player heard item
  sounds too far left and right. One setting for the whole "item and place" family, as its
  volume is; it scales the balance, not the emitter's position (scaling the position leaves a
  source straight to the side fully in one ear).

## 2026-10-19 — The tester's list on build 2026-10-18-2

From `D2R Access test build 2026-10-18-2 bug list and feedback` (the user's Downloads: list with the
user's notes, `menu_walk_`, `death_`, `sewer_`, `wrong_house_` logs, sewer screenshots), a second
player's report on guidance, and the tester's multiplayer notes. Built and unit-tested only.
- **Always-latest download link kept** (the user: many blind players cannot find their way around
  GitHub). The tester's four logs all say `test-2026-10-18-2`; the build is said at start.
- **Speech priority menu** (the user): one level per kind of news, four levels, defaults equal to
  the old behaviour except status endings, which cut in as the tester asked; introduced gradually,
  menus and focus left out ([speech priority](speech-priority.md)). A status end waits 500 ms, not 1 s.
- **Phonetic name replacements not made** (the user): the names are the game's own text; a voice that
  says them wrong is fixed in the screen reader's speech dictionary, which then works everywhere.
- **Controller glyphs from U+E013 are UI actions** with fixed PC bindings: decoded by a fixed table,
  not a runtime call ([controller glyphs](controller-glyphs.md)). Item tooltip lines keep their
  buttons; glyph-only lines stay silent. Not the Steam controller: the code was unknown to the mod.
- **Front-end right stick** read by a reader of its own out of a game (`InteractionReader::TickFrontEnd`):
  the in-game readers never run there, so the character row's controls were offered and never read.
- **Descriptions** read again on save (checked at most once a second, on request), a names list
  written per run, and the banner's form of an area name accepted only when it ends with the
  level's name ([descriptions](descriptions.md)). The tester asked for a temporary on-demand load;
  the write-time check costs nothing and can stay.
- **Porch cottage** is `Cott5`/`Cott5A`, `Cott1` a plain cottage; **sewer grate** added
  ([places](places.md)).
- **Guidance at a shut door**: the player's cell is the waypoint, not a refusal ([beacon](beacon.md)).
  The other player's "120 units" guess was checked: there is no such limit. Their log is asked for.
- **Multiplayer**: the mod sends nothing over the network and changes no game state; each copy reads
  its own client. Explained to the tester with the mod's poll rates; a checklist was sent.

## 2026-10-18 — The tester's list on build 2026-10-18 and the leftovers of 2026-10-16

From `D2R Access build 2026-10-18 bug list and extras.7z` (the user's Downloads). Built and
unit-tested only.
- **Scanner order kept.** Nearest first, right farther, left nearer, wrapping, the cursor kept by
  identity for 10 s: the log showed the radiant chest as entry 1; the tester was already past it.
  Explained, not changed.
- **Character select stick.** Out of a game the scanner no longer takes the right stick
  (`MapSurvey` waits for `LocalPlayerReady`): it answered "nothing found here", so right stick up
  never reached the row's controls. The short hint is said for every character rested on.
- **Vendor tabs under the trade menu.** The trade/repair menu still owns speech until the cursor
  lands in a grid; tab changes TabReader reports there are now said (it already holds the menu
  switcher under a dialogue). A bar that first appears under the menu is a silent baseline.
- **Fortifying potions** (antidote, thawing): "You are fortified" after any pending effect end,
  once per potion kind running, no end notice, as the tester specified ([status effects](status-effects.md)).
- **`skill-report` on by default** (the tester: it costs nothing until a running skill is used).
- **Other players** take the NPC cue, party or not: the tester asked for party members; a hostile
  player is rare and still listed by name in the scanner.
- **Greater Mummies** (and Radament, who keeps his boss cue) are raisers: monstats `GreaterMummy` AI.
  A raiser outranks the elite cue (the tester named the champion and unique mummies), not a boss.
- **Stone door** re-cut from the same recording instead of a new sample (none to hand): saturated
  and brightened, [ground items](ground-items.md#door-loops).
- **Cliff stairs** landings span the stairway's width; a stairway left 15 s holds no more
  ([places](places.md)). **Buildings** are named per piece; porch cottage, tower range, sewer
  "tables" removed ([places](places.md)).
- **Horadric Cube**: one move diff names the destination whatever the button; counts as
  "3 Chipped Topaz" (no plurals across 13 languages, no "×", which readers say as "times")
  ([Horadric Cube](horadric-cube.md)).
- **Outer Cloister exit** at the corridor's mouth facing the area the player is in
  ([beacon](beacon.md)).

## 2026-10-18 — The tester's list on build 2026-10-17

From `D2R Access test build 2026-10-17, bug list, screenshots and logs.7z` (the user's Downloads).
Built and unit-tested only.
- **Waypoint words** were computed but never said: the row's button was snapshotted alone, so the
  hook saw the button's name ([waypoints](waypoints.md)). The emote wheel's words after a row came
  from a panel that draws nothing; such panels no longer match the highlight.
- **Places by piece file** ([places](places.md)): cottage, stable, cottage on fire, tower ruin;
  blood fountain and burning buildings (Tristram, Act V ruins), the crypt's burning star, the
  Hangman's Tree padded one tile. **Cliff stairs** from their collision mesh: a landing at each
  end, "stairs up" at the bottom and "stairs down" at the top, as the tester asked. Rejected: one
  footprint for the whole stairway (its centre can fall off the walkable stairs, and the beacon
  leads there) and telling the direction by movement (no height in the walk grid).
- **Shop confirmations** (buy with A): back on the cell it came from, the shop and its tab are not
  said again, so the new item's notice is not cut off. The stash no longer says its title.
- **Heartbeat**: the alert plays on its own voice, so the beat starts with it, not after its tail
  (rejected: cutting the recording's reverb).
- **Stamina potion** in the tester's words; **running skills** in BACK+X behind `skill-report`
  (off: the tester asked for an option), derived from skills.txt `aurastate`, not a list of names.
- **Character select**: the hint only while the list is in front, once a visit, and short ("Press
  right stick up for more options", the tester's alternative); the full controls are a row's
  right stick up. **Rebinding** says "<skill> bound to <button>" instead of the window closing.
- **Lobby**: "Editing" said when a box starts editing (A worked; the game's entry panel is mute),
  the stick's up goes forward, A types. Not done: Enter ending the edit (no end-edit call known).
  Main menu arrows: up and down are kept from the game's character list while the keyboard
  cursor is engaged there (`WH_GETMESSAGE`, as for letters); unmeasured whether the game reads them
  as messages.
- **Radiant chests**: objects.txt rows named "chest" with a `Lit0` light (seven: the Horadric
  Cube, Scroll and Staff chests, Khalim's three, the reward "sparkly" chest), "Radiant <name>".
- **Forgotten Tower** quest ways only while its quest runs, by the client's own quest flags
  (found in the image this session, [quests](quests.md)), not by what happened this session (the
  tester's condition). Unreadable flags keep the old rule. Rejected: a switch for the chime (the
  tester asked for the quest state) and calling the game's quest-text builder (a game call).
  The tower piece itself is the landmark "tower ruin".
- **Shaman** layer glides up one semitone (the tester's cap). Gargoyle traps, sarcophagi and fire
  towers are already enemies (killable monstats rows) with the ordinary growl: a sound of their
  own waits for the tester's design.
- Not changed: edge exits' discovery notice (a known place or the way back is never news; a log is
  asked for), the descriptions hint (only areas with words get it; the template's are notes).

## 2026-10-17 — The tester's list of 2026-10-16 and the waypoint evidence

From `D2R Access build 2026-10-16 Bug list.txt` and the tester's waypoint folder (screenshots and
a log with every Act I row focused). Built and unit-tested only.
- **Waypoint rows** from the game's own row table (`NativeWaypoints`, [waypoints](waypoints.md)),
  not from the widgets: the rows are identical clones, and the old heuristic called every row
  locked. Words: "undiscovered" (the tester's) and "you are here"; "Terror Zone" from the row's
  skull. Rejected: reading the text colour (+0x8D4), which a terror zone overwrites and focus may.
- **Town portals** ([portals](portals.md)): the 16-bit level read took the portal's state flags
  for the high byte; now +8 plus the loader's +0x78. The owner is added, roster first.
- **Forgotten Tower**: the quest loop belongs on the Black Marsh's door (Levels.txt 20, the tower),
  not only the tower's stairs (21). Every step down the chain 20-25 is a quest way; the way back
  up is not (`EntityRadar::QuestWay`).
- **Heartbeat** twice as fast: the recording's ten beats (52 a minute) each cut to their lub-dub
  and 0.58 s, so 103 a minute with the beats' own variation. Rejected: a faster playback rate
  (an octave higher) and a pace setting (one asked-for speed).
- BACK+RT alone: "You are all alone" (the tester's words). Character select hint: A, Start, X in
  the tester's order.
- **Descriptions** (the user approved, with a switch; [descriptions](descriptions.md)): the
  Hangman's Tree as a landmark whose words replace its name the first time; areas described by
  hand in the player's `descriptions.txt`, never shipped so updates keep it; BACK + right stick
  up reads them; a hint once per described area. Rejected: the hint on every area (long speech
  for little gain, the tester's own worry) and shipping the file (an update would erase the
  tester's writing).
- **Lobby typing** ([multiplayer](multiplayer.md)): the mouse click on a box the game did not edit
  is removed, since a real mouse event switches every panel to its mouse layout and closes modals
  (Direct Join). The mod calls the game's begin-edit instead. Up/down walk the form's own order
  rather than the nearest rectangle. Rejected: keeping the click with a layout guard (the reload
  is the game's, on any mouse input).
- **Act II list**: the overview's "percent explored" now counts the rooms the automap drew or the
  player stood in, as the tracker does; the streamed rooms (most of an outdoor level) said 100 %
  with a dozen blank. The desert's cliff stairs are a place and a tracker entry, said "stairs" (the tester's word); a
  feature of their own so that only they, not every staircase prop, join the tracker. The stash names its tab once the focus lands, wherever it lands.

## 2026-10-16 — Loop speeds

- The user asked for the pace of the loops that are not constant (doors, scrolls, the wall knock)
  beside their volume. One setting per kind of pulsing loop (`LoopPace`), the drop sounds'
  mechanism generalized; steady textures get none (a pace would only cut or gap them). A new
  **Loop speeds** group gathers them with the drop speed, the wall knock repeat (now from 250 ms)
  and the corpse rattle. Rejected: one speed for every loop (a door and a shrine want different
  paces) and a speed per sound (forty settings).
- **Radar wall repeat** (`radar-wall-repeat`, off): what the user meant by Raine's wall request is
  the radar's wall, not the bump. The wall on the ray the player heads along (velocity, or a
  refused push while standing) sounds again every 250 ms to 1.5 s while it stays; the other rays
  keep once per change, or a corridor's side walls would knock without end. Rejected: repeating
  every ray's wall. The bump repeat stays as well.

## 2026-10-16 — The tester's lists III and IV (build 2026-10-15)

Worked from the two lists and the host's log of a two-player game (`[speech]` lines; that log is
build 2026-10-14-2, so it shows the lobby's D-pad working before 10-15 and nothing of 10-15's
features). Built and unit-tested only.
- **Lobby D-pad** (regression of `pad-typing`): since A starts editing a box, the game's editor
  stays active after the focus leaves it, and `MenuNavigation` took that for typing and left the
  D-pad to the stock cursor, which skips the boxes. Only the focused box counts as typed in; in it,
  left and right stay with the caret, up and down leave it.
- **Area overview**: a landmark already said as a side's border is not said again. **Crossings**
  need a shared edge: areas meeting corner to corner (the Monastery Gate from the Black Marsh) have
  none. Rejected: hiding a level until the player has been in its neighbour (the tester's rule;
  geometry says the same without a per-map list). Unverified that the Black Marsh case is a
  corner; a crossing remembered on an old map stays listed there.
- **Forgotten Tower**: the loaded warp replaced its preset and lost the quest category; it keeps it.
- **Gone targets** (multiplayer): the beacon kept leading to where an item or Cain had been. A
  target read absent from the player's room or one touching it for 2 s (`VanishedOnArrivalMs`)
  is lost at once; exits are left to arrival. A remembered quest object whose spot the map can see
  without it (`CorpseMemory::Covered`) is forgotten for the run (the archive only adds places).
- **Status effects**: a beginning cuts in and holds 1.5 s (`ScreenReader::Urge`). **Shrine
  blessings** say when they wear off, not when they begin (the shrine says that).
- **Low life heartbeat**: the Diablo IV start and loop the user chose (from the Last Epoch mod),
  threshold 5-70 percent, release five points over. Rejected: a spoken warning (it is what lagged).
- **Volumes per enemy class**: normal, elite, ranged, shaman, boss, each a factor on the enemy
  volume. **Wall knock repeat** (`wall-bump-repeat`, off): the thud again every 0.5-2 s while the
  push goes on; the tester's swoosh complaint is answered by the knock, not by a new tone.
- **Corpse rattle** in milliseconds (`corpse-interval`, a new key so a saved 3 s is not misread),
  1 s by default, a drop sound's pace.
- **Experience** names the next level. **Burning bodies** removed (the user: not now).
- **Main menu** is named ("MainMenuPanel" was spoken raw) and a panel's title holds 700 ms, so
  the first focus queues behind it; other untitled panels are said in words.
- **Character select hint** after 5 s on a character (it was armed by the active panel, which is
  the link bar, so it never fired). **Stash**: the tab (and shared page) said on opening; the shared
  pages (RB/LB) are a text box between arrows, not a tab bar, and are read as they change.
- **Not done**: locked waypoints (the images the 10-15 heuristic looks for are templates, not the
  rows' children; each focused row is now logged, `[waypoint] row`, to find what differs); house
  and ruin entrances (no detection exists; a wall-gap finder over the collision grid seeded by
  the interior volumes needs a live tuning session, and Tristram has no volumes); Cain's chime
  after his quest (the host's log rings it only while his mark was real: needs the tester's log).

## 2026-10-15 — The tester's ninth and tenth lists (worked alone, no live check)

- **Area overview by the level's own sides** (BACK+B): the tester heard screen sides where Diablo's
  areas are diamonds. Legs now walk the world axes, named northeast/southeast/southwest/northwest
  (compass or clock per `direction-style`), the place is the third along each axis ("northeast
  edge", "south corner"), and the places the scanner has logged in the level follow, nearest first,
  with distance and direction. Rejected: a second gesture for the landmarks (the tester asked for
  one readout, as the automap is one glance).
- **Mercenary out of BACK+X** (`mercenary-report`, off): BACK+RT says it, now with its level
  ("Mercenary life: 80 percent, Level 12, 6 units, north"), what BACK+X used to say.
- **Skill tree branch read from the tree itself**: the tab whose skill container is visible, so
  LT/RT with the cursor on no node is read too. Rejected (the tester's own caution): snapping the
  cursor to a node.
- **Settings list**: every group says "... sub menu"; a **Settings list** group holds the list's own
  switches (`settings-list-stops`, new `settings-list-restart`, off: the default keeps opening where
  it was left, as before).
- **Usable corpse rattle repeats** while corpses stay near (`corpse-repeat`: once, or every 2, 3, 5
  or 8 s; 3 s by default), as a drop loop does; still a cue, not a voice of the item mixer, so it
  never takes a loot voice.
- **Audible loot filter**: LB alone in play flips it (the game's own item-label button, so the tap
  reaches the game too); a **Loot filter** group chooses what keeps looping (potions, gems and
  runes, equipment from a least quality up); silver items loop at every level but off; gold,
  scrolls and quest items always loop. Off by default, as the tester asked. The initial drop sound
  is the game's and untouched.
- **The Forgotten Tower's entrance is a quest place**: an exit to Tower Cellar Level 1 (Levels.txt
  Id 21, `EntityRadar::QuestDestinations`) gets the quest loop, the scanner's quest category and
  "a new quest object" wording, before and after the quest. **Exits are found from twice the scan
  range**, seen or not (`MapSurvey::ExitSightFactor`): the tower's door stands inside ruin walls.
- **Chat box**: the panel's own visibility says "Text chat" and "Text chat closed", and its InputBox
  is echoed when the game gives nothing focus with it. The tester's build said nothing on Enter; the
  log line `[field] chat panel shown focus=` will tell which path the game takes.
- **Captionless tabs are said by their number** ("Page 2"): the shared stash's pages are icons.
- **Locked waypoints**: a waypoint row without the layout's SelectableButton/CurrentButton image is
  "locked" (WaypointsPanelExpansionHD.json templates). Heuristic from the layout, not measured live.
- **Character select hints**: Start to create and X to delete, or "No characters. Press Start to
  create one" with an empty save folder.
- **Lobby browser**: list rows join the D-pad navigation nodes, so the game list can be entered.
- **Typing with the pad** (`pad-typing`, on; the user, 2026-10-15: "para quien quiera escribir con
  mando"): A on a loader text box that does not begin editing gets a mouse click (the keyboard
  mode's way in), and while editing the right stick picks characters sent as keystrokes. Rejected:
  writing the box's string directly (the game's own edit path keeps the echo and the cursor honest)
  and a mod-drawn keyboard grid (nothing to see; a list the stick walks is enough).
- **Not done, needs the tester's log or a controller session**: the main menu's left-from-list order
  (no mod code moves that focus; the game's own travelling cursor under the loader's layout does),
  and the crypt pentagram (`[scan] fire` lines).

## 2026-10-14 — The tester's eighth list (after a Countess run)

- **BACK+RT steps through the mercenary, the summons and the party members**, one per press,
  with life and where each stands (distance and direction, or the area a party member is in);
  after a 10 s pause it starts again at the mercenary. The tester found the summons too long for
  BACK+X, so `minion-report` (BACK+X) is now **off** by default; a Necromancer's usable corpse
  count stays in BACK+X. Party members come from the party roster the scanner already reads.
- **BACK+LT reads the main skill bar**, as holding LT reads the other. The pad filter keeps a
  trigger pulled with BACK from the game during play, as it does the face buttons.
- **Party panel** on the right stick, as the character sheet (`src/party_reader.cpp`).
- **Mercenary sheet**: with an item focused on it or in the backpack beside it, right stick
  down and left keep walking the mercenary's statistics; up (the item's tooltip) and right stay
  the item's. Down read the control hints there; that is the hint switch's asked-for gesture,
  not a fault of the switch.
- **Chat**: the chat's autocomplete box is a text box like any other; "Text chat" as it opens
  and "Text chat closed" as it shuts, typed text echoed as in the name box.
- **Town exits from anywhere in town**, with the town NPCs (`town-npcs-everywhere`, its phrase
  now says "NPCs and exits"): a fresh game's empty automap hid the camp's exit from a player who
  had not walked near it. Open: a joining client may not have the next area built at all; its
  log (`[areas] level 1, N built`) would tell.
- **Potions last** (`potions-last`, on): when more items lie near than there are voices, health
  and mana potions sound only in what the rest leaves. Rejected: more voices.
- **Burning bodies**: a body with one of the game's fires (objects.txt Name "fire") within 3
  subtiles is "Burning Dead Rogue". The pentagram is no object the game names; every fire is
  logged once (`[scan] fire id=`) so a crypt can show what it is before anything is named.

## 2026-10-14 — The tester's sixth and seventh lists

Two lists (builds 2026-10-13 and 2026-10-14), worked through while the user was away; the
optional suggestions are switches, on unless they reverse a choice of the user's (the 2026-10-11 rule).

- **Settings list**: the Diablo II drop sounds are a group inside the sound glossary (the list
  takes groups within groups). At either end a step goes round with a click (the old wrap, now
  heard) or, with `settings-list-stops`, stays and knocks and says the entry again. Every percent
  setting moves in quarters: volumes 0-200, footsteps 25-300, drop sound speed 50-300; a saved
  level of the old grids keeps its percent (`quarter-steps` in the settings file).
- **BACK+A and BACK+down trade places** with `pad-route-on-a` (off: the user placed them).
- **Nothing to spend**: the character sheet and the skill tree say "0 Stat Points Remaining" /
  "0 Skill Choices Remaining" in the game's own words on opening (`zero-points`, on). Rejected:
  inferring zero from an empty line at once; the line must stay empty for 2 s.
- **Stash**: the stash grids are now read while the stash is open, so Y on a gem in a stash grid
  says the tab it went to, a backpack item Y sends into a stash is no longer taken for one sorted
  into a tab, and an item sent straight from the backpack says "moved to the personal/shared
  stash" (`stash-notice`, on; not for one placed from the hand, which has its own notice).
- **Harmful states** (poison, freeze, chill, stun, slowing, curses) interrupt as they begin; their
  end waits 1 s, so a chill renewed by each hit is one chill (`status-alerts`, on).
  `docs/status-effects.md`.
- **Hire window**: Y says the skills' whole descriptions, autodetail or not: the names alone
  already followed the row, so Y added nothing. Rejected: waiting for the game's own tooltips,
  which the icons already hold.
- **Quest objects** kept from an earlier game (Cairn Stones, the Moldy Tome) are listed again, as
  the automap still shows them; a loaded object on the same spot replaces the kept one.
- **Release ZIP keeps its fixed name** (`D2RAccess.zip`; `d2r-access-completo.zip` until the public
  release, 2026-11-02): the tester asked for the version in the name, the user kept the always-latest
  download link instead.
- **Summons** (the game's pet list, `docs/summons-and-corpses-re.md`): every player's pets are
  neither NPCs nor enemies any more (the flag the mod used marks only the hireling; a Warlock's
  bound demon was listed as an enemy). Chosen: left out, not a scanner category of their own (the
  tester: there is no need to target one's own summon). BACK+X ends with them as the party
  portraits group them: a count per counted type, life for each single one, the Warlock's demons
  one by one, numbered where a name repeats (`minion-report`; off since the eighth list). Names are the monsters' own
  ("Spirit Wolf"), singular: the game has no plural strings, so "Spirit Wolf: 2".
- **A Necromancer's corpses**: a short bone rattle toward the nearest when usable corpses come
  within the cue radius after none were (`corpse-cue`, on), and "Usable corpses nearby: N" in
  BACK+X. Usable is the game's own core rule (dead, corpseSel, no udead state). Rejected: a cue
  per corpse (a pack's death would be a burst), and a loop per corpse like the ground items.
  Only the Necromancer: a Barbarian's Find Item has a rule of its own, not established.

## 2026-10-14 — Passages by the map's sides, on by default (a tester, the user)

A tester asked for the finder's input turned 45 degrees clockwise (flick up-right for the passages at
2 o'clock) and for the finder and the room line to say the room's walls: northeast, southeast,
southwest, northwest (2, 4, 8, 10 o'clock). It reverses the 2026-09-25 scope "every other feature
keeps its screen directions"; the user chose it as the default, and the screen sides stay a switch
away (`passage-map-sides` off). Chosen: a passage named by its wall, not by its bearing from the player, so it does not
change side as the player moves through the room; the stick read by its quarter, not its dominant
axis, so a diagonal flick is not split between two sides. Not changed: the scanner, the radar and
every other readout keep their eight compass points or twelve hours. `docs/rooms.md`.

## 2026-10-14 — Exploration is room by room, and what the scanner lists can be routed to (the user)

A route to the unexplored room next to a player failed: the way was planned to the room's centre,
which can be wall. The user's rules:

- **A room is explored on stepping into it**, anywhere, not at its centre: a sighted player sees
  the room whole on arriving. A room target (explored or not) is reached at its first step inside.
  The scanner lists every landmark in an explored room (stood in, drawn on the automap or
  underfoot), seen or not; enemies and loot in the room underfoot too. Before, a landmark needed
  range and a clear line of sight unless the automap had drawn its room.
- **What the scanner lists can be routed to.** The target's own room is always admitted. A way
  over explored rooms is still preferred; when there is none, the way guesses across rooms never
  stood in as ground not seen yet (the route says so), and the first such room is followed into
  as the target's own is. Rejected: keeping the refusal ("No route through explored areas") for
  a listed target, which is what the tester hit for Stony Field. Exploration still refuses a target
  no known or guessable way reaches.
- **Turn prompts** for a route asked for: the next leg at each turn, "Off the route" once if the
  player leaves it. A switch (`route-prompts`), on: the user asked for the getting-lost to be
  improved; nothing is said for a route never asked for, and still no autowalk.
  `docs/route-description.md#following-it`.

## 2026-10-13 — Compass words, units and the route description (the user)

- **Compass points instead of up/down/left/right**, with **clock hours** as a setting
  (`direction-style`, compass by default). They stay screen directions: north is the top of the
  screen. Rejected: the game world's own axes as north (the world is drawn turned 45 degrees,
  so "north" would not be where the stick pushes). The clock bearing has twelve hours, finer
  than the eight points; a side (radar ray, finder, overview) is 12, 9, 3 or 6.
- **Units instead of steps**: a step of the character covers several subtiles, so "steps" was
  wrong; a unit is one collision subtile, the measure every readout already used.
- **Route description** on request (BACK + right stick down, Ctrl+E): the way to the beacon's
  or scanner's target as straight legs with doors. This reverses the 2026-09-20 rule against a
  dictated step sequence: a tester could not plan the way to quest places as a sighted player
  does from the minimap. The user's limit: **no autowalk**; nothing is said unasked, and it plans
  by the navigation mode's rules (explored ground only in exploration). One level only.
  `docs/route-description.md`.

## 2026-10-13 — The tester's fourth report

Sixteen items. The user's calls: the layered enemy sounds are the default, the swoops stay a
switch (`enemy-class-sounds`); the settings groups navigate as the tester proposed; the pickup
click leaves the glossary and the list until the user's own clicks per item type exist (the
2026-10-12 note: items are picked up by walking over them, when the reader already names them).
Chosen:

- **Enemy classes by timbre, not pitch**: one growl register for every enemy, a layer per class,
  elite and boss apart (`docs/entity-cues.md`). Noise was avoided for the elite's "teeth": the
  user found an earlier enemy cue too like the radar's noise breath.
- **Settings in groups**: right (Space) opens a group or changes, left (Backspace) returns. A
  setting of several values wraps when raised, so it is never lowered directly; the strengths
  have nine steps (0-400 percent, wider above 100) to keep the cycle short.
- **Shrine and well**: the shrine takes the old well sound, the well the game's own drink sound.
- **The NPC "!"** is read from the game (overlay 72), not guessed from quest state; it exists only
  within 15 subtiles, so the scanner can ding only for an NPC the player has come near.
- **Footstep volume** scales the game's own footfall instance, not synthesized steps: the steps
  keep their floor type and timing. Above the game's own value it may not grow (0-255 clamp).
- Experience in the life report, the mercenary's experience as a percent, "Mercenary says:",
  and the NPC bell are switches, on (optional suggestions, the user's 2026-10-11 rule).

Follow-up the same day: the game's own drop sounds as an alternative item set, with a speed
setting (`docs/ground-items.md#the-games-own-drop-sounds-2026-10-13`). The user asked for sets
to choose from: Diablo IV (default), Last Epoch gold and potions, Diablo II drops, one setting.
2026-11-02: the user made Diablo II drops the default after playing with it; the saved value
keeps its meaning, so a player who picked Diablo IV keeps it.
Volumes in four families (radar and walls, enemies and NPCs, items and places, guidance),
0-200 percent over each config level, scaling the finished samples so the synthesizers stay
as tested (`src/sound_volume.h`). A setting of several values could not be lowered on the pad:
the user chose LB/RB, taken from the game only while the list is open (Shift+Space lowers on
the keyboard). Rejected: wrapping a volume from 200 to silence; the D-pad (the belt). Quest items keep their reach and priority but sound as they land. The
user wanted them in the glossary: a group of their own, each named after an item that makes it
with the game's own localized name, so 45 names needed no translation.

Not done: the alternate potion/gold sounds already had a switch ("Potion and gold sounds", now
in the Sounds group); nothing was found that would keep the old sounds once it is on.

## 2026-10-12 — The tester's third report: what changed and what was left

A thirty-item list, read with the user's notes. The user's calls: Charsi is said as the game
spells it; the pickup click per item type waits; doors sound only while shut. Chosen:

- **Wall tones**, Grimdark's and the Last Epoch mod's continuous tone per direction, as an
  optional companion to the radar (`wall-tones`, off by default). The tester asked for a tone
  while pushing a wall instead of a thud with a pause; the user asked for the whole system. The
  2026-09-16 rejection of a continuous bed stands for the default.
- **Off the map is a wall** for the bump and the tones: the game reads a cell outside every room
  as 0x27, WALL included. The radar keeps "unknown is never a wall" (2026-09-16 precision rule).
- **Walking/Running is said only for an L3 press.** The game walks by itself against a wall and
  out of stamina (`forceWalk*` in controllerskillsettings), which the mod announced as a toggle.
- **BACK is kept from the game in play** (`pad-back-to-mod`, on): the game only toggled its
  automap with it. Rejected: injecting the automap on LT+RT, whose triggers are the game's.
- **The beacon follows walkable ground** or stays silent; the straight-line fallback that pointed
  through walls is gone (`docs/beacon.md#walkable-guidance`). The scanner keeps straight-line
  distances (2026-09-24), now always nearest first, and a flick after 10 s starts at the nearest.
- **Settings of several values** in the Ctrl+S list: mana alerts (every change, low only, off)
  and the pan and pitch strengths, saved like the switches.
- **Gold and potions from the Last Epoch mod** as a switch (`last-epoch-sounds`, off): the
  project's own sounds stay Diablo IV's, the user's rule; one glass sound per potion kind.
- **Shrines and wells loop** (`shrine-sounds`, on) with Diablo IV placeholders the user may swap.
- Radar cues +3 dB and the door's knock +3 dB more; the version is said at start.

Not done: entrance hints for Act 1 cottages; an update check (the repository is private, so an
unsigned request cannot see its releases).

Follow-up: in a multiplayer game the host heard the tabs past Party misnamed and "Character"
never said. Her log showed correct captions: the tab reader, held while the quest log (a
dialogue) was open, woke after Character opened with the bar still on the quest log and said
"Quest Log" over the window's "Character". The switcher's selection seen during the hold is now
adopted silently when it is still there afterwards. Rejected: trusting any panel opened within
500 ms over the caption, which would also swallow a fast Character-to-Inventory move.

## 2026-10-12 — "Silver" for white names the game draws gray

A socketed or ethereal normal, low quality or superior item has a gray name on screen and the
tester heard no colour for it, unlike magic and better. The word is "silver" only (never
socketed or ethereal, which would spoil), on the ground and in the inventory headline, behind
the `silver-items` switch (Ctrl+S, default on). Rule and evidence: `docs/silver-items.md`.
The tome count was lost when the reader rebuilt the headline; it now follows the name, and with
autodetail the size still follows it (the tester only remarked on the size, kept).

## 2026-10-12 — The settings list on the pad: BACK+RB and the right stick

The new tester plays with a pad only, and the settings list (with the sound glossary) was
keyboard-only. BACK+RB opens and shuts it in play; RB with BACK is kept from the game like LB.
While it is open the right stick steps it (up, down) and changes the entry (left or right),
claimed before the scanner and the menu readers. Rejected: A to change (it attacks in play) and
the D-pad (the game's). In a menu BACK+RB only shuts the list, since RB there is the game's.

## 2026-10-11 — A new tester's report: what changed and what stays the user's choice

A new collaborator (controller, bow and melee characters, English game) sent a bug and
suggestion list. The user asked to fix the urgent items and to put optional ones behind switches.

- **Beacon on BACK+LB.** L3 is the game's walk/run toggle, so every beacon press also toggled
  running (and running drops defense to zero). Rejected: BACK+L3, both on the left thumb; R3
  stays the game's weapon swap. LB with BACK is kept from the game like the face buttons.
- **English words.** The scanner's "ahead/behind" were screen up/down, unlike the radar's
  "Up/Down"; English now says up/down everywhere. Openings between rooms are "passages"; "exit"
  is left for ways out of an area. Only the English table changed.
- **The weapon set number and town NPCs listed unseen are switches, on by default.** First
  shipped off, keeping the user's 2026-09-27 and 2026-09-28 choices; the user turned both on
  by default on 2026-10-12 (`weapon-set-number`, `town-npcs-everywhere`).
- **Remembered enemies follow their unit and are dropped when the game holds them dead**,
  keeping the 2026-09-28 "everything discovered stays" rule without phantoms.
- **Growls on screen.** Enemies growl from the screen's edge (plus a margin for ranged ones), six
  voices by default; range, voices, pan and pitch strength are settings in `d2r-access.toml`.

- **Sound glossary in the Ctrl+S list**, after the switches, rather than a new key: the keyboard
  already has too many (2026-10-03). The pad reaches both with BACK+RB since 2026-10-12.

Not done: a precision targeting mode for the pad (the keyboard has Tab/Shift+Tab).

## 2026-10-03 — The collaborator's controller and keyboard schemes are adopted

Integrating bladestorm360's branch, the user chose his controller scheme: BACK is a modifier
(BACK+X life and mana, BACK+Y/A/B room, exit finder and terrain overview). Compare, legends and
the hire list are read on demand, and tooltips have no delay anywhere. The user "always wanted
BACK to be a modifier". This supersedes the 2026-09-23 reservation of View/Back for a settings
menu. The user also took his keyboard keys (Ctrl+G radar, Ctrl+R retreat, Ctrl+U reach filter,
Ctrl+V exit finder). Rejected: keeping main's pad behavior beside his under a switch.
The keyboard is judged to have too many keys (~60). Planned: one shortcut menu listing every mod
action, rebindable or unbound, reachable from the pad too; toggles and duplicates unbound by
default. Main's features he had dropped were restored (STATUS, 2026-10-03).

## 2026-10-02 — Ranged attacks name every monster the auto-target picks

The user, on a sorceress: enemy names were almost never spoken in combat. Of 16 names the plate
showed in sight, 11 were held back as beyond the 15-subtile ranged reach, while Glacial Spike was
killing them from 20–23. Chosen: the reach rule applies to melee only; a ranged attack names
whatever the game targets. The repeat and wall rules stay. Rejected: deriving each spell's missile
range from missiles.txt. That is unverified, and the game's own targeting already bounds it.
`docs/attack-reach.md`.

## 2026-10-02 — Corridors from the walkable grid, measured by the level's own doorways

The user asked for what the HD layouts cannot name: corridors, bends, junctions, dead ends. Chosen:
read them from the live collision grid, in maze levels, with every measure scaled by the maze's
median doorway width. Rejected: one fixed width, because measured corridors run from 6 (town
streets) to 20 subtiles (Catacombs). Rejected: corridors on open ground and in towns, where the
room partition gives no doorway width to measure by. `docs/places.md#corridors`.

## 2026-10-02 — Named places inside towns and buildings, from the HD layouts

The user asked again for tavern- and palace-style names, then for detail inside them (the hall, the
counter). This reverses the 2026-09-28 "no district names" entry: the game does name them. Every
preset's HD layout marks interiors with acoustic and lighting volumes (`TOWN_2_BAR` is Atma's
tavern) and names its props (`barstool`, `table_kitchen`, `anvil`). The room → LvlPrest map link
was reversed too. Chosen: speak the named piece, the interior and the landmark nearest by
walking distance, generated from the game's files, never drawn by hand. Rejected: hand-drawn zones
over map images, which would not survive a patch and would cover only what we drew. Rejected:
straight-line nearest props, which name a room through its wall. `docs/places.md`.

## 2026-09-28 — The radar breath rises up and falls down, as in the D4 mod

The user could not tell an opening ahead from one behind even with the darker downward breath, and
preferred the D4 mod's breath. Chosen: copy it unchanged — a band-passed noise sweep that rises for
up, falls for down and holds for the sides, 180 ms long. Rejected: darkening the downward breath
further, which already failed once. `docs/radar-readings.md`.

## 2026-09-28 — The last death place is kept per map and routed to

The user died to a boss, pressed *Save and exit*, and had to find the boss room blind: the new game
moves the corpse to town. The mod now keeps the place, per map, on disk, and the corpse category
leads there over the exits already found. The level-to-level route uses only the exploration
memory, not `Levels.txt` `Vis`. That column holds the warp links only, not the open borders
between outdoor areas, and it would reveal ways never found. Waypoints are a fallback only: the mod
cannot read which ones are activated. `docs/death-place.md`.

## 2026-09-28 — Town NPCs listed from anywhere in town, no district names

The user found Lut Gholein too big to find the NPCs and asked for tavern/smithy-style places and an
unlimited NPC range, **only in town**; outside, the scanner stays as it was. The game names no
district: each town is one Levels.txt level built from one LvlPrest map (`docs/map-landmarks.md`),
so "taberna" or "herrería" would be names the mod made up, and were turned down. The NPCs who run
those places are the real landmarks. In towns the scanner now lists every NPC the level's presets
place once, from anywhere, once it has been seen. It shows the loaded unit if there is one, else its
spawn point. The user then asked that NPCs not yet seen stay out. "Seen" is the scanner's
exploration rule: in range and in sight, or standing in a room the automap has drawn. Full
assistance lists them all, as it does rooms. Rows placed more than once, such as guards, stay as
before. `docs/entity-scan.md#town-npcs`.

## 2026-09-26 — Loader 1.3.1-beta adopted, PluginSDK kept at 0.2.0

The user asked to move to the new loader without breaking the mod. 1.3.1-beta keeps plugin ABI 4,
and the unchanged plugin passes every layout check on it (offline and live, `STATUS.md`), so the
plugin was not rebuilt. SDK 0.3.0 was turned down for now: it raises `LifecycleServiceRequiredSize`
from 40 to 56 bytes, so the 13 services that check it would refuse to start on a 1.3.0 loader,
which the earlier friends' packages carry. It adds nothing the mod uses. Its one breaking change,
`DiagnosticsService::queryHookStatus` removed, does not touch the mod either. Revisit when a
feature needs the new services (mutations, overlays, plugin communication, `getInputText`).

## 2026-09-25 — Radar back on the stick cardinals after listening

The user tried the diagonal rays and went back: every ray sounds from one side,
so walls and exits "sounded up and down" at once and read as an overload of cues.
The cardinals are enough; what matters is that passing an exit sounds, which the
cardinals do. Default `[radar] diagonals = false`; the diagonal frame stays
selectable (`access-radar diagonal`). Known limit kept: a corridor along a world
axis can read as wall in all four directions. `docs/radar-readings.md`.

## 2026-09-25 — Radar rays along the walls: the stick diagonals

The user found that a corridor could read as wall in every direction and asked to
fix it, after being offered rays aligned with the walls. D2's walls run along the
world axes, at 45 degrees to the four stick cardinals chosen on 2026-09-16, so in a
world-axis corridor every cardinal ray meets a wall. The game rotates the stick by
45 degrees into the world (41 logged stick/velocity pairs: median error 6 degrees,
against 14.5 for the screen projection), so the stick's diagonals are the world
axes. Chosen: the radar's four rays follow the stick diagonals by default, still
four rays with the same sounds, panned to their side, the lower two at down's pitch.
Kept selectable: `[radar] diagonals = false` or `access-radar cardinal`. Not
revisited: eight rays (rejected 2026-09-16). Every other feature keeps its screen
directions. `docs/radar-readings.md`.

## 2026-09-25 — L3 is the only stick button; R3 back to the game

The user removed the R3 gestures, then the L3+R3 chord too: the scanner already
covers the exit review ("next way out") and the terrain overview. Kept: L3 (the
beacon). The exit review stays on Ctrl+V and the overview on Ctrl+O, since only
the controller gestures were asked about. With no R3 gesture left, the pad
filter that hid R3 from the game (`PadFilter`, added 2026-09-21 so the beacon
click would not swap weapons) had no purpose and was removed with the chord
logic (`StickChord`), so R3 does what the game binds it to. L3 is now reported
on press, since no chord has to be told apart. Recoverable from git history.

## 2026-09-25 — Beacon on L3, one-press retarget; darker downward breath only

The user moved the beacon from R3 to L3 because R3 opens the emote wheel, and
asked that L3 on a newly scanned target switch the lock in one press, while L3
on the same target cancels. Chosen: a retarget checks the new target before
releasing the old one, so a refused target (no known route) keeps the current
lock. A short R3 click is now unbound; the R3 hold and the L3+R3 chord stay.
For the radar, only the breath (the "wind") got a larger downward drop
(`BreathDownPitch` 0.55); the tin, tock and knock keep `DownPitch` 0.86, the
sounds the user chose on 2026-09-16. Rejected: widening `DownPitch` for every
cue. `docs/beacon.md`, `docs/radar-readings.md`.

## 2026-09-24 — Fixed places found on a map stay known on that map

The user asked for exits already discovered on a map or dungeon to be readable
from far away, only on that same map: a map regenerated after a game shows its
exits only once found again. Then: waypoints too, and anything fixed, but not
objects, which change. Chosen: exits, waypoints and the stash
(`MapSurvey::Fixed`) the scanner counts as discovered go into the
explored-rooms archive under the same key (map seed, level, room-layout
digest), and the scanner lists them in later games. The key already tells a
regenerated map apart, online or offline. Rejected: a separate file (a second
store for the same key); recording what only full assistance listed (never
discovered); shrines, chests, quest objects and portals, whose type, state or
presence changes between games. `docs/exploration.md#on-disk`.

## 2026-09-24 — Unexplored rooms at the edge of the map, in the scanner

The user, after playing: keep the radar, but list unexplored rooms even when unloaded,
without revealing exits, so a lost player can head for new ground. Chosen: a separate
*unexplored room* category listing only rooms near explored ground (`RoomGraph::Near`). The
beacon may enter such a room and arrives on entry. Rejected: every unexplored room of the
level, because it would lay out the whole level's shape and point toward its far end.
`docs/entity-scan.md`.

## 2026-09-24 — Exploration follows the automap's drawn rooms

Live sessions confirmed the native automap reader (`docs/automap-reader.md`).
The automap is saved with the character, but its object glyphs are not. So
"explored" is room-level: a logical room with any drawn floor or wall cell, or
one the player stood in. The scanner lists landmarks in explored rooms. The
beacon routes through them. This is less exact than matching each glyph, but
it survives a reload and needs no per-object reveal rule.

## 2026-09-24 — One scanner on the right stick, following the automap

The user asked the scanner to follow the automap, with no L3 mode. Chosen: the
right stick always drives the catalogue (up/down categories, left/right
objects). Enemies stay as a live nearest-first category. Ground loot and
ordinary chests leave the scanner: they are not on the automap, and their loops
already cover them. The directional "what is to my left" scan was removed, not
moved to another gesture (user's choice). Until the native automap reveal state
is readable, discovery keeps the range-and-sight approximation. `docs/entity-scan.md`.

## 2026-09-24 — Pathfinder: exploration first, full assistance opt-in

The user felt the pathfinder led the player by the hand and wanted to encourage
exploration. Chosen: limit what it can solve, not how often. The catalogue only
orients (straight-line distance and direction to what was discovered), and the
beacon only routes through visited rooms, so it helps you return rather than
discover. Full assistance stays as an option, off by default (the user's choice).
Rejected: removing the pathfinder; a cooldown (one use still skips the
exploration, and when lost it only makes you wait). `docs/beacon.md#exploration-mode`.

## 2026-09-23 — Potion assignments in inventory; View reserved for settings

The user chose belt consultation through inventory focus, including the assigned
D-pad direction. Do not add a View/Back hold for potion review. The user plans a
future mod settings menu on View/Back and wants to move keyboard shortcuts there.
This task does not implement that menu or remove existing bindings. `docs/belt.md`.

## 2026-09-21 — Unexplored as a qualifier on the radar, not a new sound or a guide

The user asked for a sound-first way to find unexplored ground without spam. Chosen: a short tail
on the existing exit/floor cues when the way leads somewhere never covered, so no new event is ever
added; the meaning is per level type (chamber never stood in, level never seen, frontier of the
streamed ground on open levels). Plus tags in the exit review and room line, a percent in the
terrain overview, and the two memories (seen/visited) persisted per map seed like the game's
`.ma0`. Rejected: a continuous tone or bed (the radar already sounds every step), an automatic
beacon to the nearest frontier (too close to the routed navigation turned down on 09-20), a
sonified four-direction sweep (repeats what Ctrl+O says). `docs/exploration.md`.

## 2026-09-21 — Terrain overview instead of level announcements

Do not announce undiscovered levels — it breaks immersion. Approved instead: an accessible
automap-equivalent "terrain overview" (Ctrl+O / BACK+B on `bladestorm`, R3 double-click upstream). See `docs/terrain-overview.md`.

## 2026-09-20 — Navigation must be organic, not routed

Navigation follows the Grim Dawn mod (`grimdark`) model: the player learns where they are and its
exits (room name, exit label, distance, clock bearing, reachability), never a dictated step
sequence ("up 6, right 4"). Rejected: a "route to nearest unexplored space" feature (PR #3);
removed from the merge, kept alive on branch `feature/sight-explore-containers` if ever wanted.
Design and ported behavior: `docs/rooms.md`, `docs/beacon.md`,
`reference/audio-navigation/grim-dawn/navigation-spec.md` §8.

## 2026-09-20 — Radar precision is the priority

The radar (wall/opening cues) is what the user lives with every step; it comes first. Navigation-layer
features (exit review, beacon) are separate, explicitly requested work — do not fold them into a
"polish the radar" session unasked.

## 2026-09-16 — Radar cue redesign: kept 4 cardinals and the original sound, fixed thresholds

User's rule: *"si hay pared suena pared, si hay salida suena salida, y ya; solo avísame de los
cambios; da igual si camino rápido o lento."* Rejected after listening: eight directions ("mucha
información, con 4 ya bastaba"), a new sound palette with pitch glides ("los de antes me parecían
mucho mejor"), and speed-scaled warning distance. Kept: four screen cardinals, the original 120 ms
tin/tock cues (1100/260 Hz, Down 14% lower, pan ±0.9, level 0.15), fixed distance thresholds
(open 10 / close 7 cells after the 18:10 forensic fix). Full rationale and parameters:
`docs/radar-redesign.md`.

## 2026-09-16 — Quest tracking: automatic, no hotkey

Rejected an on-demand "read my current objective" hotkey. Required instead: (1) the quest panel
navigable/readable when opened, and (2) the new objective spoken automatically whenever the game
shows the change visually (alert, kill counter, step completed) — parity with what a sighted
player perceives passively. Implemented behavior: `docs/quests.md` ("Automatic announcements").

## 2026-09-14/15 — Core architecture

D2RLoader plugin + PRISM in-process, speech on its own thread (PRISM/NVDA `speak` blocks
300-800 ms), native widget text read via D2RCore's slot table, `d2rloader.toml` must set
`text_locale = "esES"`, and `D2R.exe`'s `.text` is Arxan-encrypted on disk so the plugin dumps live
code instead of relying on static RE. Already recorded in detail in `STATUS.md` (Hard-won facts)
and `AGENTS.md` (Rules / Hard-won facts) — this entry only marks that those were 2026-09-14/15
decisions, not new information.
