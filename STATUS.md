# STATUS — Diablo II: Resurrected (d2r-access)

> Per-mod status ledger. Open first when resuming. Dashboard, not docs.

## Automatic quest announcements installed - 2026-09-16

Enabled by default; `access-quests on|off|status`. UI quest alerts now speak a
localized update notice even with no journal panel in memory. Stable, identified
objective changes speak the quest and its current text; SDK PrimaryGoalDone
events announce the main objective achieved, without assuming reward collection.
Events are coalesced, navigation stays silent, and dialogue/UI/focus gates defer
speech. Local-player/session checks and resets prevent stale announcements.
All 13 locales include the four new phrases. No new native offset or dependency.

Build succeeds and deployed the DLL with the game closed. **37/37 CTest suites /
9266 checks pass**, including quest-progress (52), quest-service (22), quest-log
(59) and speech-strings (5724). Build/install DLL SHA-256:
`BE482497AACA8F070C4372F7DC1B1A1B1DD8CCE3BB524EFE07C904CE139D87A9`.
Live NVDA confirmation pending. Exact objective/counter coverage when the
journal is unloaded remains a native-data research task; generic update/goal
announcements are implemented. `docs/quests.md` documents behavior and tests.

## Build repaired and installed - 2026-09-16

Added the six beacon messages to all 13 locale tables and their completeness /
format validation. Linked the real quest-log and localization implementations
into `native-ui-tests`. `./build.ps1` succeeds; **35/35 CTest suites and 9036
checks pass**, including native-ui (64), speech-strings (5568), quest-log (59)
and beacon (884). The build deployed the new plugin with the game closed;
build/install DLL SHA-256 matches:
`2B5A5EA6047D26D566A7200AD1597473E5C2A897C476FBDB4CF28BC578B332FA`.

This was the preceding build repair; automatic quest announcements were added
afterwards (above). Whole-level unloaded exit discovery remains unimplemented.

## Initial verification after interrupted work - 2026-09-16 (historical)

**The initial working tree did not build (fixed above).** Codex ran `./build.ps1`, then a
keep-going MSVC/Ninja build to reveal the remaining errors, with the game closed.
No implementation fixes were made during this verification.

- Plugin: `src/beacon_service.cpp` references six missing `Strings::Table`
  fields: `beaconCleared`, `beaconUnavailable`, `beaconNothing`,
  `beaconFollowing`, `beaconArrived`, `beaconLost` (C2039). The 13 locale
  headers and phrase visitor do not define these beacon messages yet.
- Tests: `native-ui-tests` fails with LNK2019/LNK1120 for
  `Localization::Text`, `QuestLog::ForWidget` and `QuestLog::Title`.
  Its target at `CMakeLists.txt:213` omits the new dependencies.
- CTest: **34 of 35 suites passed; `native-ui` could not run** because its
  executable did not link. New isolated suites pass: `beacon` (884 checks),
  `quest-log` (59), `speech-strings` (5334). These do not validate the plugin
  integration or live controller/NVDA behavior.
- The installed DLL and previous build DLL still match (SHA-256
  `3988BAA9067B57AF4AD27B9E36F0CEAB800302491760E7AB31F859744D65DD14`,
  timestamp 2026-09-16 19:12:48 local). No new plugin DLL was deployed.
- Latest game session began at 19:16:31. Its log records exit detections,
  including counts 1 and 2, but still uses the old `strCancel` language probe;
  no quest-watcher or beacon records were found. Exit audibility and specific
  dungeon transitions still need user confirmation.

Quest log naming is implemented, but **automatic quest progress speech is
not**: `quest_service.cpp` only records panel text for investigation.
Beacon guidance is implemented for a previously scanned nearby target; discovering
exits across an entire unloaded level remains pending (`docs/beacon.md`).
The earlier build/install notes below describe previous builds.

## Level exit loops (portal sound) - 2026-09-16

Built and installed; live NVDA/controller check pending. The user's `portal.ogg`
loops at every unit that leads to another area: level warps (unit type 5, named
after the destination level like the hover label), town/red portals, waypoints,
gateways, trapdoors, and level-named portals such as Duriel's. Same mixer as the
item loops, exits always voiced before loot, list refreshed every 500 ms. Ctrl+P,
`access-items exits-on|exits-off`. The scan now reports them as `Kind::Exit`.
Offline RE (type-5 layout, SubClass byte, Levels row offsets) with fingerprints
in `entity_layout.h`; `docs/level-exits.md`, corrections in
`docs/entity-units-re.md` §10. 32 suites pass (native-entities covers warp,
portal, waypoint and level-named exits; ground-items and service cover the loop
priority and the toggle). Live check: hear the loop at the Blood Moor exit and
at an open town portal; if nothing sounds, see the pending checks in the doc.

## Friendly NPC announcer - 2026-09-16

Built and installed; live controller/NVDA check pending. Periodic (500 ms)
poll of the scan's unit reader names the nearest friendly NPC once it is within
15 subtiles, with distance and screen direction (`Kashya, 12 metros, a la
derecha`), diff-gated with 3-subtile hysteresis; enemies/objects/items never.
Ctrl+N, `access-npc`, `radar.toml` `[npc]`. 32 suites pass (`npc-proximity`
34 checks, `native-world` 86 incl. [npc] config). `docs/npc-proximity.md`.

## Inventory/NPC follow-up — 2026-09-16

User confirms improvement and prefers the available-slot navigation while holding
equipment, but initially felt lost. Added one notice when an item is picked up
(item in hand; choose a space/compatible slot), and one when no longer held.
The SDK supplies cursor state; unknown reads stay silent. Installed; new notice
awaits live NVDA check. Full build and 31 CTest suites pass, including 15 SDK
cursor checks and 14 new interaction checks. The earlier radar test build failure
is resolved. Details and derived evidence: `docs/inventory-npc-followup.md`.

## Active build coordination - 2026-09-16 entity scan phase 2

- **Active build owner: none.** Right-stick entity scan reads monsters, NPCs
  and world objects (`native_entities.cpp`, 11 fingerprints verified at load)
  plus ground items. User confirmed live in the camp and Blood Moor (Kashya,
  Warriv, Charsi, stash, enemies). Hidden "evil force" helpers now excluded by
  the game's killable nameplate rule; that build is installed.
- **Sonar radar redesign installed** (one confirm-by-travel state machine per
  direction, replacing the memory/geometry/guidance classifiers; connected-
  passage scan is the only classifier). Four directions and the original
  sound design are kept: the review's eight-direction/new-sound-design
  attempt was tried live and reverted at the user's request after listening.
  A forensic analysis of the 18:10 live session (377 cues, 207 snapshots,
  zero code discrepancies) found the imprecision was model-level: narrowed
  the route search to 1 cell of lateral tolerance (was 3), raised openDepth
  to 10 (closing 7 for three-cell bodies), and capped clutter removal at
  5 cells; `[radar]` lines now log the left-stick position. No active owner.
  Built and installed; live listening of this build pending. `docs/radar-redesign.md`.
