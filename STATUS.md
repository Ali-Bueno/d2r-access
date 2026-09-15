# STATUS — Diablo II: Resurrected (d2r-access)

> Per-mod status ledger. Open first when resuming. Dashboard, not docs.

**Last updated:** 2026-09-14 (menus readable with controller; option values)

## Identity

- **Engine / framework:** native C++ x64 (D2R.exe, patch 3.3) / **D2RLoader** (closed-source loader, v1.2.1-beta 2026-09-03, https://d2rloader.net) + its open **PluginSDK** (C++20 header-only, MIT, cloned at `../external/PluginSDK`, API v4).
- **Screen-reader transport:** plugin DLL → `LoadLibraryW(<pluginDirectory>\prism.dll)` + `GetProcAddress` (load in `D2RLoaderLoadPlugin`, never in `DllMain`).
- **Build command:** `.\build.ps1` (CMake 4.2 + Ninja + MSVC Build Tools 2022 14.44; vcvars64 from `C:\Program Files (x86)\Microsoft Visual Studio2\BuildTools`). POST_BUILD copies `d2rl-d2r-access.dll` + `prism.dll` into the game. PRISM v0.18.2 vendored in `third_party/prism/`.
- **Mod install path:** `D:\games\steam\steamapps\common\Diablo II Resurrected\d2rloader\plugins\` (global) — D2RLoader zip extracted into the game folder, launch `D2RLoader.exe`.
- **Run / test:** Steam D2R 3.3.93787 (app 2536520, Spanish) installed. D2RLoader 1.2.1-beta installed into the game folder (SHA-256 verified). Start NVDA, launch `D2RLoader.exe`. Loader config (`d2rloader/config/d2rloader.toml`): `enable_console = true` (Ctrl+` in-game), developer logs on with `widget_panels` + `extension_details`. Logs: `d2rloader/logs/d2rloader.log`. Offline/single-player only (online ban risk undocumented).

## Section status

