# AGENTS.md — d2r-access (Diablo II: Resurrected accessibility plugin)

Read **`STATUS.md` first** (identity, section table, derived facts, next step, known issues). The
general playbook lives one folder up (`../AGENTS.md` / `../CLAUDE.md`, `../PRINCIPLES.md`,
`../reference/`). This file is the mod-specific quick guide.

## What this is

A **D2RLoader plugin** (`d2rl-d2r-access.dll`, C++20, PluginSDK API v4 as a git submodule in
`external/PluginSDK`) that speaks through **PRISM** (`third_party/prism`, `prism.dll` shipped next
to the plugin in `plugins/d2r-access/`). Target: a blind player using **NVDA and an Xbox-style
controller**, game in **Spanish**. Front-end menus are done. In-game text is implemented and built,
awaiting live NVDA/controller validation (`docs/test-ingame-text.md`).

## Build, deploy, test

- Build: `.\build.ps1` (vcvars64 + CMake/Ninja Release). The POST_BUILD step copies the DLL and
  `prism.dll` into `D:\games\steam\steamapps\common\Diablo II Resurrected\d2rloader\plugins\`.
  Build only while the game is closed.
- User-led tests use controller button sequences. The user also authorized local computer-use
  setup/testing on 2026-09-15: `tools/computer_use/README.md` documents the working capture/input
  tools and game bindings. Inspect screenshots between short actions; do not assume keyboard
  movement validates analog controller behavior or audible cues. Verify from screenshots and
  the plugin log: `D:\games\steam\steamapps\common\Diablo II Resurrected\d2rloader\logs\d2r-access.log`.
  The last session starts at the last line containing `d2r-access loaded.`; ignore `MonsterHealth`
  lines (per-frame noise). Useful line prefixes: `[ui]` (game UI messages), `[focus]` (what was
  announced and why), `[value]`, `[popup]`/`popup option`, `[modal]`, `[native]` (native widget
  probes), `[saves]`, `[pump]` (tick rate / longest gap), `slow tick`.
- Console (Ctrl+` in game): `access-say <text>`, `access-trace on|off` (log every UI message +
  per-panel focus diagnostics), `access-probe Panel/Child/Child` (log the strings inside a native
  widget), `access-dump-code`, `access-text on|off|repeat|dump`.
- Automated tests: `ctest --test-dir build --output-on-failure --timeout 20`.
  Latest shared-workspace run: 26 suites / 1284 checks, including UI navigation.
  Radar and footsteps account for 849 checks, including 36 isolated navigation, 82 structure and 25 surface regressions.
  Read the build coordination block in `STATUS.md` before compiling shared sources.
- Experimental sonar: enabled by default, Ctrl+R toggle / Ctrl+T status;
  `access-radar on|off|status|dump|test`. Native map evidence and controller test:
  `docs/sonar-radar.md`. User reports improved fixed-axis radar but excessive cues.
  Current geometry cues and actor/terrain separation need live validation;
  `docs/kirandur-radar-comparison.md` records original-code findings. Ctrl+T
  now saves the local grid; use it at the exact spot of any failed passage.
- Latest radar request: study `../D2R_Navigation_Radar_Research.md` after the user
  confirms object filtering helps but remains cluttered. Read
  `docs/CurrentRadarAnalysis.md` and `docs/D2RNavigationResearch.md` first.
  The real native collision grids are already available; the missing layer is
  connected passage interpretation and cue grouping. External Legacy graphs use
  coarse 5x5 cells and are not exact body navigation. `navigation_grid` is an
  isolated test-only PoC (36 checks), not connected to runtime or audio.
  Installed object filtering and its physical clearance guards remain unchanged.
  Historical camp capture is shared in `tests/camp_grid.h`; no original flags.
  Next: validate live local connectivity and stable entrances before audio.
  Keep fixed stick directions, rear ray and 120 ms cues / 50 ms silence.