- **32 suites / 3697 checks pass** (radar: `radar-engine` 250, `radar-passage` 957,
  `radar-structure` 100, `radar-snapshot` 59, `radar-audio` 35, `radar-readout` 38;
  `native-world` 86 incl. volume config; `entity-radar` 44, `native-entities` 22).
- One agent owns compilation/deployment at a time. Record ownership here before
  building; other agents pause shared compiled-source edits until handoff.

---

**Last updated:** 2026-09-16 (automatic quest announcements deployed; 37/37 suites / 9266 checks pass; live validation pending). Quick guide: `AGENTS.md`.

## Identity

**UI update (2026-09-16):** user confirmed corrected skill descriptions and
controller action/binding readout after installation. Stock hover refresh keeps
descriptions synchronized; skill position/count is spoken. Fourteen UI suites /
435 checks pass. Main-menu native identity/value-reader fixes remain in place.
Controller Options intentionally has three tabs; keyboard bindings are the fourth.
`docs/controller-navigation.md`.

- **Engine / framework:** native C++ x64 (D2R.exe, patch 3.3) / **D2RLoader** (closed-source loader, v1.2.1-beta 2026-09-03, https://d2rloader.net) + its open **PluginSDK** (C++20 header-only, MIT, cloned at `../external/PluginSDK`, API v4).
- **Screen-reader transport:** plugin DLL → `LoadLibraryW(<pluginDirectory>\prism.dll)` + `GetProcAddress` (load in `D2RLoaderLoadPlugin`, never in `DllMain`).
- **Build command:** `.\build.ps1` (CMake 4.2 + Ninja + MSVC Build Tools 2022 14.44; vcvars64 from `C:\Program Files (x86)\Microsoft Visual Studio2\BuildTools`). POST_BUILD copies `d2rl-d2r-access.dll` + `prism.dll` into the game. PRISM v0.18.2 vendored in `third_party/prism/`.
- **Mod install path:** `D:\games\steam\steamapps\common\Diablo II Resurrected\d2rloader\plugins\` (global) — D2RLoader zip extracted into the game folder, launch `D2RLoader.exe`.
- **Run / test:** Steam D2R 3.3.93787 (app 2536520, Spanish) installed. D2RLoader 1.2.1-beta installed into the game folder (SHA-256 verified). Start NVDA, launch `D2RLoader.exe`. Loader config (`d2rloader/config/d2rloader.toml`): `enable_console = true` (Ctrl+` in-game), developer logs on with `widget_panels` + `extension_details`. Logs: `d2rloader/logs/d2rloader.log`. Offline/single-player only (online ban risk undocumented).

## Section status

