# d2r-access — screen reader accessibility for Diablo II: Resurrected

A [D2RLoader](https://d2rloader.net) plugin that makes Diablo II: Resurrected playable with a
screen reader (NVDA, JAWS, SAPI…) through [PRISM](https://github.com/ethindp/prism). Built for
blind players using a controller; keyboard and mouse are planned.

## What works today

- Screen changes and panels are announced with the game's own localized titles (title screen,
  main menu, character select/create, options tabs, cinematics, calibration, modals).
- **Controller focus is read on every menu**: the focused button, checkbox, dropdown, carousel,
  slider or text field is spoken with its label, its role and the game's own tooltip text.
  Catalog controls now resolve from native focus identity first, fixing silent main-menu
  actions after returning from gameplay when cached rectangles no longer match.
- Slider values are spoken as a percentage, on focus and while changing them.
- Checkbox and carousel states are tracked from the game's `Settings.json` plus the player's input;
  dropdown popups read the highlighted option.
- Character class names while creating a character.
- Native text in dropdown rows, character rows and confirmation prompts. Character
  selection reads the name and level/class from the selected runtime row, even
  when the game's focus remains on the first row.
- D2RLoader multiplayer controls use native focus for host, browser and direct-join
  fields, buttons, checkboxes and overlay context. Short D-pad presses move between
  the form controls. Hosting status and empty browser
  results are announced when they change. See [playing with a friend](docs/multiplayer.md)
  for setup and the current validation limits.
- Everything is spoken in the game's text language: the plugin's own phrases are translated into all thirteen D2R locales and follow the language selected in the game's options (changing it in Options switches the plugin too).

Implemented, awaiting live NVDA/controller verification: in-game NPC menus and topic
lists, full scrolling dialogues, tutorial/automatic hints and area-name banners. The
reader follows native controller focus and visible text, and suppresses repeated
announcements. With the game's *NPC Speech* sound option on *Audio Only*, dialogue
text is not read: the voice carries it. See [the controller test guide](docs/test-ingame-text.md).

The user confirmed the Rogue Encampment entry announcement. Informational notices
(including the Classic/Expansion restriction when pressing **G**) and the visible
HUD notification feed now have a reader. The user confirmed that the G warning
also reads on subsequent presses. New notices queue
without interrupting a conversation. A new delivery of identical text is read
again; simply leaving a notice visible stays silent. Notification text with links
uses its native embedded textbox.

The user confirmed the stash and Charsi target labels, basic inventory
selection, smooth inventory navigation, and axe descriptions in the blacksmith's shop. The reader
also exposes inventory coordinates and selected stash tabs. Item grids and equipment
announce one name and its quality, such as `Crimson Spear. magic`, or
`Spear. Unidentified`. Magic, set, rare, unique and crafted items use the game's
full tooltip name. The stock 400 ms controller delay is removed while the inventory
reader is active; ordinary items still read directly from the highlighted item.
Prices and properties are available with right-stick up, or automatically with
Ctrl+D. Activating the game's **Compare** action reads the selected item's full
visible description followed by **Currently Equipped** and the displayed equipment
statistics. Right-stick up repeats the comparison while it is visible; closing
the comparison restores ordinary item detail. Unidentified items expose only the text
the game renders: basic damage/defense and requirements, with magic properties
remaining hidden until identification. Empty grid cells retain coordinate feedback. See
[native tooltip evidence](docs/native-tooltips.md).

The mercenary panel uses the shared stat-row reader. When the controller leaves
that panel without a focused widget, the right stick reads its visible statistics
one row at a time, as on the character panel. Equipment focus retains item detail.
The advanced statistics panel also reads its live rows for either character:
right-stick up/down moves between rows, and left/right reads the first/last row.
Focusless inventory transitions no longer read every label, gold amount or
controller-font symbol as a modal message.

Hiring rows retain their candidate text and read the game's current controller
actions. When the game renders skill descriptions for a row, those descriptions
are announced and can be repeated with right-stick up. Disappearing skill tooltips
do not interrupt the full description by repeating its remaining fragment.
Closing them or changing focus clears the detail. Live logs confirm all three
Act I skills; the correction awaits listening validation. See
[hiring evidence](docs/character-panel.md#hiring-rows-2026-09-22).

Existing NPC menu rows read
without a population delay; inventory coordinates settle for one poll and
already rendered item descriptions read immediately. Inventory section controls
are discovered on entry and their selected values stay live, avoiding repeated
full inventory walks. Gameplay focus is sampled at 30 Hz.

The latest build also reads LT/RT (L2/R2) tab changes from the visible tab bars,
including the controller menu strip and stash tabs while selection stays in the
player inventory. Runtime logging now runs on a separate worker because the
loader forces a disk flush on every line. The user confirmed inventory responsiveness
after this change; tab announcements still await confirmation;
see [latency and tab evidence](docs/navigation-latency.md).

Character point-allocation buttons now announce **Strength, Dexterity, Vitality
or Energy**, their current value, remaining points and help in the game's language.
Allocating a point reads the changed value and remaining count. Skill trees now add
**D-pad grid navigation**: each press selects one visible skill, skips gaps and stops
at the boundary. Holding or changing direction without fully releasing the D-pad
does not repeat selection. Releasing cancels any pending native cursor snap and
restores free stick movement. Branch tabs are named, and each highlighted
skill reads its name first, followed by its row/column, position within the branch,
remaining points, current controller binding and available action buttons. Hold-to-use
instructions come from the game's own footer. The full description, requirements
and levels follow. Cursor selection now also refreshes native hover to prevent
stale descriptions. General/item-granted skills use the same
description reader. No points are spent by the mod. These additions passed
automated checks. The user confirmed the description/action-help correction with
the controller. All thirty skills in the inspected Barbarian
tree are reachable in the navigation tests. See [skill actions and coverage](docs/skill-actions.md).
See [progression evidence](docs/progression.md) and the
[controller navigation follow-up](docs/controller-navigation.md).
Use `access-skill-grid off` to restore stock skill navigation.

With a controller, Options has **Video, Audio and Gameplay**. The game only adds
the fourth **Controls** tab in keyboard mode. Controller cursor speed and
thumbstick/weapon-swap settings are further down in **Gameplay** (**Juego**).

Inventory icon buttons now use their native label/help or controller legend,
including **Soltar oro**, and equipment slots announce their body location
(head, neck, weapon, ring, belt, and so on), followed by any rendered item description.
The user confirmed these additions. The specific
scroll-related control reported by the user has not yet been identified live.

The inventory/NPC follow-up reads equipment slot names immediately, reports
confirmed empty/occupied slots even while the game has no item tooltip, and
rejects lingering descriptions after equipment is removed. Controller footer
actions now cover inventory, stash and NPC services as well as skills: repair,
shop/refresh and other available actions use the game's current button labels
and hold instructions. Unchanged actions are not repeated on every item; manual
text repeat includes them. No purchase, repair or quest service runs automatically.
UI memory permission checks are reused within each reading pass to reduce cursor
stalls. These changes are built and tested; live controller/NVDA validation remains
pending. See [inventory and NPC follow-up](docs/inventory-npc-followup.md).

The user confirmed improvement and prefers the game's equipment-placement
navigation. Picking up an inventory item now announces **Item in hand**, with
a short instruction to choose a space or compatible equipment slot. Leaving
that mode announces the placement destination when known. Brief focus gaps or
crossing another control do not repeat the pickup instruction. While carrying
an item, equipment destinations read immediately, including occupied slots
whose item tooltip is still pending. Manual repeat includes the held-item
context while in the inventory.

The **belt** now names the D-pad direction and row of each focused cell, its
potion, and the total items in that column. The bottom row is used first;
higher rows are reserves for the same button. Placement names the destination
button and row too. If the game displays an automatic potion from the inventory
in an empty bottom cell, the reader identifies it separately from physical belt
reserves. No new controller binding is added. See [potion assignments](docs/belt.md).

**Hold LT (L2) alone** for about half a second during play to hear the LT skill
bar, the six skills the game puts on LT + A, X, B, Y, RB and RT: each control,
the skill's name in the game's language and the count the HUD draws (scrolls in
a tome). Releasing LT stops the reading; an LT + button combo never triggers it.
`access-skill-bar on|off` toggles it. See [the controller skill bar](docs/skill-bar.md),
which also lists what every stock controller button does.

An experimental **navigation radar** now reads the collision grids of loaded
game rooms. It synthesizes four directional cues: a bright `tin` when a passage
leads somewhere (another chamber of a dungeon, or a level exit), a soft breath
for floor that leads nowhere new (rising upward, falling downward), a dull `tock` for a nearby wall and two
knocks for a closed door ([readings](docs/radar-readings.md)),
using fixed **up / left / right / down** directions that match the left stick;
down has a lower pitch. `[radar] diagonals = true` or `access-radar diagonal`
switches to the stick's diagonals, which the game turns into the directions its
walls and corridors run in; each is then panned to its side and the two lower
ones have a lower pitch. Simultaneous
cues play successively with **50 ms of
silence between them**. Exit readings include nearby transitions between outdoor
areas when the passage scan confirms an approach. The radar checks the full body along each passage using
the player's native movement mask. It also recognizes a complete, uniquely
identifiable player footprint displaced from the reported cell, preventing that
footprint from appearing as a nearby wall. Ambiguous shapes keep their blocking
markers. It separates creature occupancy from terrain and keeps the original
movement mask as the final opening check. With wall-slide prevention on (the
default), a direct move stops at stones, trees and furniture, so the radar
announces them like walls. With prevention off the game steers round them, so
isolated furniture and small clutter are excluded from automatic space changes
when a full-body check proves a route around them. Walls, doors, enclosed
spaces and objects cutting off a corridor remain structural barriers either way.
Uncertain cases retain their collision.

The radar follows **usable connected passages** and remembers a wall's position
when you move away from it. More free floor before the same wall stays silent;
an opening must prove a clear approach past that remembered edge. A nearer
observed barrier updates the reference, and an announced doorway can sound on
return after the intervening wall is confirmed. It also retains multiple
announced approaches per direction within the current area. An opening no
longer replaces an earlier one, and pauses or temporary reading gaps preserve
their positions. Returning requires fresh full-body clearance. Approaches join
one remembered region only when the whole connecting strip is clear; crossing
its other edge does not immediately repeat it. Staying at the same spot remains
silent. Local contour checks keep
small raster changes on an observed continuous wall from becoming exits. Moving along
the wall can still reveal a doorway or its end. Unknown coverage does not turn
an old wall into an exit. The four wall references supplement the local native
body graph; they are not a saved exploration map. Speed and queue length do not
create geometry transitions. Changes require two observations, with 120 ms cues
and 50 ms silent gaps; a cancelled cue retries only if it never started.

**Ctrl+R** toggles it; **Ctrl+T** first describes actual recent screen movement
(including diagonals), then the four straight-line clearances, and saves a local
map snapshot. This distinguishes moving along a wall from being stationary while
one or two straight rays are blocked. Short passages remain limited space;
recognizable actor obstruction is an occupied passage. It starts enabled,
baselines silently and pauses during dialogue/offline pause. Existing settings
are preserved; no external map or sound installation is required.

Live map reads work in the camp and Blood Moor. The user confirmed that object
filtering and connected passages helped. The latest correction retains known
walls during retreat, follows local raster contours and rearms return visits.
Cue events automatically capture local collision for diagnosis. This update is
built and installed; controller/listening validation is pending. See
[opening memory and current validation](docs/radar-opening-memory.md),
[short return visits](docs/radar-revisits.md),
[wall contours and sliding](docs/radar-sliding.md),
[remembered walls](docs/radar-wall-memory.md),
[connected passages and tests](docs/radar-connected-passages.md) and
[object filtering](docs/radar-object-filtering.md) for behavior and limits.
[Sonar controls](docs/sonar-radar.md) and
[cue delivery](docs/radar-state-delivery.md) document the rest.

**Movement without wall sliding** stops direct directional movement when the
game would otherwise choose another direction around a blocked target. Enabled
by default; `access-movement on|off|status` controls it for the current session.
Mouse routes retain their normal pathfinding. The hook is restricted to the
local player's directional path and checks the game code before installation.
Controller validation is pending; see [behavior and evidence](docs/wall-sliding.md).

An experimental **stalled-footstep filter** suppresses repeated local HD
footfalls when the character remains at exactly the same position while walking
or running. Any fractional movement restores footsteps, including sliding along
a wall. It starts enabled; **Ctrl+F** toggles it for comparison. The first blocked
footfall may still sound. Native code checks and automated tests pass; the new
filter awaits live listening validation. See [footstep behavior and limits](docs/footsteps.md).

Not yet: full inventory operation coverage, full chat history, general in-game HUD/combat,
full-map navigation and complete quest-objective coverage while the journal is unloaded.
Named landmarks inside towns are under investigation; see [map findings](docs/map-landmarks.md).
See `STATUS.md` for the live ledger.

**Ground items:** approaching loot reads its localized base name and quality
(gold also reads its amount). Nearby objects use the supplied continuous 3D
loops, with stereo direction, distance attenuation and slightly lower pitch
behind you. Orientation follows the screen: up is ahead, down is behind.
**Ctrl+I** toggles proximity speech and **Ctrl+L** toggles loops independently.
Loops start only after an item lands. Their level is normalized and was reduced
by about 3.5 dB following the user's listening feedback, including piles of items.
Sounds stop for pickup, dialogue, pause and leaving the game. See
[item sound categories, controls and validation](docs/ground-items.md).
The native reader and all six audio assets have initialized in a live game.

**Quest item cues:** `quest item.ogg` marks nearby ground quest items and unused
quest-item objects, including the Tree of Inifuss, the Malus, Lam Esen's Tome,
and the Horadric and Khalim component chests. Every unactivated Cairn Stone is
eligible for the cue. The scan, catalogue and beacon name its fixed symbol;
opening the deciphered Scroll of Inifuss reads the symbol sequence. Reopen it
to hear the sequence again, then choose which stone to touch yourself.
The cue repeats about every 700 ms,
uses screen direction and reaches 45 subtiles with a gentler distance fade.
Up to two quest sources get priority
over ordinary sounds. Pickup or successful activation silences it; **Ctrl+L** toggles
these cues together with item loops. Detection covers loaded nearby rooms;
it does not locate the tree across the entire Dark Wood or check active quest flags.

Navigation and loot cues use game state and bundled or synthesized sounds,
independently of the text and voice language. If a container or exit name cannot
be read, its cue remains available with a generic localized label. Beacon audio
is cancelled when guidance is suspended by dialogue, menus or loss of focus.
See [audio language verification and limits](docs/audio-language-audit.md).

**Level exits:** entrances that require interaction, including dungeon stairs,
cave entrances, portals and waypoints, play the `portal` loop; the stash plays
its own `stash` loop with them. Only crossings
traversed by walking use `zone transition`, at openings in the loaded collision map. The scanner
names exits after their destination. **Ctrl+P** toggles the exit loops;
`access-items exits-on|exits-off`. See [level exits](docs/level-exits.md).

**Unexplored rooms:** only the nearest unvisited dungeon room within audio range
plays `unexplored room` in a loop. Entering a new room plays `inside unexplored
room` once; revisiting it stays silent, including after reloading the saved map.
The two room sounds can be disabled with `[exploration] enabled = false` in the
plugin config. See [exploration](docs/exploration.md).

**Containers:** chests, caskets, urns, barrels, crates, corpses and the other
things you open for loot loop through the same mixer, so you can hear where they
are; dead bodies use the `corpse` loop and weapon racks the `weapon rack` loop
instead of `chest`. Opening one silences it. What counts as a container comes from the game's
own interaction handler, not from the name, so it holds in every language.
**Ctrl+K** toggles them; `access-items chests-on|chests-off`, and `chests-dump`
logs the verdict for everything currently in sight. See
[container loops](docs/ground-items.md#container-loops).
The user confirms that the ground-item loops are audible. The quieter level and landing-only
start await a follow-up listening check.

**Doors:** a shut door loops `wooden door`, `metal door`, `stone door` or
`slime door`, chosen by the sound the game itself plays when that door opens,
and falls silent once opened. Scrolls, runes and gems on the ground have their
own `scroll`, `rune` and `gem` loops; the Moldy Tome and Horazon's journal play
`scroll` until read. `[doors] enabled` in the plugin config and
`access-items doors-on|doors-off` switch the door loops. See
[door loops](docs/ground-items.md#door-loops).

**Scanner (right stick; live validation pending):** during play, push the
right stick up or down to change category, and left or right to step through
it, nearest first. It lists what a sighted player finds on the automap: exits,
portals, waypoints, NPCs, the stash, shrines, objects, quest containers,
visited rooms and, in their own category, the unexplored rooms sharing an
open edge with what you have explored. It also lists the enemies in sight right now and, in multiplayer,
the other players nearby and party members anywhere on the same level, by
name, walls or not. Exits, waypoints and the stash you have found stay listed
from anywhere on the level in later games too, as long as the map is the same;
a regenerated map starts with none. Ground loot and
ordinary chests are left to their sounds. Keyboard: Ctrl+Numpad 8/2 (category)
and 4/6 (object). See [scanner](docs/entity-scan.md).

**Target guidance (built; live validation pending):** click the left stick (L3) or
press **Ctrl+B** to follow what the scanner last named with directional pings.
After naming another target, one click switches to it; clicking again on the
same target cancels. See [distant exits](docs/distant-exits.md) and
[target guidance](docs/beacon.md).

**Exploration mode (default; live validation pending):** the scanner follows
the game's own automap. It lists landmarks in the parts of the level the
automap has drawn, including parts drawn in earlier games. It also lists
anything that has come into range with a clear line of sight, the rooms you
have stood in, and the unexplored rooms at the edge of explored ground, loaded
or not, but never what they contain. The beacon can lead into one of those
and arrives as you step in. It gives their straight-line distance and direction, not a
walking route. The beacon only follows routes through explored areas (drawn on
the automap or walked); otherwise it says there is no route through explored
areas and stays silent, and it stops if the route leaves them. Another player is
the exception: the beacon follows them through unexplored rooms too. See the
[automap reader](docs/automap-reader.md).
**Full assistance** restores routes through unexplored rooms and the whole
generated catalogue: add `full_assistance = true` under `[navigation]` in
`d2rloader/config/d2r-access.toml` and restart, or switch for the session with
`access-beacon full` (`access-beacon exploration` switches back). See
[exploration mode](docs/beacon.md#exploration-mode).

**Player corpses** have their own scanner category, after players: "your
corpse" and "X's corpse", from the game's ownership link. Once seen, a corpse
is listed from anywhere on its level for the rest of the game; a party
member's corpse is listed even unseen, from the party roster. Picking it up
removes it. Scenery corpses retain their ordinary container behavior. See
[corpse detection and limits](docs/player-corpse-re.md).
The beacon retries a failed room-graph read instead of keeping an empty graph
for the entire level.

**Quests (built; live validation pending):** quest buttons have names from the
game's string tables. Selecting a quest reads its current objective; changing
progress reads only updated text, including counters supplied by the game.
The current controller button and action are read with the quest, with the
visible dialogue-action caption as a fallback. Repeating the quest includes
these hints; progress updates do not repeat them. Classic-layout quest identities
cover the differently ordered icons in Acts I and III. Empty detail panes are
explicitly reported. Automatic alerts announce journal updates and primary
quest objectives achieved, without opening the journal. When updated objective
text is available and identifiable, the reader speaks the quest and objective.
A verified native capture also reads the text the game generates when a quest
packet updates its state, including actual counters, even if the journal panel
is unloaded. It observes the normal update without opening the journal or
changing quest state. Unmatched or ambiguous updates retain the generic alert.
Announcements wait for dialogue
or menus to close. Console `access-quests on|off|status` controls this feature.
See [quest-log implementation and remaining work](docs/quests.md).

**Room exits (built; live validation pending):** on maze levels (the Crypt, Catacombs, the Den of
Evil and the rest), walking into a room speaks its ways out — plain openings as a screen direction,
stairs and portals by their own name. **Ctrl+V** reviews them one at a time, nearest first, with distance, clock bearing and the beacon's verdict on
getting there (straight, around or no route). A short left-stick click, or **Ctrl+B** right after,
guides to the one just reviewed. **Ctrl+M** toggles the feature.
See [room exits](docs/rooms.md).

**Named places (built; live validation pending):** walking into a named part of the map speaks it,
without interrupting: a named map piece (*bridge*, *Fallen camp*, *graveyard*), an interior
(*tavern*, *palace entrance*, *hut*, *temple*) and the landmark within a few steps (*bar*,
*kitchen*, *fireplace*, *stairs*, *dock*, *stalls*, *throne*). Everything comes from the game's own
HD map layouts; the landmark is the nearest one by walking distance, so it never names a room
through its wall. In maze levels it also reads corridors from the walkable grid: *corridor*,
*dead-end corridor*, *bend*, *junction*, and what a corridor leads to (*towards* the stairs, the
throne). **Ctrl+U** toggles it; `access-places where` speaks the current place.
See [named places](docs/places.md).

**Terrain overview (built; live validation pending):** a spoken level overview. **Ctrl+O** speaks
the level name, where you stand in it (`left edge`, `centre`…) and,
for each screen direction, how far the level reaches, where unexplored terrain starts, or the name of
a neighbouring level from available native layout data. It can describe undiscovered terrain;
it does not calculate a route.
See [terrain overview](docs/terrain-overview.md).

**Unexplored mark (built; listening pass pending):** the radar's exit and floor cues end in two short
pips when the way leads to ground you have not covered: an opening into a chamber you have
never stood in, stairs to a level you have never seen, open ground towards the frontier of what the
game has streamed. The base cues are untouched and nothing new sounds on its own; the tail rides on
the cue that was going to play. The room entry line and the exit review tag each way out never
taken (`abertura, sin explorar`) and list those first; the terrain overview says how much of the
level is filled in (`explorado 22 por ciento`). What you have seen and where you have stood is kept
in one compact archive, limited to 4 MiB, with separate records per map seed and layout.
Existing exploration files are migrated automatically, so progress survives quitting. `access-radar
test` auditions the marked cues. See [exploration](docs/exploration.md).

### NPC and enemy sounds

Soft synthesized pulses locate the nearest NPC and up to three enemies: the
nearest boss/elite, resurrecting shaman and ordinary ranged enemy, then the
nearest other enemies in the streams left, each heard on its own (as Diablo 1's
diablo-access plays its three nearest monsters, so a pack's spread is audible). NPCs use a light wooden tap; priority enemies a low, resonant
metallic swoop; ordinary enemies a short rough growl. Sounds follow the radar's
fixed screen directions. A single target repeats about every 0.2 seconds nearby
or 0.5 seconds at maximum range. Dialogue and menu/focus gating suspend the sounds.

**Attack reach.** A bright rising two-note ping, placed toward the enemy, plays
when an enemy in sight comes within reach of your attacks, once until every
enemy is out of reach again. The reach follows the skills bound to the controller
bars and the weapon in hand (bow or crossbow, ranged skills, melee skills,
damaging spells), read from the game's own skill and weapon tables; ranged
reaches as far as the enemy sounds, melee is contact plus the weapon's
`rangeadder`. `attack-melee` / `attack-ranged` fix it by hand, `attack-auto`
returns to the automatic choice, and `status` says the current reach.

The monster health plate names a monster only once it is within that reach
(bosses and elites from first sight), without interrupting other speech, and a
name the auto-target keeps trading inside a pack is not repeated until it has
been off the plate for ten seconds.

Enabled by default; `access-entity-cues on|off|status`, with separate `npc-off`,
`enemy-off`, `ranged-off`, `ordinary-off` and `reach-off` switches (and matching
`*-on`). Persistent range, volume, switches and the attack reach live under
`[entity_cues]` in the plugin TOML. This is independent of the existing NPC
speech toggle. See [entity cue behavior and controls](docs/entity-cues.md) and
[attack reach](docs/attack-reach.md).

### Character panel

Opening the character sheet says the panel name with the character, class and
level once, and then each row is read as you move through it: strength,
dexterity, vitality, energy, defense, stamina, life and mana (as "45 of 120")
and the four resistances. The mini-menu buttons that open it and the other
screens are announced by name. See
[character panel, equipment slots and placing items](docs/character-panel.md).

### Life, mana and broken equipment

Life and mana are announced as they cross their steps: every 25 points while the
resource is comfortable (75, 50), every 10 below half (40, 30, 20, 10) and point
by point at the end (5, 4, 3, 2, 1). Crossing a step speaks the real percentage,
once, in either direction; a downward crossing at or below 10 % interrupts.
Simultaneous changes use one announcement, life first, so a mana warning cannot
erase the life warning from the same sample. The numbers are the
game's own, read from the HUD orbs' value display, so the *Show life orb text*
and *Show mana orb text* gameplay options must be on — with them off the game
writes no number and the mod identifies whichever option is missing, including
when the other orb works. Automatic hints wait for a persistent missing reading.
A worn item
that reaches zero durability is announced with its equipment slot, and an item
that starts being worn, however it got there, with its slot and name
("Equipado en Cabeza: Gorro, mágico"). An item whose requirements the character
does not meet (the ones the game draws red) says so with its name, and a refused
attempt to equip one says "No se puede equipar" and why. Console
`access-vitals on|off|status`.
See [life, mana and broken equipment](docs/vitals.md).

Player poison is read from the same native state check that colors the health
orb. Entering and leaving poison are announced once, and the vitals report
includes poison while active. These notices share the vitals toggle. Further
condition coverage is tracked in [combat status evidence](docs/combat-status-evidence.md).

The current mercenary's life uses the game's displayed percentage and the same
steps and critical interrupt rule as the player's life. Simultaneous player and
mercenary changes form one announcement, with the player's values first.
Mercenary level increases are announced once; the vitals report includes both
life and level. Missing mercenary data is not treated as death. Broken-equipment
notices queue behind the life alerts.


### Boss life

A boss's life is said as it falls, the way your own is: `Andariel, 74 por
ciento` when it crosses 75, 50, then every 10 down to 10, then every point
(interrupting at 10 and below), and `0` when it dies. It covers act and quest
bosses and superuniques (Rakanishu, Blood Raven, Andariel, Mephisto, Diablo...),
using the fill of the game's own health plate. The first sight is only a
baseline, so nothing is said until the fight starts. `access-boss-life
on|off|status`; `[boss_life] enabled` in the plugin TOML. See
[boss life](docs/boss-life.md).
### Weapon swap, stash gold and trading

A weapon swap (R3, W or the inventory's I/II tabs) names the set and the weapons
now in hand ("Armas 2: Arco largo, Carcaj"); console `access-weapon-swap on|off`.
See [weapon swap](docs/weapon-swap.md). In the stash, the withdraw button says the
gold the open tab holds ("12345 de oro en el alijo").

Trading with another player names the partner in the game's own request prompts,
names the two offer grids, and announces what the other player offers (items,
gold), both acceptances and whether the trade completed or was cancelled; console
`access-trade on|off`. See [trade](docs/trade.md).

The first time the scanner lists an exit on a map, dungeon entrances included, it
plays a discovery sound and says "El radar ha registrado una salida nueva" with the
exit's name (waypoints and the stash: "un lugar nuevo"; unused quest objects such as
the Tree of Inifuss or a Cairn Stone: "un objeto de misión nuevo"); the exit back to
the level just left is not announced. `[discoveries]` in the settings file. See
[exploration](docs/exploration.md).

## How it works

- The plugin listens to the game's UI message bus through the D2RLoader SDK (`PanelManager`,
  `FrontEndNavigation`, `OptionsPanelMessage`…).
- The game's own **FocusManager** panel owns a `FocusIndicator` widget; its screen rectangle is
  polled every UI update and matched against a catalog of focusable widgets generated offline from
  the game's UI layout JSON (`tools/`). Labels, tooltips and option lists come from the same layouts
  and are resolved through the game's localization tables at runtime.
- Speech goes through PRISM, loaded dynamically from `d2rloader/plugins/d2r-access/prism.dll`.
- In-game text uses a guarded native widget adapter: actual focused widget, visible
  children, class ancestry and runtime strings. It polls on the UI thread after the
  local player is ready; all speech stays on the dedicated PRISM thread. Details and
  verified build checks: [native UI evidence](docs/native-ui.md).
- World labels and item descriptions use verified native tooltip render hooks.
  They copy the primary and displayed comparison text before the game frees it
  each frame, while preserving the stock comparison selection. They require the
  supported game build and add no DLL dependencies. A separate native ground-item reader
  supplies nearby loot snapshots to proximity speech and spatial sound services.

## Requirements

- Diablo II: Resurrected 3.3.x (Steam or Battle.net), **D2RLoader 1.3.1-beta** installed in the
  game folder — the build `d2rloader.net` currently distributes. 1.3.0-beta is verified too.
  Older builds are no longer downloadable, so the native readers target these; a newer loader may
  work, since they locate the game pointers by what the loader resolved rather than by an offset
  into it, but only those two are verified.
- A screen reader (NVDA, JAWS) or Windows SAPI; PRISM picks the best one available.
- To build: CMake 3.28+, Ninja, MSVC Build Tools 2022 (x64). The D2RLoader PluginSDK is a git
  submodule (`external/PluginSDK`); PRISM's prebuilt DLL and header are vendored in
  `third_party/prism/`.

## Build and install

Build with the game closed. FFmpeg must be on `PATH` to convert the OGG loop
sources to WAV. The build embeds those sounds in the plugin DLL, so there are no
loose sound files to install or replace; FFmpeg is not needed to run the plugin.

```powershell
git clone --recurse-submodules <this repo>
cd d2r-access
.\build.ps1        # configures, builds and copies the plugin into the game's d2rloader\plugins
```

Set `D2R_GAME_DIR` in `CMakeLists.txt` (or pass `-DD2R_GAME_DIR=...`) if the game is not at the
default Steam path. Launch the game with `D2RLoader.exe`; the plugin speaks "Diablo 2 accessibility
loaded" when it starts. D2RLoader 1.3.0-beta ignores the language chosen in Steam: with
`text_locale` and `audio_locale` left empty the game starts in English. Set both explicitly in
`d2rloader/config/d2rloader.toml` (for example `"esES"`).

To start the loader from Steam's Play button instead, set the game's launch options (Properties →
General → Launch Options) to the loader's path followed by `%command%`, for example:

```
"D:\games\steam\steamapps\common\Diablo II Resurrected\D2RLoader.exe" %command%
```

Steam then passes the `D2R.exe` path to the loader as an argument, which the loader forwards to the
game harmlessly (verified with loader 1.3.0-beta). Do not rename or replace `D2R.exe`: the loader
maps it from disk, and Steam restores it on update or file verification.

Console commands (developer console, `Ctrl+`` in game): `access-say <text>`, `access-trace on|off`,
`access-dump-code`, `access-text on|off|repeat|dump`. The latter controls in-game text
reading and can log the visible native text tree. Its toggle lasts until the game exits.

Radar commands: `access-radar on|off|status|dump|test`. `test` auditions the
synthesized sounds; `dump` logs nearby native collision and sensor states.
Generated cues and their queue, submission, completion or cancellation are
logged automatically with event IDs, directions, origins and timing. See the
[radar event log](docs/radar-event-log.md) to compare cues with movement without
recording audio.
`status` (Ctrl+T) distinguishes dialogue/pause, an inactive game window, waiting
for the player, and unavailable terrain. Failed terrain reads record their
specific stage in the plugin log.
Unknown directions omit distances; an immediate body obstruction is described
as nearby. Unchanged terrain is intentionally silent between state changes.
Persistent radar preference: `[radar] enabled = true` (or `false`) in
`d2rloader/config/d2r-access.toml`, created from the DLL only when missing.

Footstep commands: `access-footsteps on|off|status`; Ctrl+F toggles the filter
until the game exits. The log records audible/stalled transitions and grouped
movement/animation/read-failure diagnostics, so a failed case can be investigated
without visually identifying whether the character is sliding along a wall.

Ground item commands: `access-items on|off|sound-on|sound-off|status`.
`on/off` controls proximity speech; `sound-on/sound-off` controls the loops.

Run automated checks after building: `ctest --test-dir build --output-on-failure`.
Test executables are not installed into the game.

## Local computer-use testing

The [desktop test tools](tools/computer_use/README.md) can capture the foreground
game, operate menus and send short movement inputs using Python and existing
Windows APIs. No packages or controller driver are required. Compare each action
with screenshots and Ctrl+T diagnostics; audio capture and analog stick emulation
are not implemented. The [first live measurements](docs/computer-use-radar-check.md)
record navigation discrepancies that still need investigation.

## Regenerating the UI catalog after a game patch

```powershell
cmake -S tools\casc-extract -B build\casc-extract -G Ninja && cmake --build build\casc-extract
build\casc-extract\casc_extract.exe extract "<game folder>" game-data "global\ui\layouts\" "local\lng\strings\"
python tools\layout_index.py
python tools\gen_panel_titles.py
python tools\gen_panel_widgets.py
```

Named places come from the HD map layouts:

```powershell
build\casc-extract\casc_extract.exe extract "<game folder>" <scratch> "hd\env\preset\"
build\casc-extract\casc_extract.exe extract "<game folder>" <scratch> "\pf_"
python tools\gen_places.py --hd-root <scratch>
```

## Layout

- `src/` — plugin: `plugin.cpp` (SDK entry points, wiring only), `screen_reader` (PRISM sink),
  `ui_message_monitor` (UI bus listener), `focus_tracker` (indicator matching, labels, values),
  `control_state`, `settings_file`, `input_direction`, `panel_state`, `widgets`, `localization`,
  `ui_thread_pump`, `speech_strings` (all spoken phrases), `generated/` (catalog headers).
  `native_memory`/`native_ui` provide guarded native snapshots; `ui_text` contains the
  testable reading policy; `ui_notifications` reads and deduplicates visible notices;
  `game_text_reader` manages gameplay lifecycle and speech context.
  `native_world` provides guarded client collision snapshots; `radar`/`radar_grid`
  evaluate navigation; `radar_audio`/`radar_synth` render cues and `radar_service`
  wires lifecycle, UI suspension and shortcuts.
- `tests/` — reading policy, native memory/tree fixtures and gameplay-service integration.
- `tools/` — CASC extractor, layout indexer/generators and local computer-use tools.
- `docs/` — discovery run scripts and research notes. `STATUS.md` — where the mod is.
  `docs/decisions.md` — dated design-decision log (what was rejected and why).
  `docs/testing.md` — target player profile and how to test/validate the mod.

## License

Code: MIT. PRISM is MPL-2.0 (see `third_party/prism/LICENSES`). Diablo II: Resurrected is a
trademark of Blizzard Entertainment; this project is not affiliated with or endorsed by Blizzard.