| Section / feature | Status | Notes |
|---|---|---|
| Adapter + Speech (`Speak("test")` through PRISM) | done | Verified with NVDA 2026-09-14 (PRISM 0.18.2, backend NVDA). Console: `access-say <text>` |
| UiMessage logger (discovery + panel open/close announcer) | wip | `src/ui_message_monitor.cpp` + `panel_state.cpp` (open stack; close spoken only if it was the active panel, never interrupting). Speaks raw panel names for now |
| Widget vocabulary from layout JSON | wip | Extracted (2026-09-14): 329 layouts + 31 string tables in `game-data/` (gitignored) via `tools/casc-extract` (CascLib CLI; open the game root, not `Data\`). Indexer: `tools/layout_index.py` → `game-data/index/` |
| Focus/hover tracking (own inline hook) | todo | phase 2; needs Ghidra on D2R.exe 3.3 |
| Main menu / char select | todo | SDK input actions do NOT fire pre-game → own key poll from UI-thread pump |
| In-game panels (inventory, skills, stash) | todo | |
| Control values | wip | Sliders: percent from the runtime `Indicator` child (done). Toggles/carousels: baseline from `%USERPROFILE%\Saved Games\Diablo II Resurrected\Settings.json` (key = update command minus `Set`, normalized) + flip / step by last horizontal input (`control_state`, `settings_file`, `input_direction`); dropdowns: popup row index from the indicator y on `DropdownListContents/ItemTemplate`. **Untested in game** |
| Dynamic text (modals, tooltips, chat) | todo | Needs a widget pointer + text offset. Ghidra `D2R_live` DB analyzed (81,635 fns) but Arxan stripped constant xrefs; the focus getter at live RVA 0xE20BE0 returns a static focus-manager object (no widget). Modals get a controller hint instead (`A para confirmar, B para cancelar`). Next lead: call-graph descent from 0xF7BF80 (only ref to "SliderWidget") |
| Item readout | todo | `ItemInfo` has code/grid/quality but no name/stats → data-table row RE or tooltip hook |

## Derived facts (source: PluginSDK headers, path:line relative to external/PluginSDK/include/D2RLPlugin)

| Fact | Value | Source |
|---|---|---|
| Required exports | `D2RLoaderGetPluginInfo`, `D2RLoaderLoadPlugin(const PluginContext*)`, `D2RLoaderUnloadPlugin` | lifecycle.h:7-9 |
| Plugin flags | `PluginFlags::Client` (ours) + `NativeHooks` (0x2) only if using `InstallInlineHook` | lifecycle.h:13-20 |
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
| Settings tabs | `SettingsPanelHD` `TabBarWidget OptionsTabs`: `textStrings` [Vídeo, Audio, Juego, Controles] ↔ `tabMessages` `SettingsPanelMessage:CheckChanges:{VideoOptionsPanel,SoundOptionsPanel,GameplayOptionsPanel,KeyBindingMenu}` | index |
| Main menu (controller) | `MainMenuPanelHD` QuickPlayButton "JUGAR" → `MainMenuHD:Play`; ribbon Opciones→`PanelManager:OpenPanel:SettingsPanel`, Vídeos→`FrontEndNavigation:ToCinematics`, Créditos→`OpenPanel:CreditsModal`, Salir→`FrontEndNavigation:ToExit` | index |
| D2R.exe code | `.text` is Arxan-encrypted on disk; the plugin dumps the live `.text` once at first `ToFrontEnd` (`src/code_dump.cpp` → `plugins/d2r-access/D2R.text.bin`, 30,076,928 bytes, entropy 6.5 = real code). **Live layout ≠ disk**: live `.text` is 0x1CAF000 (disk 0x15CB6E0) and every later section is at `disk_rva + 0x6E3000` (IAT-call vote 76:1); on-disk `.pdata` is stale for the live code. `re/scripts/rebuild_pe.py` → `re/D2R.rebuilt.exe` (file offset == RVA); Ghidra program `D2R_live` in `re/ghidra`, seeded with 55,944 functions (`SeedFunctions.java`), analysis log `re/ghidra/analysis-live.log`. Notes: `re/notes.md` | Opus RE passes 2026-09-14 |
| Widget JSON reflection table | Every layout JSON field has a 0x30-byte record in `.data`: `+0x08 memberSize, +0x18 name, +0x20 type (0 int,1 bool,2 float,3 string,4 struct,5 enum,6 msg-hash), +0x28 memberOffset`; 3,904 records (`re/out/schema*.tsv`). Static: SliderWidget `increment +0x5B0`, `onUpdateMessage +0x590`, `isHorizontal +0x588`; Panel `defaultWidget +0x90`; Modal `confirmMessage +0x1A0`/`denyMessage +0x1C0`; widget `name +0x0`, `rect +0x70`. **`isToggled` (rec 0x2400418), `states` (0x2400FE8), `text` (0x2400118), `tooltipString` (0x23FFCC8) get type/offset filled at runtime** → the plugin can read those records live (rec RVA + 0x6E3000 from exeBase) instead of hardcoding. Engine string type = 0x28 bytes | re/notes.md §7 |
| Slider handle | `SettingsSliderWidget` creates a runtime child `Indicator`; percent = (indicator.x - slider.x) / (slider.width - indicator.width) | run 8 probe 2026-09-14 |
| Value messages | Controls' `onUpdateMessage` (`OptionsPanelMessage:SetMasterVolume`…) DO reach the listener but with empty text; panels fire all of them on init | run 7 |
| Focus vocabulary in D2R.exe .rdata | `FocusManager` 0x172E0C0, `FocusLink`, `FocusNavigation`, `FocusIndicator`, `panelBoundary`, `FocusableWidget` 0x1711EA0, `SetFocusIndex`, input actions `focusUp/Down/Left/Right`, `navigateUp/…`; build paths `D2Client\src\Game\ControllerInputHandler.cpp`, `UI\Widgets\SettingsPanel.cpp` | re/out/strings.tsv |
| Runtime message stream (controller) | Main menu = `MainMenuPanel` (exclusive) + `MainMenuButtonRibbon` + `MainMenuLinks` + `CharacterSelectPanel` open together; D-pad moves emit **no** message; `CharacterCreate:Hover/Unhover/Select <ClassKey>` (text is a string key: "Barbarian"→"Bárbaro"); `CharacterSelect:SelectAct` every frame; `MainMenuHD:UpdateCharacterInfo/CheckUpdateLinks/UpdateButtonLegend`; modals: `OnlinePlayModeModal`, `ConfirmationModal`, `DismissableModal` (text widget `Prompt`, dynamic) | run 3, 2026-09-14 |
| WidgetService rect semantics | `getWidgetRect` is local to the parent. The layout `anchor` (fraction of parent size) is **already applied by the SDK when the parent has a nonzero size**, but root panels (and the screen) report 0x0, so anchors at those levels collapse → apply them against the HD layout canvas **3840x2160** (measured: QuickPlayButton anchor 0.5, local x -710 → indicator 1211; CinematicsModal anchor y 0.1 + Option0 y 170 → 386). Controller/Console|Switch|Handheld variants do not exist on PC → excluded from the catalog | runs 5-6 diagnostics 2026-09-14 |
| Dropdown popup scrolling | `DropdownListContents` shows 13 rows; past the last visible row the list scrolls under a still `FocusIndicator` (every D-pad press emits `ScrollController:ManualScrollInput`). The option index = visible row + scroll offset, tracked from the press direction (`InputDirection::LastVertical`); the first read assumes the applied option is highlighted (static states from Settings.json; resolution from `"Screen Resolution (Windowed)"` = `"WxH"`). Display modes: GDI `EnumDisplaySettings` and DXGI list the same set; the game's list starts at 1024x768 | log 2026-09-15 04:04 |
| Text language storage | `Settings.json "Language"` (not `TextLanguage`) holds the locale's index in the string-table order `enUS, zhTW, deDE, esES, frFR, itIT, koKR, plPL, esMX, jaJP, ptBR, ruRU, zhCN` (`local/lng/strings/ui.json`), which differs from the carousel's `states` order; mapped in `control_state.cpp` (`StoredValueOrders`). `SetTextLanguage` applies immediately (`AcceptChanges` + front-end reload) | Settings.json 2026-09-15 |
| Hidden table rows | `SoundOptionsPanel` `Row VoiceLanguage` is hidden on this install (one voice pack), so `Row TextLanguage` is laid out on its slot and both carousels resolve to the same rect. The SDK has no visibility getter; on a coverage tie within a panel the later sibling wins (a stacked layout only skips hidden rows) | log 2026-09-15 03:57 |
| Preloaded in-game panels | Right after `OpenExclusivePanel TitleScreenPanel` the game opens `ShowItemsPanel` and `EmoteWheelPanel` (never closed); their buttons are invisible in the front end but overlapped CharacterCreate's class area (Emote6 "¡Adiós!" at coverage 0.51). `FrontEndNavigation:ToFrontEnd` now resets the panel record | log 2026-09-15 03:58 |
| Button → panel wiring | `ButtonWidget.fields.onClickMessage = "PanelManager:OpenPanel:SettingsPanel"`, `textString = "@OptionsHD"` (MainMenuButtonRibbonHD.json:43) | extraction |

## References

- `docs/research-roadmap-chatgpt.md` (+ `-extended.md`/`.docx`, 744 lines) — user-supplied research roadmap (ChatGPT, 2026-09-14). No offsets/RVAs anywhere; focus tracking is its declared biggest unknown (extended §4.5, line ~171): experiment list + external repos to mine for later phases (MapAssist / D2RMH for unit tables & map data, diablo-access, D2RL-Plugins). Its C#-companion-via-IPC idea is superseded: PRISM speaks in-process.

## Next step

Verify with the controller: (1) Video → Resolución popup reaches 2560x1440 (row 14 of 15) and the highlighted row on open matches `Settings.json` once a resolution has been applied; (2) Audio → "Idioma del texto" is announced with its current value and the new one after a change; (3) Crear personaje no longer reads emote buttons.

## Known issues / open questions

- **Disabled controls are not detectable through the SDK** (only `setWidgetEnabled` exists; the visual is `disabledFrame`). Known dependencies are encoded as `DisabledWhenRules` in `focus_tracker.cpp` (ScreenResolution unavailable when WindowMode == Fullscreen). General detection needs the widget object's enabled flag (RE) or an SDK `getWidgetEnabled`.
- Widget rect resolution is expensive (name walk per call); rects are cached per widget until a panel opens/closes or a list scrolls.

- ~~Does `UiMessageListener` fire pre-game?~~ **Yes** (run 1, 2026-09-14): title/logo screens emit `PanelManager` `OpenPanel`/`OpenExclusivePanel`/`ClosePanel` with names `LogoMenuPanel`, `TitleScreenPanel`, `TooltipsPanel`, `GammaCalibrationPanel`. Keyboard navigation inside a menu does NOT produce messages (only raw `InputMessage:KeyDown/KeyUp`), so focus tracking still needs phase 2.
- `MonsterHealth:ClearInfo` fires every frame (filtered from the trace). A DLL placed directly in `d2rloader/plugins` is treated as a plugin and rejected → `prism.dll` lives in `plugins/d2r-access/`.
- PRISM v0.18.2: `acquire_best` returns an already-initialized backend (`ALREADY_INITIALIZED` on `initialize` = OK). Fixed in `screen_reader.cpp`.
- ~~API version match?~~ **Yes**: loader 1.2.1-beta accepted API v4 (`d2rloader.log`).
- Reported "built-in TTS" in D2R is unverified (likely a forum request, not a feature) — check in Options once installed.