| Section / feature | Status | Notes |
|---|---|---|
| Adapter + Speech (PRISM) | done | NVDA backend; all PRISM calls on a dedicated speech thread (`screen_reader.cpp`), `Speak` only queues. Console `access-say` |
| Mod localization (13 locales) | built and installed - live check pending | Locale headers, game-string language detection, beacon messages and quest announcements implemented; 5724 checks pass. |
| Quest log / automatic quest tracking | built and installed - live check pending; full objective coverage limited | 27-quest catalog, generic update/primary-goal alerts and identified live objective changes. Missing or stale panels use a generic notice. No guessed completion row/name mapping. `docs/quests.md` |
| Target beacon guidance | built and installed - live check pending | Scan then right-stick click / Ctrl+B wiring and route tracker present; 884 isolated checks pass. Whole-level unloaded exit discovery remains unimplemented. `docs/beacon.md` |
| Panel open/close announcer | done | `ui_message_monitor.cpp` + `panel_state.cpp`; localized titles from `generated/panel_titles.h`; modals: title + native prompt text + controller hint |
| Widget vocabulary from layout JSON | done | `game-data/` (gitignored) → `tools/gen_panel_widgets.py` → `src/generated/panel_widgets.h` (105 panels, PC variants; runtime-filled states like `gameVersionNames` followed) |
| Controller focus tracking (front end) | wip — return-to-menu fix awaits live check | Native focused object + full parent path resolves catalog controls first; rectangle coverage remains fallback. Fixes silent ribbon actions after gameplay. Value-control subclasses retain their reader. `docs/controller-navigation.md` |
| Control values | done | Sliders (runtime `Indicator`), toggles/carousels (Settings.json baseline + input direction; text language locale order), dropdowns (popup rows spoken from native slot text; picked index tracked) |
| Dropdown popups / scrolling lists | done | Popup slots `Items/ListItemN/TextBox` read natively (resolutions spoken "W por H"); scroll refresh only near the indicator after a settled scroll |
| Main menu / character select | done | Character rows from native `Name` + `Level&Class` (save-file fallback), legend "A: Jugar, X: Borrar", delete modal prompt read natively; version dropdown states Clásico/Expansión/Reign of the Warlock |
| Dynamic text (modals, popup rows) | done (front end) | `native_widgets.cpp`: D2RCore slot table → game FindPanel/FindChildWidget; text box text at +0x88. Pending: ButtonWidget labels (modal Confirm/Dismiss probed), legends, runtime tooltips |
| Performance | done — improvement confirmed by user | Scoped native reads and immediate capture on selection change installed; user reports improvement. `docs/inventory-npc-followup.md` |
| In-game text (tutorials, auto hints, NPC dialogue, area names) | wip — Charsi dialogue observed in logs | Native visible-tree/focus reader at 30 Hz, full speech text, diff-gating; lifecycle starts after LocalPlayerReady. No SetText hook needed. `docs/test-ingame-text.md` |
| NPC menus / dialogue topics | wip — Charsi menu/topics observed in logs | Actual focused ListItem/Button text, heading + initial option, option-only navigation, return from dialogue; hidden templates excluded. Log includes introduction, gossip, trade/repair and cancel; explicit dialogue validation pending |
| Area entry banners | wip — camp entry confirmed | User confirmed hearing Campamento de las Arpías; boundary/dungeon checks remain |
| System notices / G graphics restriction | done — repeated G delivery confirmed by user | Native message identity rearms a newly delivered identical warning; stationary/older notices remain silent. `docs/test-ingame-text.md`, `GEMINI.md` |
| Internal town landmarks | wip — investigated, not implemented | Actual camp/NPC/object records identified; runtime world positions still need deriving. No kitchen/square district labels found in reviewed tables. `docs/map-landmarks.md` |
| Native world / local map | wip — camp and Blood Moor runtime confirmed | 18:19–18:29 session retained terrain, entered level 2; movement mask 1C09. Direct room reads, 19 fingerprints. Full-map coverage unverified. `docs/sonar-radar.md` |
| Entity scan (right stick) | done - confirmed live (camp NPCs, stash, Blood Moor enemies, items); distance calibration pending | Push = nearest visible entity in that screen sector + distance; repeat = next nearest, wraps. Terrain-layer line of sight. Items verified live (Blood Moor dagger). Unit layout from offline RE: `docs/entity-units-re.md`. Ctrl+Numpad 8/4/6/2, Ctrl+E, `access-scan`. `docs/entity-scan.md` |
| Friendly NPC announcer | built - live check pending | Nearest friendly NPC spoken once when in range (15 subtiles, 3 hysteresis), distance + screen direction, no interrupt; silent in menus/dialogue/UI speech. Ctrl+N, `access-npc`, `[npc]` in `radar.toml`. `docs/npc-proximity.md` |
| Sonar radar | wip — 2026-09-16 redesign installed, incl. 18:10 forensic fix; live listening pending | One confirm-by-travel state machine per direction; connected-passage scan is now the only classifier, with 1-cell lateral tolerance (was 3), openDepth 10/close 7 (was 8/5), clutter removal capped at 5 cells (was up to 20), and `stick=` logged per cue. Four screen-cardinal directions and the original tin/tock sound design are kept — an eight-direction/new-sound-design attempt was tried live and reverted after the user listened. `docs/radar-redesign.md` (current model); `docs/sonar-radar.md` and its follow-ups are superseded history. |
| Connected navigation research | done locally - runtime integration built | Native body graph's bounded directional passage query (`ScanPassage`) is now the radar's only classifier, driving all four directions. No persistent doorway IDs/full-map guidance. `docs/D2RNavigationResearch.md`, `docs/radar-connected-passages.md` (historical), `docs/radar-redesign.md` (current use). |
| Stalled local footsteps | wip - second sliding case verified; suppression pending | 23:52:33/40 grids show up/right blocked with real travel along world X; +11.008728 then -9.091965 X in five-second intervals. No stalled decisions. No filter change; Ctrl+T now distinguishes recent movement from straight-ray blockage. Full stationary suppression remains unverified. `docs/footsteps.md` |
| Local computer use | done — initial live check | Python stdlib capture + bounded Windows inputs; menus, scroll, Ctrl+T and four movement directions verified with NVDA active. F9 up, F10 right, F11 down, F13 left assigned through game UI. Twelve mocked-input tests pass. `tools/computer_use/README.md`; no audio capture/analog controller emulation |
| Radar cue delivery log | wip — log format updated for the redesign | `[radar-audio]` still retains ID/origin through queue, submission, completion, cancellation or drop (no retries now); `[radar-direction]`/`[radar-state]` replace the removed opening-hold/wall/guidance/reference lines. Live validation of the new log pending. `docs/radar-redesign.md` (current fields), `docs/radar-event-log.md` (superseded) |
| Radar / footstep automated tests | done (local) | Six radar suites / 1439 checks against the confirm-by-travel engine (state machine, corridor stability, recorded camp/return grids, snapshot RLE, audio, readout), plus footsteps. Expectations derived from geometry, named constants and captures; timing printed, never asserted. |
| Automated text tests | done (local) | Nine CTest suites, 255 checks: text/notifications, guarded memory/tree and cell queries, gameplay lifecycle, tooltips, action help, equipment locations, delayed item speech without coordinates, shared tabs and a deliberately blocked SDK log sink |
| Highlighted NPC/object labels | done — stash and Charsi confirmed by user | Shared primary tooltip capture plus visible target-name HUD. `docs/native-tooltips.md` |
| In-game panels (inventory, skills, stash) | wip — basic selection and fluency confirmed | Shared item/grid archetype, native row/column; shared tab reader covers controller strip and unfocused stash changes (tab speech unconfirmed); skills/full inventory operations remain pending |
| Inventory actions / equipment slot names | wip — placement orientation installed | User confirms available slots read while holding equipment and prefers this navigation. Added SDK-driven pickup/release context notices; live notice/repair check pending. `docs/inventory-npc-followup.md` |
| Attribute allocation / skill-tree reading | wip — descriptions/actions confirmed by user | Stock hover refresh fixes repeated skill descriptions. Current binding, available action buttons and position/count precede the full description. All 30 live Barbarian class cells are reachable in regression tests. Live log distinguishes lower-row skills and reads Porrazo's A binding/X reassignment; user confirms it works. Zero points prevents live spend-action validation. `docs/skill-actions.md` |
| Item readout | wip — blacksmith axes and coordinate suppression confirmed | Full rendered descriptions, stale cell rejection; occupied grids wait for the stock tooltip and read it without row/column. Empty/unknown cells keep coordinates. Full inventory/storage operations remain unverified |
| Nearby ground item speech | wip — native live detections observed | Localized base name/quality and gold amount within 3 subtiles; once per approach, rearm beyond 6; Ctrl+I. Grammar tags removed after live bolts/arrows evidence. Full generated equipment names remain outside this reader. `docs/ground-items.md` |
| Level exit loops (portal) | prior build installed - live detections logged; listening check pending | Type-5 warps + portal/waypoint/gateway objects as `Kind::Exit`, `portal.ogg` through the item mixer, Ctrl+P. Latest 19:16 session records nearby exits; audible placement and dungeon transitions remain unconfirmed. `docs/level-exits.md` |
| Spatial item loops | wip — audibility confirmed; quieter landing-only follow-up installed | Six supplied loops; fixed screen axes, distance, rear pitch 0.94; Ctrl+L. User confirms sound but requested lower level. Near gain 0.4 and pile limit 0.5; native falling mode excluded, no landing timer. 100 audio checks plus native landing transition regression. `docs/ground-items.md` |

## Derived facts (source: PluginSDK headers, path:line relative to external/PluginSDK/include/D2RLPlugin)

