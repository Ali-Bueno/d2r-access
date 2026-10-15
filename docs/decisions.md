# Decisions log

Dated log of design decisions and explicitly rejected alternatives, newest first. Short entries —
the *how* lives in the linked docs, this page only records the *why* and what was turned down.

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
- **Release ZIP keeps its fixed name** (`d2r-access-completo.zip`): the tester asked for the version
  in the name, the user kept the always-latest download link instead.
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
