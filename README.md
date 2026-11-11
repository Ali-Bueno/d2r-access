# d2r-access — screen reader accessibility for Diablo II: Resurrected

A [D2RLoader](https://d2rloader.net) plugin that makes Diablo II: Resurrected playable by blind
players. Everything the game shows is spoken through the player's screen reader (NVDA, JAWS, or
the Windows voice) via [PRISM](https://github.com/ethindp/prism), and spatial sounds give what text
cannot: walls and passages, loot, exits, chests, doors, monsters. It is played with a controller or
the keyboard, in any of the game's thirteen languages.

## Status: the whole game is playable

Several testers playing by screen reader have taken characters through the complete game with this
mod, from the first quest of Act I to the end of Act V, and finished it with no blocking problems:
menus, character creation, towns and shops, every act's dungeons and quest puzzles, the act bosses
and the end screen, with the controller and with the keyboard, alone and together (see
[Thanks](#thanks)).

Development continues through frequent test builds shaped by the testers' reports. The newest
addition, the audio description of the act cinematics, is confirmed from the main menu's Videos
list; the videos the game plays between acts are being heard in play for the first time. Reports
on anything that sounds wrong are welcome.

## Getting started

### Requirements

- Diablo II: Resurrected 3.3.x or 3.4.x from Steam or Battle.net, on 64-bit Windows 10 or 11. On 3.4 the loader first shows a notice that this version is not fully tested: press OK.
- **D2RLoader 1.3.1-beta**, the build `d2rloader.net` distributes (1.3.0-beta also works). The
  complete package below includes it.
- NVDA or JAWS running before the game starts; without either, PRISM uses the Windows voice (SAPI).
- On Linux with Proton, speech goes to Orca or Cthulhu on the host (Speech Dispatcher and Spiel
  are not yet tried in game), with the shipped `d2r-access-proton.sh` set as the game's Steam
  launch option, run through `sh` ([Proton](docs/proton.md)).

### Install and update

1. Close the game.
2. Download the [complete package](https://github.com/Ali-Bueno/d2r-access/releases/latest/download/D2RAccess.zip)
   (the mod, D2RLoader and the Visual C++ runtime).
3. Extract everything into the game folder, the one that has `D2R.exe`, replacing files if asked.
   Do not rename or replace `D2R.exe`.

The package's manual, `README.html`, explains every step, control and setting in detail. To update, extract a newer
package on top: it carries no settings files, so the loader's options and the mod's settings stay
as they are.

### Start the game

Start it with **`D2RAccess.exe`** in the game folder ([launcher](docs/launcher.md)). It renews the
online license before Blizzard's 30-day offline limit is reached, offers a newer version of the mod
when there is one (switch `update-check`), and starts the game through D2RLoader. The mod says
"Diablo 2 accessibility loaded" when it is running. It opens Steam or the Battle.net app, whichever
your copy came from, when it is closed; if that fails it shows a message and does not start the game.

To use Steam's Play button instead (the installer offers to set it), set the game's launch options
(Properties → General → Launch Options) to the launcher's path followed by `%command%`:

```
"C:\Program Files (x86)\Steam\steamapps\common\Diablo II Resurrected\D2RAccess.exe" %command%
```

### Language

The mod speaks in the game's text language, in all thirteen D2R locales. D2RLoader starts the
game in English unless its configuration names a language, so set both lines in
`d2rloader/config/d2rloader.toml`, for example:

```toml
text_locale = "esES"
audio_locale = "esES"
```

Codes: `enUS`, `deDE`, `frFR`, `itIT`, `esES`, `esMX`, `plPL`, `ptBR`, `ruRU`, `jaJP`, `koKR`,
`zhCN`, `zhTW`. With `text_locale` empty, the game's own *Text language* option rules the text and
the mod follows it at once.

## Controls at a glance

The game keeps its whole controller layout; the mod adds these (keyboard equivalents in brackets).
Every control for the Xbox and PlayStation pads and the keyboard, with how the menus, the belt, the
skills and the stash work, is in BFBeast666's [commands and tutorial](GUIDE.md) (`GUIDE.html` in the
package). The full lists are in the package's `README.html`, and in
[keyboard keys](docs/keyboard-keys.md) and [keyboard play](docs/keyboard-play.md).

| Controller | Keyboard | What it does |
|---|---|---|
| Right stick up / down | Ctrl+Page Up / Down | Scanner: previous / next category |
| Right stick left / right | Page Up / Down | Scanner: previous / next thing, nearest first |
| BACK + LB | Ctrl+B | Beacon: follow or release what the scanner named |
| BACK + A | Alt+B | Beacon follows the scanner: each thing named is followed at once (off by default) |
| BACK + right stick down | Ctrl+E | The route to the beacon's or scanner's target |
| BACK + right stick up | Shift+Y | Description of the area you are in |
| BACK + Y | Ctrl+Y | The ways out of the room you are in |
| BACK + B | Ctrl+O | Area overview |
| BACK + X | Z / X | Life, mana, stamina, states on you |
| BACK + RT | Shift+Z | Mercenary, summons and party members, one at a time |
| Hold LT / BACK + LT | — | The skills on each skill bar |
| LB (in play) | — | Audible loot filter on or off |
| BACK + RB | Ctrl+S | The mod's settings list |
| BACK + right stick left / right | K / L | Tutorial: previous / next message |
| — | Ctrl+T | Radar status (on or off in the settings list) |
| — | D | Walk to the nearest usable thing and use it |
| — | A / W, held | Fight with the left / right skill; a buff or a summon is cast once where you stand |
| — | Shift+A, held | Attack in place with the left skill: no walking up, no falling back |
| — | F1 to F8 | The game's skill keys; the mod says the skill when it changes |
| LB / RB in the shared stash | Alt+Page Up / Down, Ctrl+Shift+Page Up / Down | Shared stash: previous / next page; ten pages back / forward |

In menus, right stick up reads the detail of what has focus (an item's full description, a
skill's text) and right stick down the buttons the screen offers.

## What the mod makes accessible

### Menus and screens

- **Every menu with the controller:** the focused button, checkbox, dropdown, carousel, slider or
  text field is spoken with its label, its state or value (sliders as a percentage), its role and
  the game's own tooltip. Controls the game locks are said to be unavailable.
- Screen and panel changes are announced with the game's own localized titles: title screen, main
  menu (with its news), character select and creation, difficulty, options, confirmation prompts.
- Character rows say the title the game shows above the name ("Slayer Tank, Level 39 Paladin");
  class names are read while creating a character.
- The death screen and the end screen after the last boss read themselves whole.
- NPC menus, topic lists and full scrolling dialogue are read. With the game's *NPC Speech* option
  on *Audio Only*, the voice carries the dialogue instead.
- Notices, the HUD notification feed, area-name banners and the game's own hints are read, queued
  without cutting into a conversation.
- The waypoint panel says each row's state ("Dark Wood, you are here", "Jail Level 1,
  undiscovered", "Terror Zone") and its act tabs ([waypoints](docs/waypoints.md)).

See [native UI](docs/native-ui.md) and [in-game text](docs/test-ingame-text.md).

### Items, inventory, shops and trading

- Inventory, stash, equipment and shop grids name each item and its quality (`Crimson Spear,
  magic`), with the game's full name for magic, set, rare, unique and crafted items, as soon as
  the cursor lands: the game's tooltip delay is removed. Right stick up reads prices and
  properties, or `autodetail` reads them automatically.
- **Comparing:** hold Y on an item and move the left stick: right reads the equipped item it is
  compared with, left the item again, up its detail. Keyboard: Shift+Page Up / Shift+Page Down.
  See [native tooltips](docs/native-tooltips.md).
- Equipment slots say their body location and whether they are empty. Items whose requirements the
  character does not meet say so, and a refused attempt to equip one says why.
- Picking up an item says "Item in hand" and where it can go; placing it names the destination.
- The belt names each cell's D-pad direction, row and potion ([belt](docs/belt.md)).
- Stash tabs, LT/RT tab changes, the stash's gold, and shop, repair and other NPC service buttons
  with the game's own labels. Nothing is bought, sold or repaired automatically. Set items are
  said by their set name, a shop's empty categories are skipped, and the shared stash's pages are
  said as they change ([stash pages](docs/stash-pages.md)).
- The Horadric Cube says what moves in and out and what Transmute made ([Horadric Cube](docs/horadric-cube.md)).
- A weapon swap names the set and the weapons now in hand ([weapon swap](docs/weapon-swap.md)).
- Trading with another player names the partner, both offers, both acceptances and the outcome
  ([trade](docs/trade.md)).

See [inventory and NPC follow-up](docs/inventory-npc-followup.md).

### Character, skills and mercenary

- The character sheet reads each statistic as you move through it, and the point buttons name
  Strength, Dexterity, Vitality or Energy with the value and the points left
  ([character panel](docs/character-panel.md), [progression](docs/progression.md)).
- Skill trees move one skill at a time with the D-pad. Each skill says its name, position, points,
  controller binding and the actions available, then its full description, requirements and
  levels ([skill actions](docs/skill-actions.md)), then the skills it needs a point in
  ("Prerequisite: 1 skill point in Howl"). The mod never spends a point by itself.
- On the keyboard, F1 to F8 say the skill they select when it changes ([skill hotkeys](docs/skill-hotkeys.md)).
- Holding LT reads the six skills on that bar ([skill bar](docs/skill-bar.md)).
- The mercenary panel, the advanced statistics panel and hiring rows (with each candidate's
  skills) are read row by row.

### Finding your way

- **Radar** ([readings](docs/radar-readings.md)): four directional cues that match the left stick
  (or its diagonals, `[radar] diagonals`): a bright *tin* where a passage leads somewhere, a soft
  breath for open floor, a dull *tock* for a nearby wall, two knocks for a closed door and a short
  knock for a lone obstacle with a way round it. It follows passages a full body can use,
  remembers walls as you move, and stays silent while nothing changes. Two short pips mark a way
  into ground you have not explored yet. The settings list turns it on or off, Ctrl+T describes the surroundings.
- **No wall sliding:** a stick push into a wall stops instead of letting the game slide you along
  it, with a low bump sound panned to the side you pushed; narrow stairs and passages keep the
  game's slide ([wall sliding](docs/wall-sliding.md)).
- **Scanner** ([entity scan](docs/entity-scan.md)): what a sighted player finds on the automap,
  by category, nearest first, with distance and direction: exits, portals, waypoints, NPCs, the
  stash, shrines, objects, quest objects, explored and unexplored areas, enemies in sight, other
  players, and corpses. Exits, waypoints and the stash you have found stay listed in later games
  on the same map.
- **Beacon** ([target guidance](docs/beacon.md)): directional pings lead along a real route to what
  the scanner named, and the mod says when you arrive.
- **Route description** ([route description](docs/route-description.md)): the way to the target as
  a sighted player plans it off the minimap ("Route to Akara, 85 units: north, 20; then door; then
  east, 30"), then each turn as you reach it, and "Off the route" if you leave it. Nothing walks
  for you.
- **Room exits** ([rooms](docs/rooms.md)): entering a room in a maze level (the Crypt, the
  Catacombs, the Den of Evil...) speaks its ways out; in hand-made levels such as the Throne of
  Destruction, BACK+Y gives the ways out of the chamber you stand in.
- **Named places** ([places](docs/places.md)): bridges, camps, graveyards, taverns, palace
  entrances, and corridors, bends, junctions and dead ends in mazes, all from the game's own map
  layouts.
- **Area overview** ([terrain overview](docs/terrain-overview.md)): where you stand in the area,
  how much you have seen, how far each side reaches and what lies beyond it.
- **Area and landmark descriptions** ([descriptions](docs/descriptions.md)): hand-written words for
  every area and its landmarks (the Hangman's Tree, the Hellforge), in the game's language.
- **Exploration mode** (default): the scanner lists what you have explored or seen, as a sighted
  player's automap would, and routes guess across rooms never entered and say so. *Full
  assistance* in the settings list lists everything the game has loaded
  ([exploration mode](docs/beacon.md#exploration-mode), [exploration](docs/exploration.md)).
- A new exit, waypoint or quest object found on the map plays a discovery sound and is named.
- Town portals are listed by owner ("TankMan's portal") and say where they lead ([portals](docs/portals.md)).

Directions are screen directions as compass points (north is the top of the screen) or, in the
settings list, clock hours. Distances are in units, the game's smallest map cell
([spoken directions](docs/radar-readings.md#spoken-directions-2026-10-13)).

### Sounds of the world

All spatial sounds follow the screen and fade with distance; they pause in dialogue, menus that
pause the game and when the window loses focus. Each family has its own switch and volume.

- **Loot** ([ground items](docs/ground-items.md)): approaching an item reads its name and quality
  (and gold its amount); gold, equipment, potions, scrolls, runes and gems each loop their own
  sound. LB turns on the **audible loot filter**, which keeps only the drops you chose in the
  settings list (potions, gems and runes, equipment from a chosen quality up).
- **Containers:** chests, urns, barrels, corpses and weapon racks loop until opened, recognised by
  the game's own interaction rule, so it holds in every language.
- **Doors:** a shut door loops a wooden, metal, stone or slime sound, chosen by the sound the game
  plays when it opens; doorways creak; a secret door in the Jail gives "You feel a cool draft".
- **Exits:** stairs, cave entrances, portals and waypoints loop one sound, walkable area crossings
  another, the stash its own. Unexplored rooms in dungeons loop a sound of their own.
- **Quest objects** ([quest objects](docs/quest-objects.md)): quest items and mechanisms (the Tree
  of Inifuss, the Cairn Stones, the Horadric Staff orifice, the Compelling Orb, the Hellforge,
  Frozen Anya, the Chaos Sanctuary seals...) loop until used. Shrines and wells loop too.
- **NPCs and enemies** ([entity cues](docs/entity-cues.md)): soft pulses locate the nearest NPC
  and up to three enemies, with bosses, elites and ranged enemies given priority. A rising ping
  says an enemy is within melee reach by the game's own rule ([attack reach](docs/attack-reach.md)),
  and a short beep says the game's target landed on an enemy ([crosshair](docs/crosshair.md)).
- A Necromancer hears a bone rattle toward usable corpses.

Navigation and loot sounds come from game state, never from the text, so they work in every
language ([audio language audit](docs/audio-language-audit.md)).

### Combat and survival

- **Life and mana** are announced as they cross their steps, more often as they fall, interrupting
  at 10 % and below; a heartbeat loops while life is low ([vitals](docs/vitals.md)). The game's
  *Show life orb text* and *Show mana orb text* options are read when on; without them the mod reads the values directly.
- **States:** poison, freezing, chill, stun, slowing and curses are named as they begin and end,
  and a shrine's blessing as it wears off ([status effects](docs/status-effects.md)).
- **Boss life** is said as it falls, for act bosses, quest bosses and superuniques
  ([boss life](docs/boss-life.md)). Monster names are spoken as they come within reach, bosses and
  elites from first sight, with ranks, enchantments and immunities in the scanner.
- The mercenary's life and level, summons grouped as the game's portraits show them, and party
  members' life and whereabouts ([summons and corpses](docs/summons-and-corpses-re.md)).
- Running auras and buffs, worn items breaking, walking and running, and low stamina.
- On the keyboard, a buff, a summon or a spell that needs no target is cast once where you stand,
  and Shift+A attacks in place ([skill targeting](docs/battle-skill-targeting.md)).
- Your corpse and party members' corpses are listed in the scanner
  ([player corpses](docs/player-corpse-re.md)).

### Quests, story and guidance

- **Quest log** ([quests](docs/quests.md)): every quest by name, with its current objective and the
  game's own counters. Progress is announced as it happens, even with the journal closed.
- **Quest puzzles:** the Scroll of Inifuss reads the Cairn Stone sequence and each stone names its
  symbol ([Cairn Stones](docs/cairn-stones-re.md)); the Canyon of the Magi's true tomb symbol and
  each tomb entrance's marking are read ([Tal Rasha symbols](docs/tal-rasha-symbols.md)); the Chaos
  Sanctuary seals are numbered and the mod says how many are left.
- **Audio description of the cinematics** ([cinematics](docs/cinematics-audio-description.md)): the
  seven act cinematics, in both the 2021 and the classic 2000 versions, are narrated between their
  spoken lines in every language the mod speaks. Switch `audio-description`.
- **Tutorial** ([tutorial](docs/tutorial.md)): a new level 1 character playing alone is first asked
  whether to take it (press A / cross or Enter twice for yes, B / circle or Escape twice for not now,
  Y / triangle or Backspace twice to never be asked again; the game takes no input until then), then
  guided through the mod's tools up to the Den of Evil's reward, buying tomes from Akara, spending
  the skill point and the camp's waypoint, with short lessons on levelling up, the inventory, gold
  and shops. Its waits follow the screen reader when it can tell that it has stopped talking (NVDA).

### Playing with others

D2RLoader's host, game browser and direct-join forms are read as a list, and a controller can type
into their text boxes with a QWERTY keyboard on the right stick. Other players and their corpses
appear in the scanner, party members in the life report, and trades are read in full
([multiplayer](docs/multiplayer.md)).

### Settings

**Ctrl+S** (BACK+RB) opens the mod's settings list, in groups (Sounds, Volumes, Enemies and NPCs,
Navigation, Items and menus, Alerts, Controller, Loot filter...), remembered between sessions. It
includes **speech priority** for each kind of news ([speech priority](docs/speech-priority.md)),
the pace of every pulsing loop, and a **sound glossary** that names and plays each of the mod's
sounds. The rest of the configuration lives in `d2rloader/config/d2r-access.toml`.

## Known limitations

- No Battle.net: D2RLoader runs the game offline, so there is no Ladder, no Seasons and no online
  characters. Offline characters can still play together over D2RLoader's TCP/IP multiplayer
  ([Battle.net, Ladder and Seasons](docs/battle-net-and-ladder.md)).
- Native readers are tied to the supported game and loader builds. A game patch may silence some
  readings until the mod is updated (see
  [regenerating the UI catalog](#regenerating-the-ui-catalog-after-a-game-patch)).
- D2RLoader starts the game in English unless its configuration names a language (see
  [Language](#language)); it ignores Steam's language setting.
- Area descriptions and cinematic narration are written in English and machine-translated into
  the other twelve languages; the Spanish ones have been proofread.
- The scanner's line of sight uses walkable terrain, so a see-through fence hides what is behind
  it until you go round.
- Quest object sounds reach the rooms the game has loaded around you; the scanner and the beacon
  find the rest.
- The chat history cannot be reviewed yet.

Known issues and the work in progress are tracked in [`STATUS.md`](STATUS.md).

## Thanks

This mod reached the end of the game thanks to its testers, who played build after build, said
what went wrong and what they needed, and kept going until the credits rolled.

- **mojsior**, for testing.
- **bladestorm360**, for testing and for implementing keyboard play, and then finishing the whole
  game with it.
- And a special thanks to **BFBeast666** and **Raine**, a couple who reported a huge number of bugs,
  contributed area descriptions and ideas, finished the game together, and told me about their
  progress every step of the way.

## For developers

### How it works

- The plugin listens to the game's UI message bus through the D2RLoader SDK (`PanelManager`,
  `FrontEndNavigation`, `OptionsPanelMessage`…).
- The game's **FocusManager** panel owns a `FocusIndicator` widget; its screen rectangle is matched
  against a catalog of focusable widgets generated offline from the game's UI layout JSON
  (`tools/`). Labels, tooltips and option lists come from the same layouts and are resolved
  through the game's localization tables at runtime.
- In-game text uses a guarded native widget adapter (focused widget, visible children, class
  ancestry, runtime strings), polled on the UI thread; all speech runs on a dedicated PRISM thread
  ([native UI](docs/native-ui.md)).
- World labels and item descriptions come from native tooltip render hooks; a native ground-item
  reader and collision-grid snapshots feed the spatial sound services.
- Speech goes through PRISM, loaded dynamically from `d2rloader/plugins/d2r-access/prism/prism.dll`,
  next to its Wine bridges ([Proton](docs/proton.md)).
  Sounds are embedded in the plugin DLL.

The full map of services and hooks is in the [architecture guide](docs/architecture.md); design
decisions and what was rejected are in [decisions](docs/decisions.md).

### Build

Requirements: CMake 3.28+, Ninja, MSVC Build Tools 2022 (x64), and FFmpeg on `PATH` (it converts
the OGG sources at build time; it is not needed to run the plugin). The D2RLoader PluginSDK is a git
submodule (`external/PluginSDK`); PRISM 0.18.3's prebuilt DLL, Wine bridges and header are
vendored in `third_party/prism/`.

Build with the game closed:

```powershell
git clone --recurse-submodules https://github.com/Ali-Bueno/d2r-access.git
cd d2r-access
.\build.ps1        # configures, builds and copies the plugin into the game's d2rloader\plugins
ctest --test-dir build --output-on-failure
```

Set `D2R_GAME_DIR` in `CMakeLists.txt` (or pass `-DD2R_GAME_DIR=...`) if the game is not at the
default Steam path. The build runs at below-normal priority and leaves two logical processors free.
Test packages are published with `tools\test-release.ps1` ([test releases](docs/test-releases.md)).

### Testing

[Testing](docs/testing.md) describes the target player, how tests are scripted as controller and
key sequences with the expected speech, and how to verify them from the plugin's log
(`d2rloader/logs/d2r-access.log`). The [desktop test tools](tools/computer_use/README.md) can drive
the game unattended with keyboard input and screenshots.

### Console commands

In the developer console (`Ctrl+`` in game): `access-say <text>`, `access-trace on|off`,
`access-dump-code`, `access-text on|off|repeat|dump`, `access-radar on|off|status|dump|test`,
`access-footsteps on|off|status`, `access-items on|off|sound-on|sound-off|status`,
`access-entity-cues on|off|status`, `access-boss-life on|off|status`, `access-quests on|off|status`,
`access-places on|off|where`, `access-beacon full|exploration`. Radar cues are logged with event IDs
and timing ([radar event log](docs/radar-event-log.md)).

### Regenerating the UI catalog after a game patch

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

### Layout

- `src/` — the plugin: `plugin.cpp` (SDK entry points, wiring only), `screen_reader` (PRISM sink),
  `ui_message_monitor` (UI bus listener), `focus_tracker` (indicator matching, labels, values),
  `speech_strings` (all spoken phrases), `native_memory`/`native_ui` (guarded native snapshots),
  `native_world` (collision snapshots), `radar*` (navigation), `generated/` (catalog headers).
- `data/` — descriptions and cinematic narration per language.
- `tests/` — reading policy, native memory/tree fixtures and gameplay-service integration.
- `tools/` — CASC extractor, layout indexer and generators, release scripts, computer-use tools.
- `docs/` — feature notes and research. `STATUS.md` — where the mod is and what comes next.
- `GUIDE.md` — the players' commands and tutorial; `packaging/README.md` — the package's manual (`README.html`).

## License

Code: MIT. PRISM is MPL-2.0 (see `third_party/prism/LICENSES`). Diablo II: Resurrected is a
trademark of Blizzard Entertainment; this project is not affiliated with or endorsed by Blizzard.
