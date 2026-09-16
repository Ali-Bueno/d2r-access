# d2r-access — screen reader accessibility for Diablo II: Resurrected

A [D2RLoader](https://d2rloader.net) plugin that makes Diablo II: Resurrected playable with a
screen reader (NVDA, JAWS, SAPI…) through [PRISM](https://github.com/ethindp/prism). Built for
blind players using a controller; keyboard and mouse are planned.

## What works today

- Screen changes and panels are announced with the game's own localized titles (title screen,
  main menu, character select/create, options tabs, cinematics, calibration, modals).
- **Controller focus is read on every menu**: the focused button, checkbox, dropdown, carousel,
  slider or text field is spoken with its label, its role and the game's own tooltip text.
- Slider values are spoken as a percentage, on focus and while changing them.
- Checkbox and carousel states are tracked from the game's `Settings.json` plus the player's input;
  dropdown popups read the highlighted option.
- Character class names while creating a character.
- Native text in dropdown rows, character rows and confirmation prompts.
- Everything is spoken in the game's text language (Spanish and English connective phrases so far).

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
Allocating a point reads the changed value and remaining count. Skill trees follow
the game's **D-pad grid navigation**: branch tabs are named, and each highlighted
skill reads its name first, followed by its displayed description, requirements,
level, row/column and remaining points. General/item-granted skills use the same
description reader. No points are spent by the mod. These additions passed
automated checks; corrected controller speech awaits live confirmation.
See [progression evidence](docs/progression.md).

Inventory icon buttons now use their native label/help or controller legend,
including **Soltar oro**, and equipment slots announce their body location
(head, neck, weapon, ring, belt, and so on), followed by any rendered item description.
The user confirmed these additions. The specific
scroll-related control reported by the user has not yet been identified live.

An experimental **navigation radar** now reads the collision grids of loaded
game rooms. It synthesizes directional `tin` (opening) / `tock` (blocked) cues,
using fixed **up / left / right / down** directions that match the left stick;
down has a lower pitch. Simultaneous cues play successively with **50 ms of
silence between them**. The radar checks the full body along each passage using
the player's native movement mask. It also recognizes a complete, uniquely
identifiable player footprint displaced from the reported cell, preventing that
footprint from appearing as a nearby wall. Ambiguous shapes keep their blocking
markers. It filters brief openings using measured
movement speed from the game's fractional position and a reaction window, warns earlier when approaching walls
faster, and separates creature occupancy from terrain changes. Openings must
still pass the full movement-collision check. It starts enabled, baselines silently without
walking, and pauses for dialogue and offline pause menus. It uses the game's map
data directly. Live camp/Blood Moor reads are confirmed; the latest speed and
noise filtering still needs controller validation. The running forecast now
accounts for actual pending and simultaneous cues, so an empty sound queue no
longer reserves four sounds before every turn;
see [the radar evidence and controller test](docs/sonar-radar.md).
**Ctrl+R** toggles it; **Ctrl+T** reads current clearance and records a local
diagnostic snapshot. Short passages are described as limited space; recognizable
actor obstruction is described as an occupied passage. The embedded default config
preserves existing settings and adds no external sound/map installation step.
See the [Kirandur comparison and reaction model](docs/kirandur-radar-comparison.md).
Details and regression checks: [running radar timing](docs/radar-running-timing.md).

An experimental **stalled-footstep filter** suppresses repeated local HD
footfalls when the character remains at exactly the same position while walking
or running. Any fractional movement restores footsteps, including sliding along
a wall. It starts enabled; **Ctrl+F** toggles it for comparison. The first blocked
footfall may still sound. Native code checks and automated tests pass; the new
filter awaits live listening validation. See [footstep behavior and limits](docs/footsteps.md).

Not yet: full inventory operation coverage, full chat history, general in-game HUD/combat,
full-map navigation and destination beacons.
Named landmarks inside towns are under investigation; see [map findings](docs/map-landmarks.md).
See `STATUS.md` for the live ledger.

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
  build and adds no DLL dependencies. It never reads all nearby loot at once.

## Requirements

- Diablo II: Resurrected 3.3.x (Steam or Battle.net), **D2RLoader 1.2.1-beta or newer** installed
  in the game folder.
- A screen reader (NVDA, JAWS) or Windows SAPI; PRISM picks the best one available.
- To build: CMake 3.28+, Ninja, MSVC Build Tools 2022 (x64). The D2RLoader PluginSDK is a git
  submodule (`external/PluginSDK`); PRISM's prebuilt DLL and header are vendored in
  `third_party/prism/`.

## Build and install

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
