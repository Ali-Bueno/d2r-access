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
- Native text in dropdown rows, character rows and confirmation prompts.
- Everything is spoken in the game's text language: the plugin's own phrases are translated into all thirteen D2R locales and follow the language selected in the game's options (changing it in Options switches the plugin too).

Implemented, awaiting live NVDA/controller verification: in-game NPC menus and topic
lists, full scrolling dialogues, tutorial/automatic hints and area-name banners. The
reader follows native controller focus and visible text, and suppresses repeated
announcements. See [the controller test guide](docs/test-ingame-text.md).

The user confirmed the Rogue Encampment entry announcement. Informational notices
(including the Classic/Expansion restriction when pressing **G**) and the visible
HUD notification feed now have a reader. The user confirmed that the G warning
also reads on subsequent presses. New notices queue
without interrupting a conversation. A new delivery of identical text is read
again; simply leaving a notice visible stays silent. Notification text with links
uses its native embedded textbox.

The user confirmed the stash and Charsi target labels, basic inventory
selection, smooth inventory navigation, and axe descriptions in the blacksmith's shop. The reader
also exposes inventory coordinates and selected stash tabs. It follows item grids and
equipment; it reads complete rendered descriptions and suppresses stationary
repeats. Described grid items now read without row/column prefixes. An occupied
cell waits for the game's tooltip without announcing coordinates first; empty
cells retain coordinate feedback. The user confirmed this behavior. See
[native tooltip evidence](docs/native-tooltips.md).

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
that mode announces that the item is no longer held. These notices occur on
state changes, and manual repeat includes the current held-item context.

An experimental **navigation radar** now reads the collision grids of loaded
game rooms. It synthesizes directional `tin` (a usable passage opens) / `tock`
(a nearby structural barrier closes that passage) cues,
using fixed **up / left / right / down** directions that match the left stick;
down has a lower pitch. Simultaneous cues play successively with **50 ms of
silence between them**. The radar checks the full body along each passage using
the player's native movement mask. It also recognizes a complete, uniquely
identifiable player footprint displaced from the reported cell, preventing that
footprint from appearing as a nearby wall. Ambiguous shapes keep their blocking
markers. It separates creature occupancy from terrain and keeps the original
movement mask as the final opening check. Isolated furniture and small clutter
are excluded from automatic space changes when a full-body check proves a route
around them. Walls, doors, enclosed spaces and objects cutting off a corridor
remain structural barriers. Uncertain cases retain their collision.

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

**Level exits:** every way out of the current area (level exits, cave and
dungeon entrances, town portals, red portals, waypoints, gateways) plays the
supplied `portal` loop at its position through the same spatial mixer, and the
entity scan names it after its destination. **Ctrl+P** toggles the exit loops;
`access-items exits-on|exits-off`. See [level exits](docs/level-exits.md).

**Containers:** chests, caskets, urns, barrels, crates, corpses and the other
things you open for loot loop through the same mixer, so you can hear where they
are; opening one silences it. What counts as a container comes from the game's
own interaction handler, not from the name, so it holds in every language.
**Ctrl+K** toggles them; `access-items chests-on|chests-off`, and `chests-dump`
logs the verdict for everything currently in sight. See
[container loops](docs/ground-items.md#container-loops).
The user confirms that the ground-item loops are audible. The quieter level and landing-only
start await a follow-up listening check.

**Target guidance (built; live validation pending):** scan an entity with
the right stick, then click the stick or press **Ctrl+B** to follow it with
directional pings. Repeat to cancel. Fixed stairs and level passages can be
scanned beyond ordinary range and through walls, including inactive rooms whose
exit presets the game retains. Never-generated positions remain unknown. See
[distant exits](docs/distant-exits.md) and [target guidance](docs/beacon.md).

**Quests (built; live validation pending):** quest buttons have names from the
game's string tables. Selecting a quest reads its current objective; changing
progress reads only updated text, including counters supplied by the game.
Empty detail panes are explicitly reported. Automatic alerts announce journal updates and primary
quest objectives achieved, without opening the journal. When updated objective
text is available and identifiable, the reader speaks the quest and objective;
otherwise it says that the journal was updated. Announcements wait for dialogue
or menus to close. Console `access-quests on|off|status` controls this feature.
See [quest-log implementation and remaining work](docs/quests.md).

**Room exits (built; live validation pending):** on maze levels (the Crypt, Catacombs, the Den of
Evil and the rest), walking into a room speaks its ways out — plain openings as a screen direction,
stairs and portals by their own name. **Ctrl+V**, or holding the right stick down for 350 ms,
reviews them one at a time, nearest first, with distance, clock bearing and the beacon's verdict on
getting there (straight, around or no route). A short right-stick click, or **Ctrl+B** right after,
guides to the one just reviewed. **Ctrl+M** toggles the feature.
See [room exits](docs/rooms.md).

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
once, in either direction; at or below 10 % it interrupts. The numbers are the
game's own, read from the HUD orbs' value display, so the *Show life orb text*
and *Show mana orb text* gameplay options must be on — with them off the game
writes no number and the mod says so once, naming those options. A worn item
that reaches zero durability is announced with its equipment slot. Console
`access-vitals on|off|status`.
See [life, mana and broken equipment](docs/vitals.md).

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
- World labels and item descriptions use one verified native tooltip render
  hook. It copies the primary displayed text before the game frees it each
  frame, then preserves the stock draw call. It requires the supported game
  build and adds no DLL dependencies. A separate native ground-item reader
  supplies nearby loot snapshots to proximity speech and spatial sound services.

## Requirements

- Diablo II: Resurrected 3.3.x (Steam or Battle.net), **D2RLoader 1.3.0-beta** installed in the
  game folder — the build `d2rloader.net` currently distributes. Older builds are no longer
  downloadable, so the native readers target this one; a newer loader may work, since they locate
  the game pointers by what the loader resolved rather than by an offset into it, but only 1.3.0
  is verified.
- A screen reader (NVDA, JAWS) or Windows SAPI; PRISM picks the best one available.
- To build: CMake 3.28+, Ninja, MSVC Build Tools 2022 (x64). The D2RLoader PluginSDK is a git
  submodule (`external/PluginSDK`); PRISM's prebuilt DLL and header are vendored in
  `third_party/prism/`.

## Build and install

Build with the game closed. FFmpeg must be on `PATH` to convert the six OGG
loop sources to WAV. The build also installs these audio assets; FFmpeg is not
needed to run the installed plugin.

```powershell
git clone --recurse-submodules <this repo>
cd d2r-access
.\build.ps1        # configures, builds and copies the plugin into the game's d2rloader\plugins
```

Set `D2R_GAME_DIR` in `CMakeLists.txt` (or pass `-DD2R_GAME_DIR=...`) if the game is not at the
default Steam path. Launch the game with `D2RLoader.exe`; the plugin speaks "Diablo 2 accessibility
loaded" when it starts. If the game runs in the wrong language under D2RLoader, set `text_locale`
in `d2rloader/config/d2rloader.toml`.

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

## License

Code: MIT. PRISM is MPL-2.0 (see `third_party/prism/LICENSES`). Diablo II: Resurrected is a
trademark of Blizzard Entertainment; this project is not affiliated with or endorsed by Blizzard.
