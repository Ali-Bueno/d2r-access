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
- Everything is spoken in the game's text language (Spanish and English connective phrases so far).

Not yet: dynamic text (confirmation prompts, chat, item tooltips), in-game HUD and combat,
navigation. See `STATUS.md` for the live ledger.

## How it works (no game patching)

- The plugin listens to the game's UI message bus through the D2RLoader SDK (`PanelManager`,
  `FrontEndNavigation`, `OptionsPanelMessage`…).
- The game's own **FocusManager** panel owns a `FocusIndicator` widget; its screen rectangle is
  polled every UI update and matched against a catalog of focusable widgets generated offline from
  the game's UI layout JSON (`tools/`). Labels, tooltips and option lists come from the same layouts
  and are resolved through the game's localization tables at runtime.
- Speech goes through PRISM, loaded dynamically from `d2rloader/plugins/d2r-access/prism.dll`.

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
`access-dump-code`.

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
- `tools/` — CASC extractor and the layout indexer/generators.
- `docs/` — discovery run scripts and research notes. `STATUS.md` — where the mod is.

## License

Code: MIT. PRISM is MPL-2.0 (see `third_party/prism/LICENSES`). Diablo II: Resurrected is a
trademark of Blizzard Entertainment; this project is not affiliated with or endorsed by Blizzard.