- Earlier work improved running cues and collision feedback.
  Controlled desktop checks found and corrected
  fractional-motion artifacts and a displaced player stamp causing false NO_PATH walls.
  `docs/computer-use-radar-check.md` records the failing reproduction and installed live checks.
  Ctrl+T includes `presenceOffset`: infer displacement only from a complete unique native shape;
  never clear arbitrary adjacent NO_PATH markers or use Path+0x10/+0x12 (targets) as its anchor.
  Short spaces, geometry cues and edge sliding still need controller validation.
- Audio queue timing is logged as `playback_ms`; it no longer changes geometry
  classification. The previous reaction policy in `docs/radar-running-timing.md`
  is superseded by `docs/radar-precision.md`.
- HD local footsteps: Ctrl+F / `access-footsteps on|off|status`. Repeated stationary walking
  events are suppressed; any native fractional movement restores them. Three whole-function
  fingerprints guard the hook. `[footsteps]` logs state changes/counters; `docs/footsteps.md`.
  The 23:19–23:22 reproduction confirms up-left wall sliding while held-up clearance stays
  blocked: 328 allowed footfalls, no stalled/read-failure/unresolved-foreign events. Footfalls
  stopped being recorded once the final position became fixed. Keep sliding audible; a blocked
  straight ray does not prove total immobility. The later 23:52 case likewise shows real world-X
  travel with up/right blocked; Ctrl+T now includes actual recent movement feedback.
  Five-second active-event summaries expose mode, movement and reset reasons for any different
  failure. Actual repeated-stall suppression remains unverified. User chose to test while
  playing; do not take control or mix desktop inputs with their movements.
- User prefers cue logging before audio capture. `docs/radar-event-log.md` describes automatic
  generated/queued/submitted/completed/cancelled records. Join event_ms + direction in the latest
  load session, using monotonic timing instead of the delayed SDK prefix. Eight navigation chains
  were verified in the latest run; submission is not proof of speaker output or navigation accuracy.
- Loader config `d2rloader/config/d2rloader.toml` must keep `text_locale = "esES"`,
  `audio_locale = "esES"`, `enable_console = true`.

## Rules

- Commit (English, short) and push to `origin main` after each fix the user asked for; the repo is
  the deliverable. **Never create GitHub releases** unless explicitly asked.
- PRISM is the only screen-reader library. No Tolk.
- No magic numbers: every offset/constant is named and documented with its source (layout JSON,
  log measurement, RE evidence). Derive from game data at runtime when possible.
- Never block the game's UI thread: all PRISM calls run on the speech thread
  (`screen_reader.cpp`); loader widget calls are cheap but must stay on the UI thread.
- Temporary files go to the session scratchpad, never into the repo.
- Keep `STATUS.md` current (section table, derived facts with sources, next step) whenever a chunk
  is finished; update `README.md` when user-facing behaviour changes.
- Record user confirmations in `STATUS.md`. The user retired `GEMINI.md` on 2026-09-15:
  ignore it and do not update it, overriding the earlier Gemini tracking instructions.

## Architecture (src/)