| Fact | Value | Source |
|---|---|---|
| Required exports | `D2RLoaderGetPluginInfo`, `D2RLoaderLoadPlugin(const PluginContext*)`, `D2RLoaderUnloadPlugin` | lifecycle.h:7-9 |
| Plugin flags | `PluginFlags::Client | NativeHooks` for the primary tooltip draw hook | lifecycle.h:13-20 |
| Service lookup | `ctx->QueryService(ServiceId::X, XServiceV1Version, &svc)` | context.h:158 |
| Threads | UI thread vs game thread; **no per-frame callback** → self-requeue `runOnUiThread` as pump; all widget calls UI-thread only | threads.h:40-41, widgets.h:62 |
| Widget API (all of it) | `findPanel`, `findWidget(parent,name)`, `getWidgetRect`, `setWidgetVisible`, `setWidgetEnabled`, `dispatchUiAction` — **no tree enum, no text, no focus** | widgets.h:55-60 |
| Only game-UI event | `UiMessageEvent{targetHash, commandHash, target, command, text}`; stock panels use `PanelManager:TogglePanel/ClosePanel:<name>` | shared_events.h:91-131 |
| Localization | `getStringById(id 0-65535)`, `getStringByKey("strCancel")` (key without `@`), two-call BufferTooSmall | localization.h:35-40 |
| Input | `registerAction(ActionRegistration{logicalId, displayName, defaultPrimary…})`, one modifier max; **not fired in text edit / pre-game** | input.h:187-229 |
| Hooks | `ctx->InstallInlineHook(rva, expected, size, fn, &orig)` — RVA only, **no AOB scanner** (write own over `ctx->exeBase`, context.h:132) | hooks.h:10-42, context.h:318-348 |
| Gameplay events | GameJoined, GameLeft, LocalPlayerReady, ActChanged, LevelChanged, PlayerResurrected, PlayerLevelChanged, QuestCompleted | lifecycle_events.h:32-41 |
| Inventory | `getLocalPlayer`, `getCursorItem`, `getEquippedItem`, `forEachInventoryItem` → `ItemInfo` (code, x, y, quality, ilvl, qty, sockets, affix ids) | inventory.h:403-419, item.h:191-217 |
| Data tables | raw compiled rows (`rows`+`rowSize`, no accessors), 75 TableIds, game thread only | data_tables.h:120-235 |
| Debug channel | `RegisterConsoleCommand` | console.h |
| CASC storage | Steam static-build CASC (`Data\.build.config` + `NN-XXXXXXXX.data`, no `.idx`); CascLib opens the **game root**; TVFS root carries real names (175,812 files), no listfile | tools/casc-extract run 2026-09-14 |
| Layout files | `data:data\global\ui\layouts\` — root (204) + `Controller\` (83), `Controller\Console\` (26), `Controller\PC\Handheld\` (11); `X.json` = SD, `XHD.json` = HD; JSON5-ish (trailing commas); `@Key` = string table key, `$Name` = `_profile*.json` style | extraction |
| String tables | `data:data\local\lng\strings\ui.json` (2131 records `{id, Key, enUS, esES, …}`, UTF-8 BOM) holds `@str…` and `@OptionsHD`; `ui-controller.json` overrides for controller | extraction |
| Menu layout names | MainMenuPanelHD, MainMenuButtonRibbonHD, MainMenuLinksHD, TitleScreenPanelHD, LogoMenuPanelHD, CharacterSelectPanelHD, CharacterCreatePanelHD, SettingsPanelHD, Gameplay/Video/Sound/LootFilter/OnlineGameOptionsPanelHD, GammaCalibrationPanelHD, KeyBindingMenuHD, PauseLayoutHD, MiniMenuPanelHD, PlayerInventory{Original,Expansion}LayoutHD, SkillsTreePanelHD, WaypointsPanel{Original,Expansion}HD | extraction |
| Focus model in layouts | Controller layouts carry `navigation` (678: next/prev/up/down/left/right widget names or `panelBoundary`), `defaultWidget` (97: initial focus per panel), `focusIndicatorFilename` (521), `focusOnMouseOver` (100), `generateFocusLinks` (13). 125/329 layouts use `basedOn` inheritance (indexer merges it). Engine keeps a real focused widget → phase-2 hook target = its setter | tools/layout_index.py 2026-09-14 |
| Settings tabs | Layout has four tabs, but native setup `0x147CBC0` only adds Controls outside controller mode (`0x077E10`). Controller options are under Gameplay/Juego; KeyBindingMenu is for keyboard bindings | layouts + `docs/controller-navigation.md` |
| Main menu (controller) | `MainMenuPanelHD` QuickPlayButton "JUGAR" → `MainMenuHD:Play`; ribbon Opciones→`PanelManager:OpenPanel:SettingsPanel`, Vídeos→`FrontEndNavigation:ToCinematics`, Créditos→`OpenPanel:CreditsModal`, Salir→`FrontEndNavigation:ToExit` | index |
| D2R.exe code | `.text` is Arxan-encrypted on disk; the plugin dumps the live `.text` once at first `ToFrontEnd` (`src/code_dump.cpp` → `plugins/d2r-access/D2R.text.bin`, 30,076,928 bytes, entropy 6.5 = real code). **Live layout ≠ disk**: live `.text` is 0x1CAF000 (disk 0x15CB6E0) and every later section is at `disk_rva + 0x6E3000` (IAT-call vote 76:1); on-disk `.pdata` is stale for the live code. `re/scripts/rebuild_pe.py` → `re/D2R.rebuilt.exe` (file offset == RVA); Ghidra program `D2R_live` in `re/ghidra`, seeded with 55,944 functions (`SeedFunctions.java`), analysis log `re/ghidra/analysis-live.log`. Notes: `re/notes.md` | Opus RE passes 2026-09-14 |
| Widget JSON reflection table | Every layout JSON field has a 0x30-byte record in `.data`: `+0x08 memberSize, +0x18 name, +0x20 type (0 int,1 bool,2 float,3 string,4 struct,5 enum,6 msg-hash), +0x28 memberOffset`; 3,904 records (`re/out/schema*.tsv`). Static: SliderWidget `increment +0x5B0`, `onUpdateMessage +0x590`, `isHorizontal +0x588`; Panel `defaultWidget +0x90`; Modal `confirmMessage +0x1A0`/`denyMessage +0x1C0`; widget `name +0x0`, `rect +0x70`. **`isToggled` (rec 0x2400418), `states` (0x2400FE8), `text` (0x2400118), `tooltipString` (0x23FFCC8) get type/offset filled at runtime** → the plugin can read those records live (rec RVA + 0x6E3000 from exeBase) instead of hardcoding. Engine string type = 0x28 bytes | re/notes.md §7 |
| Slider handle | `SettingsSliderWidget` creates a runtime child `Indicator`; percent = (indicator.x - slider.x) / (slider.width - indicator.width) | run 8 probe 2026-09-14 |
| Value messages | Controls' `onUpdateMessage` (`OptionsPanelMessage:SetMasterVolume`…) DO reach the listener but with empty text; panels fire all of them on init | run 7 |
| Focus vocabulary in D2R.exe .rdata | `FocusManager` 0x172E0C0, `FocusLink`, `FocusNavigation`, `FocusIndicator`, `panelBoundary`, `FocusableWidget` 0x1711EA0, `SetFocusIndex`, input actions `focusUp/Down/Left/Right`, `navigateUp/…`; build paths `D2Client\src\Game\ControllerInputHandler.cpp`, `UI\Widgets\SettingsPanel.cpp` | re/out/strings.tsv |
| Runtime message stream (controller) | Main menu = `MainMenuPanel` (exclusive) + `MainMenuButtonRibbon` + `MainMenuLinks` + `CharacterSelectPanel` open together; D-pad moves emit **no** message; `CharacterCreate:Hover/Unhover/Select <ClassKey>` (text is a string key: "Barbarian"→"Bárbaro"); `CharacterSelect:SelectAct` every frame; `MainMenuHD:UpdateCharacterInfo/CheckUpdateLinks/UpdateButtonLegend`; modals: `OnlinePlayModeModal`, `ConfirmationModal`, `DismissableModal` (text widget `Prompt`, dynamic) | run 3, 2026-09-14 |
| WidgetService rect semantics | `getWidgetRect` is local to the parent. The layout `anchor` (fraction of parent size) is **already applied by the SDK when the parent has a nonzero size**, but root panels (and the screen) report 0x0, so anchors at those levels collapse → apply them against the HD layout canvas **3840x2160** (measured: QuickPlayButton anchor 0.5, local x -710 → indicator 1211; CinematicsModal anchor y 0.1 + Option0 y 170 → 386). Controller/Console|Switch|Handheld variants do not exist on PC → excluded from the catalog | runs 5-6 diagnostics 2026-09-14 |
| Popup list widgets | The game clones `ItemTemplate` into **`Items/ListItem0..N`** (one per option, 624x40) which move with the list: the clone covering the `FocusIndicator` is the option index and the clone count the option count (`FindPopupRow`). `Items`/`Items/Selector` report screen positions only; the scroll bar's `Indicator` thumb updates lazily (only its extremes were ever read) | log 2026-09-15 04:52 |
| Dropdown popup scrolling | Only the D-pad / left stick move the highlight; past the last of the 13 visible rows the list scrolls under a still indicator (each press emits `ScrollController:ManualScrollInput`). Press-direction tracking remains only as a logged fallback. The game's resolution list holds every mode Windows reports (640x480 first, 18 here); GDI and DXGI enumerate the same set | log 2026-09-15 |
| Character saves | `<Saved Games>\Diablo II Resurrected\*.d2s`, file version **105** (D2R 3.3): magic bytes `55 AA 55 AA` @0 (0xAA55AA55 little-endian), version @4, size @8, checksum @0xC, class @0x18, level @0x1B, last-played Unix time @0x20, name (16 bytes) @0x12B. Read by `character_saves.cpp` for the select list rows (`CharacterTemplate` clones, texts set at runtime), most recently played first | hex dump 2026-09-15 |
| Version dropdown | `CharacterCreatePanelHD.json` `GameVersionDropdown` declares `states: [""]`; the real list is the root field `gameVersionNames` = `@Classic, @Expansion, @RotWLong` (generator now follows it) | layout |
| Delete character | With a controller the X face button deletes (legend `MainMenuLegend` is filled at runtime; no UI message precedes the `ConfirmationModal`). The plugin says "X: Borrar" after a character row and "Borrar <name>" with the modal when an X press preceded it (`InputDirection::LastFaceButton`). Mouse path: `DeleteCharacter` button (tooltip `@strDelete`) → `CharacterSelect:ShowDeleteCharacterModal`. `strdeletecharmessage` = the hold-to-delete hint | layout + user 2026-09-15 |
| Text language storage | `Settings.json "Language"` (not `TextLanguage`) holds the locale's index in the string-table order `enUS, zhTW, deDE, esES, frFR, itIT, koKR, plPL, esMX, jaJP, ptBR, ruRU, zhCN` (`local/lng/strings/ui.json`), which differs from the carousel's `states` order; mapped in `control_state.cpp` (`StoredValueOrders`). `SetTextLanguage` applies immediately (`AcceptChanges` + front-end reload) | Settings.json 2026-09-15 |
| Hidden table rows | `SoundOptionsPanel` `Row VoiceLanguage` is hidden on this install (one voice pack), so `Row TextLanguage` is laid out on its slot and both carousels resolve to the same rect. The SDK has no visibility getter; on a coverage tie within a panel the later sibling wins (a stacked layout only skips hidden rows) | log 2026-09-15 03:57 |
| Class selectors | `CharacterCreatePanel` `HDCharacterSelectorWidget` Amazon…Warlock resolve under the focus indicator but carry only the panel title as label; the game announces them itself via `CharacterCreate:Hover`, so the tracker skips that type (`MessageAnnouncedTypes`) | log 2026-09-15 |
| Class descriptions | `CharacterCreatePanelHD.json` text boxes `ClassTitle`/`ClassDescription` are filled at runtime from `strAmazonDesc`, `strAssDesc`, `strBarbDesc`, `strDruDesc`, `strNecroDesc`, `strPalDesc`, `strSorcDesc`, `strWarlockDesc` (ui.json); spoken after the class name on `CharacterCreate:Hover` | extraction 2026-09-15 |
| Preloaded in-game panels | Right after `OpenExclusivePanel TitleScreenPanel` the game opens `ShowItemsPanel` and `EmoteWheelPanel` (never closed); their buttons are invisible in the front end but overlapped CharacterCreate's class area (Emote6 "¡Adiós!" at coverage 0.51). `FrontEndNavigation:ToFrontEnd` now resets the panel record | log 2026-09-15 03:58 |
| SDK widget handles | `WidgetHandle` = plugin-owned name PATH, not a game pointer (README "Existing Widgets"; widgets.h:20). Every `getWidgetRect`/`findWidget` re-walks the tree inside the loader: measured up to ~50 ms per call and one-second UI stalls when a hundred rects were re-read (pump log 2026-09-15). Handles are cached for the plugin's lifetime; rects re-read only near the indicator after a settled scroll | log + README |
| Native widget access | D2RCore.dll 1.2.1-beta slot table (offsets from the DLL base, filled at startup): **0x5AA340** → address of the game's `PanelManager*` global, **0x5AB880** `FindPanel(mgr, name)`, **0x5AB610** `FindChildWidget(parent, name)` (recursive below parent), **0x5ADA48** `TextBoxWidget::SetText(widget, {ptr,len}*)`; D2R live RVAs are logged at load ("[native] D2R live RVAs"). `src/native_widgets.cpp` | Opus RE 2026-09-15 (D2RCore call sites 0x2AE3B3, 0x2AE3C2, 0x2A6FB6, 0x2A7260) |
| Engine string | 0x28 bytes: `{char* data; size_t size (bytes, no NUL); size_t capacity | bit63 = non-owning; char inline[16]}`; `data` always valid (points at the inline buffer or heap). D2RCore 0x699509 (ctor), 0x314622 (dtor), 0x3F911B (size≠cap). 1-byte chars (UTF-8 vs ANSI unconfirmed) | Opus RE |
| Reflection records (corrected) | Stride 0x30, **base = the qword holding the name pointer**: `+0x08 type, +0x10 memberOffset, +0x20 memberSize`. Live RVAs: `name` 0x23FE4E0 (string, off 0x0), `rect` 0x23FEBA0 (struct, off 0x70), `text` 0x2400130 / `tooltipString` 0x23FFCE0 / `states` 0x2401000 / `isToggled` 0x2400430 are name-only on disk (runtime fill unconfirmed). Offsets are relative to a reflected sub-object after the vptr (widget+0 is a vtable), shift F unconfirmed. `re/out/schema2.tsv` | Opus RE |
| Widget text offsets | Probe 2026-09-15: every widget's **name at +0x8**; a text box's **displayed text at +0x88** (TextBox rows "1280x720", character row Name "yarkey" / Level&Class "Bárbaro de niv. 1", modal Prompt "Han cambiado los ajustes…"), UTF-8; a second copy at +0x200. Popup slots: `DropdownListContents/Items/ListItemN/TextBox`; character rows: `CharacterSelectPanel/CharacterList/ListItemN/{Name,Level&Class}`; modal: `ConfirmationModal/Frame/Prompt`. Hidden template rows report +0x50/+0x51 = 0, visible clones 1 | log "[native] probe" |
| Speech cost | PRISM/NVDA `speak` is synchronous: 300-800 ms per call measured on the UI thread → all PRISM calls now run on a dedicated speech thread (`screen_reader.cpp`); Speak() only queues | slow-tick log 2026-09-15 |
| Widget flags | `widget+0x50` / `+0x51` are bools with vtable setters (+0x48/+0x50); `+0x51` tested as "panel up/active" (D2RCore 0x2C35FC) → likely visible/enabled — candidates for disabled-control detection | Opus RE |
| Button → panel wiring | `ButtonWidget.fields.onClickMessage = "PanelManager:OpenPanel:SettingsPanel"`, `textString = "@OptionsHD"` (MainMenuButtonRibbonHD.json:43) | extraction |
| Radar redesign (2026-09-16) | One confirm-by-travel state machine per direction replaces the memory/geometry/guidance classifiers; connected-passage scan (`ScanPassage`) is the only classifier. Four directions and the original tin/tock sound design are kept — an eight-direction/new-sound-design attempt was tried and reverted after user listening. 18:10 forensic analysis (377 cues, 207 snapshots, zero code discrepancies) found the imprecision was model-level: lateral tolerance 3→1 cell, openDepth 8→10 (close 5→7), clutter cap 20→5 cells, `stick=` added to `[radar]`; parameters, log format and removed classes fully listed | `docs/radar-redesign.md` |

## In-game native UI evidence (2026-09-15)

Full derivation: `docs/native-ui.md` (existing live Ghidra program + prior runtime log).

- Widget: parent `+0x30`, enabled/list availability `+0x50`, visible `+0x51`, child pointer/count `+0x58/+0x60`. Source: game `0x855C90`, `0x8562F0`, `0x886420`, `0x886840`.
- Native class ancestry: vtable `+0x58` getter → descriptor name `+0x8`, base `+0x10`; source `0x851030`, `0x89F760`.
- **Actual controller focus**: PanelManager `+0xD0` → focused panel `+0x188`, focused widget `+0x190`; source `0x846020`, `0x868230`, `0x876CA0`. This is separate from the ECS/game-world focus research above.
- **Button label**: embedded TextBoxWidget `+0x818`, displayed text `+0x8A0`; stock modal code `0x8496CA–0x8496D6` calls SetText on it. Used by the new in-game native reader; front-end modal-label integration remains unchanged.
- Extended traversal requires the recorded FindChild/SetText RVAs and verified child-vector machine code. New builds fail closed. The tree reader needs no hooks; transient target/item descriptions use one verified draw hook, without additional DLLs or gameplay changes.
- **Primary tooltip**: `TooltipsPanel +0x168/+0x170` is a contiguous vector, stride `0x2E8`, body/footer strings `+0/+0x28`. Draw `0x880550` destroys/clears it every frame; single-record draw `0x8DA7E0` is hooked after byte verification. Live record zero was `Tu alijo personal`; record one only U+E013. `docs/native-tooltips.md`.
- **Inventory selection**: controller x/y `+0x544/+0x548` (`0x2A89C0`); grid width/height `+0x628/+0x62C` (`0x2C5FA0`); TabBar selected index `+0x16C4` (`0x878D30`), runtime `TextN` label children.
- **Icon button help**: FocusableWidget registration `0x875CB0`, live reflection records `0x2535D90`/`0x2535E20`: tooltipString `+0x88`, buttonLegendString `+0x310`. Both contained `Soltar oro` on live gold_button while its embedded caption was empty. Applied only to known button classes; toggles retain the value reader.
- **Equipment location**: InventorySlotWidget registration `0x2CA5F0`, live record `0x236C530`: int32 `location +0x5D8`. Ten live slots match stock layout values 1–10 (head, neck, torso, right/left weapon, right/left ring, belt, feet, gloves). Full mapping in `docs/native-tooltips.md`.
- **Grid occupancy query**: `0x2A89C0` calls vtable `+0xC8` with selected x/y; live grid vtable `0x1CF3800` points to `0x2C49F0`. That getter resolves the widget owner (`0x2A7810`), page `+0x630`, and cell (`0x386A00`). Guarded UI-thread query distinguishes occupied/empty/unknown; no pointer is retained. Blacksmith log showed tooltip delay ~350–390 ms, so occupied cells now wait without speaking early coordinates. `docs/native-tooltips.md`.

- **SDK runtime logging:** live context `logInfo` → D2RCore `0x3CF110` → `0x3D14F0`; `0x3D171D` calls `FlushFileBuffers` (IAT `0x5125C0`) after every line. `async_log.cpp` moves that work off UI callbacks.
- **Controller tabs:** live `ControllerOverlay` contains `UISwitcherTabs` outside focused inventory; `+0x16C4 = 3` read `Inventario` while `+0x544 = 2`. Stash `SelectTab` also fires without moving player-inventory focus. Shared `tab_reader.cpp`; full evidence in `docs/navigation-latency.md`.

## References

- **Progression widgets:** `CharacterStatLevelUpButtonWidget` companions
  `<stat>Title` / `<stat>Stat`, `CombinedPointsInfo`; real skill buttons under
  `SkillButtonContainer` inside `TabWidget`, named `SkillButton<tree> <x>,<y>`.
  Live raw skill tooltips store reversed lines and U+E07E font-size commands;
  the existing render hook supplies their text. Sources and validation:
  `docs/progression.md` (22:27–22:34 controller navigation).

- **Skill cursor release:** `0x234820` starts another snap on the `+0xB0`
  held-to-released transition; stock `0x2347B0` only resets position/velocity.
  Clear `+0xB0` and interpolation `+0x68`, including one cleanup update per
  cursor after release. Complete-function fingerprints already guard these fields.
  `docs/controller-navigation.md` (23:47 log and 2026-09-16 RE).

- `docs/research-roadmap-chatgpt.md` (+ `-extended.md`/`.docx`, 744 lines) — user-supplied research roadmap (ChatGPT, 2026-09-14). No offsets/RVAs anywhere; focus tracking is its declared biggest unknown (extended §4.5, line ~171): experiment list + external repos to mine for later phases (MapAssist / D2RMH for unit tables & map data, diablo-access, D2RL-Plugins). Its C#-companion-via-IPC idea is superseded: PRISM speaks in-process.

- User-supplied `../D2R_SONAR_RADAR_IMPLEMENTATION_SPEC.md` v2 drove the first radar prototype. Native map evidence, parameters, controls, limits and controller test: `docs/sonar-radar.md`.

- **World source:** Core `0x372C44/4D` calls slots `0x5A8CF8/0x5A8D10` (index/player lookup). D2R lookup `0x9A480`; path `+0x38`, collision X/Y **uint16 `Path+2/+6`** (`0x341A20/30`, used directly by collision caller `0x341A60`), body pattern `+0x5C` (`0x341870`). **Correction:** `Path+8/+0xC` / getters `0x341C10/20` are projected positions, not collision coordinates; the 17:32 log exposed this earlier error. Room collision pointer `+0x38` (`0x2EFB30`); neighbors `+0/+0x40` (`0x2EFDE0`). Grid origin/dimensions `+0/+4/+8/+0xC`, uint16 cells `+0x20` (`0x366250`); **runtime movement mask `Path+0x64`**, passed by wrapper `0x3C5610` to `0x364700`; setter `0x342740`. The old `0x1C09` was from a destination query, not this runtime field. Full-act residency not established.

- **Room lifetime / orientation:** logical list `Level+0x10 → RoomEx+0x48`, rectangles `RoomEx+0x60..0x6C` (`0x3614C0`); active instance `RoomEx+0x58` set/cleared by `0x361290/0x361260`; unloading frees collision (`0x2F0110 → 0x363A10`). Room metadata does not establish persistent detailed collision. Projection `0x334DB0`: X down/right, Y down/left, fixed stick up/left/right/down = world (-X,-Y)/(-X,+Y)/(+X,-Y)/(+X,+Y). Full evidence: `docs/sonar-radar.md`.

- **Fractional motion:** native `0x382AA0` forms 16.16 cell centers; `0x3818E0` stores packed X/Y at Path+0/+4. Both fingerprinted. Low words feed the actual-motion readout; high words still drive collision. Seven new regression checks; live `[radar-motion]` records confirmed after the 20:43:49 load. `docs/sonar-radar.md`.

- **Local presence:** `Path+0x60` footprint mask (`0x341860`); setter/resetter `0x363CF0/0x3636D0` add/remove NO_PATH `0x1000` at the centre (pattern 1) or cross (2); pet patterns 3/4 use `0x2000`. A complete player stamp can be displaced one cell from Path: controlled 20:47 false down block at (4463,4614), stamp (4464,4614), followed by successful down movement. Infer an offset only from one complete overlapping native shape; reject unknown/incomplete/ambiguous or monster/missile-overlapped shapes. Copy-only removal preserves other obstacles. Ctrl+T adds `presenceOffset`; live (0,1) and (-1,0) corrected at 21:04–21:06. Path+0x10/+0x12 are targets (`0x34120B`, `0x380A70`), not an authoritative stamp anchor. `docs/computer-use-radar-check.md`.

- **Creature versus terrain:** `0x341D70` restores footprint `0x100`, `0x341600` changes a monster to corpse footprint `0x8000`; both now fingerprinted. Actor-filtered estimates suppress wall/exit chatter but original movement collision gates every opening. Ctrl+T adds obstruction flags, velocity and occupied cells. Original Kirandur `0x1403f5de0` compares world hit points; its two-frame counter is classification retention, not unconditional audio. Full evidence: `docs/kirandur-radar-comparison.md`.

- **HD footfall:** dispatcher `0x7D9800` event 1 calls `0x7D9390`; resolver `0x769B50` validates the ECS entity generation and returns its unit. Suppression returns the already-supported null sound handle. Three complete function fingerprints matched live code before hook implementation. Unit mode +0xC, path +0x38, packed 16.16 X/Y at Path+0; only verified native local player and walk/run/town-walk modes qualify. `docs/footsteps.md`.

- **Precision investigation (2026-09-16):** read-only native trace/body/projection checks plus `0x340E70/0x343920` alternate collision-tested directions explain wall sliding. The 07:05–07:18 log has 879 generated cues, including unchanged-cell closed/open pairs 125 ms apart driven by speed. Removed that classifier; geometry now drives all cues. `docs/radar-precision.md`.

- **Running follow-up (2026-09-16):** the 08:00 log has 148 cues, 17 interrupted buffers and six retries. The saved 21:06 camp grid reproduces hit (4466,4628), depth 9 at player (4473,4621): an isolated object hits the side of the native body. AHC C# line tracking informed the replacement; only real observations advance confirmation and submitted cues do not retry. `docs/radar-surface-tracking.md`.

- **Object filtering (2026-09-16):** the 08:26 load generated 87 geometry cues, zero retries; user heard no improvement. A separate structural map now removes bounded wall/object components only after a full-body connectivity proof. Captured camp post is filtered, neighboring wall preserved. Synthetic regression: old engine 48 cues / new engine 0. `docs/radar-object-filtering.md`.

- **Navigation research (2026-09-16):** audited d2mapapi_mod `f61d052`, D2RMH `32d55b8`, MapAssist `6b26303`. Legacy path bytes encode a coarse 5x5 graph with wall-bit seam tests, not exact native body movement. Prefer our existing direct grids; isolated PoC confirms local connectivity. Automap runtime function and full unseen-level coverage remain unproved. `docs/D2RNavigationResearch.md`.

- **Ground item evidence:** `0x2EFD90` / `0x34B4A0` expose the actual room unit chain
(`Room+0xA8`, `Unit+0x160`); `0x266690` reads static item path X/Y at +0x10/+0x14.
Items/type tables come from the active native bank; 14 complete function guards
match the decrypted executable and live runtime. Full table/layout provenance,
localized name cleanup and source level measurements: `docs/ground-items.md`.

## Next step

Validate automatic quest alerts with NVDA while the journal is closed: expect
one update notice, and a main-objective notice after the related dialogue closes.
Inspect `[quest] announce` and `watch` to establish whether native objective text
remains available; check a live Den of Evil counter when possible. See
`docs/quests.md`. Then validate target guidance with a scanned entity and
right-stick click / Ctrl+B. Earlier feature-specific checks follow below.

NPC announcer: user-led walk around the Rogue Encampment with the log open.
Expect one `[npc]` announcement per NPC as each becomes the nearest in range,
nothing repeated after talking to them, and a re-announcement after walking
about eighteen metres away and back. If the camp's fences make announcements
misleading, set `line_of_sight = true` in `[npc]`. `docs/npc-proximity.md`.

Inventory/NPC: confirm the new item-in-hand/release notices with NVDA and the
controller; movement between equipment slots should not repeat them. Blacksmith
repair and other NPC services still need explicit confirmation.
`docs/inventory-npc-followup.md`.

Entity scan: confirm the hidden "una fuerza maligna" units no longer appear
in the camp, then calibrate `metersPerSubtile` by walking to an NPC at a known
spoken distance. Open RE items still unobserved live: cached unique/champion
names, shrine names, opened-chest modes, minion flag
(`docs/entity-units-re.md` "live check"). `docs/entity-scan.md`.

Items: user-led listening check of the quieter loops and landing-only start.
Approach gold, equipment, potion and scroll; check left/right, distance and lower
pitch below the character. Compare `[item-audio] progress` with listening feedback.
No gameplay input or audio capture during the user's test. `docs/ground-items.md`.

UI: skill descriptions and binding/action hints confirmed by user. On a future
level-up, verify the native spend-point hint appears; no point spending is needed.
Main-menu Options/Videos/Exit still need an explicit check. Independent radar
work continues below.

Next radar step: user-led listening of the build with the 2026-09-16 18:10
forensic fix (1-cell lateral tolerance, openDepth 10/close 7, clutter cap
5 cells) applied on top of the confirm-by-travel redesign (four directions,
original tin/tock sound design — the eight-direction/new-sound-design attempt
was already tried and reverted). Run `access-radar test` first to learn the
8 chirps (4 directions x open/blocked), then walk the camp and Blood Moor
with the log open, checking `stick=` against the actual push. Check cue rate
per minute, same-direction open/blocked flips under 1 second, and that
`[radar-state]` `hold` reasons (`retreat`/`interval`) match what was actually
heard — `retreat` is the world-anchored wall-memory gate (`Frontier`) that
replaced the old "repeat" hold. Do not mix desktop inputs with the user's
controller movement. Read
`docs/radar-redesign.md` first (validation checklist).

## Known issues / open questions

- **Entity scan:** distance factor (one subtile per metre) is provisional;
  line of sight uses terrain walkability, so a see-through fence hides what is
  behind it until the native BLOCK_LOS flag is verified. Monster/object
  offsets are verified offline against the live image and CASC tables, not yet
  in-game (`docs/entity-units-re.md`). The right stick showed no native
  gameplay use in the first live test.
- **Ground items:** audibility is confirmed. Quieter volume and landing-only start
  are installed and await listening feedback.
  Equipment speech gives base name and quality, not the complete generated name.
  Only resident room/neighbors are scanned. User-added drop sounds are unused;
  this feature uses the six loop recordings. `docs/ground-items.md`.
- **Sonar radar:** the 2026-09-16 confirm-by-travel redesign is installed but **not yet live-validated**, including the 18:10 forensic fix (1-cell lateral tolerance, openDepth 10/close 7, 5-cell clutter cap) that corrected the 76%-at-contact wall cues and false "exit" cues a live-session analysis found in the first cut. Direction count (four) and sound design are unchanged from before the review: an eight-direction/new-sound-design attempt was tried live and reverted at the user's request. Native wall sliding remains active and unaffected. `docs/radar-redesign.md`.
- **Footsteps:** both reported wall cases show real movement. No repeated stationary footfalls were captured, so live suppression remains unverified. Classic audio, first-footfall tails and non-locomotion animation modes retain prior limits. Ctrl+T now reports actual recent movement along the wall. `docs/footsteps.md`.

- Character list rows beyond the visible window (many characters) are not scroll-corrected yet.
- Dynamic text is read natively for popup rows, character rows and modal prompts (see "Widget text offsets"). ButtonWidget label offset is now derived (see above); applying it to the existing front-end modal reader is still pending. Also pending: legends (`ButtonLegendWidget` text/subText), tooltips set at runtime. The dropdown's own displayed value after a pick still comes from the tracked index; read it natively next.
- **In-game phase:** camp entry, repeated G warnings, stash/Charsi labels, basic inventory selection/fluency, action/slot names, blacksmith axe descriptions and coordinate suppression confirmed. Earlier pauses reached 719–1000 ms inside callbacks before SDK logging moved off the UI thread. Occupied grid cells await their description without coordinates; empty/unknown cells retain coordinate feedback. An occupied cell with no rendered tooltip remains silent. The exact scroll-related control is still unidentified. Full inventory operations, chat history, HUD/combat and navigation remain pending.
- Extended class descriptions (play styles, as in Diablo IV) do not exist in the game data: only one sentence per class (`str*Desc`). Any longer text would have to be written by the mod.

- **Disabled controls are not detectable through the SDK** (only `setWidgetEnabled` exists; the visual is `disabledFrame`). Known dependencies are encoded as `DisabledWhenRules` in `focus_tracker.cpp` (ScreenResolution unavailable when WindowMode == Fullscreen). The new native in-game reader uses `+0x50` for availability; the existing front-end reader still uses those dependency rules.
- Widget rect resolution is expensive (name walk per call); rects are cached per widget until a panel opens/closes or a list scrolls.

- ~~Does `UiMessageListener` fire pre-game?~~ **Yes** (run 1, 2026-09-14): title/logo screens emit `PanelManager` `OpenPanel`/`OpenExclusivePanel`/`ClosePanel` with names `LogoMenuPanel`, `TitleScreenPanel`, `TooltipsPanel`, `GammaCalibrationPanel`. Keyboard navigation inside a menu does NOT produce messages (only raw `InputMessage:KeyDown/KeyUp`), so focus tracking still needs phase 2.
- `MonsterHealth:ClearInfo` fires every frame (filtered from the trace). A DLL placed directly in `d2rloader/plugins` is treated as a plugin and rejected → `prism.dll` lives in `plugins/d2r-access/`.
- PRISM v0.18.2: `acquire_best` returns an already-initialized backend (`ALREADY_INITIALIZED` on `initialize` = OK). Fixed in `screen_reader.cpp`.
- ~~API version match?~~ **Yes**: loader 1.2.1-beta accepted API v4 (`d2rloader.log`).
- Reported "built-in TTS" in D2R is unverified (likely a forum request, not a feature) — check in Options once installed.