| Module | Role |
|---|---|
| `plugin.cpp` | D2RLoader entry points, console commands, per-tick order: input, `GameTextReader::Tick`, old `FocusTracker::Tick` when text does not own focus, UI messages, native probes |
| `ui_message_monitor.cpp` | Listens to the game's UI message bus (`PanelManager:OpenPanel`, `FrontEndNavigation:To*`, `CharacterCreate:Hover`, `OptionsPanelMessage:Set*`, `ScrollController:ManualScrollInput`); announces panels, navigation, class hover + description, modal prompts (native text, 120 ms after open) |
| `panel_state.cpp` | Open-panel stack (reset on `ToFrontEnd`: preloaded in-game panels like EmoteWheelPanel are invisible) |
| `focus_tracker.cpp` | Controller focus: polls the game's `FocusManager/FocusIndicator` rect (self-requeued `runOnUiThread` pump), matches it against `generated/panel_widgets.h`, speaks label/role/value/tooltip; dropdown popup rows and character rows from native text |
| `native_focus.cpp`, `skill_navigation.cpp`, `native_skill_navigation.cpp`, `native_cursor_gesture.cpp`, `skill_grid.cpp` | Native catalog identity fixes silent menu actions; D-pad selects once per full press/release and refreshes stock hover for matching skill descriptions. Native action legends/current bindings and branch counts: `native_controller_help.cpp`, `controller_text.cpp`, `native_progression.cpp`. Fourteen UI suites / 435 checks pass; user confirmed descriptions and binding/action hints. Live spend-point hint awaits an available point. `access-skill-grid on\|off`; `docs/skill-actions.md` |
| `widgets.cpp` | SDK WidgetService wrapper: handles cached for the plugin's lifetime (they are name paths), anchors applied against the 3840x2160 canvas for 0x0 parents, per-poll rect memo |
| `game_text_reader.cpp`, `ui_text.cpp` | Gameplay text context, lifecycle and testable reading policy; runs before the old focus reader and owns NPC/dialogue/tutorial focus while active |
| `ui_notifications.cpp` | Visible HUD notification rows and temporary informational prompts; text diffing and noninterrupting speech, without reading chat history |
| `native_memory.cpp`, `native_ui.cpp` | Guarded copies, native visible tree, class ancestry, actual focus, button captions/help/legends and equipment locations; evidence in `docs/native-ui.md` and `docs/native-tooltips.md` |
| `native_tooltips.cpp`, `interaction_reader.cpp` | One verified tooltip draw hook copies primary target/item/action descriptions; shared action/grid/equipment reader with selected tabs, coordinates, slot names, stale-description filtering and target-name HUD; `docs/native-tooltips.md` |
| `native_widgets.cpp` | Game widget objects via D2RCore.dll's slot table (PanelManager global, FindPanel, FindChildWidget, SetText); engine string reader; `TextAt(panel, path)`; probes |
| `control_state.cpp`, `settings_file.cpp`, `input_direction.cpp`, `display_modes.cpp`, `character_saves.cpp` | Toggle/carousel/dropdown values (Settings.json baseline + input direction), display modes for the resolution list, offline character saves (fallback) |
| `async_log.cpp` | Bounded background queue for runtime SDK logging; loader flushes disk per line, so UI callbacks only enqueue |
| `tab_reader.cpp` | Shared selected-tab state across visible panels, including controller overlay and unfocused stash; inventory consumes the same state |
| `screen_reader.cpp` | PRISM on a dedicated speech thread; `Speak(text, interrupt)` only queues |
| `footsteps.cpp`, `native_footsteps.cpp`, `footstep_service.cpp` | Pure stalled-motion policy, verified HD footfall hook, local identity/position reads, toggle and event-only diagnostics |
| `speech_strings.*`, `localization.cpp` | Plugin phrases (English/Spanish) and the game's own string tables via the SDK |
| `tools/` | CASC extraction, layout indexer, catalog generators (`gen_panel_widgets.py`, `gen_panel_titles.py`) → `src/generated/` |

## Hard-won facts (details and sources in STATUS.md)

- SDK widget handles are **name paths**, not objects; there is no text, focus or enabled getter.
  Native objects come from D2RCore's resolved game functions; widget name at `+0x8`, a text box's
  displayed text at `+0x88` (UTF-8).
- Dropdown popups show 13 recycled slots (`Items/ListItem0..12`): read each slot's `TextBox`
  text, do not infer options from rects.
- PRISM/NVDA `speak` blocks 300-800 ms → speech thread. Re-reading many rects per press through
  the loader also stalls; refresh only near the indicator after a settled scroll.
- Class selectors, hidden table rows (VoiceLanguage), the game version dropdown, save file v105
  header, and the text-language locale order all have documented handling in STATUS.md.
