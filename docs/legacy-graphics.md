# Legacy graphics

A tester, 2026-11-10: with Legacy Graphics on some menus read differently, and nothing says the
mode changed. Layouts compared 2026-11-10 (MEASURED from the files); live runs 2026-11-11 on D2R
3.4.95041 with two new offline characters, `pruebaclasico` (classic) and `pruebalod` (expansion):
a Reign of the Warlock character cannot switch, and every older character here is one.

## The switch

- The key is the game's *Legacy Toggle* (`CfgToggleRenderer`), **F5** on both new characters
  (Options, Controls), not G; the creation screen has the `ToggleLegacy` button.
- Both send `SystemMessage:RenderingToHDChanged`, then `SystemMessage:ResolutionChanged` 70 to
  180 ms later (MEASURED, five switches). The message has no argument saying which way.
- `Settings.json`'s `Legacy Graphics Mode` is not written on a switch (stayed 0 through all five).
- Legacy draws 800x600 pillarboxed; widget rectangles are then in that space (`rect 0,183 800x50`),
  and `NativeSkillNavigation::PlacePointer` still puts the pointer on the right pixel (the game
  converts: 400,208 went to 1280,499 on a 2560x1440 window).

`LegacyGraphics` (`src/legacy_graphics.*`) says "legacy graphics on / off" (13 languages) after
that message, once the layouts are back: the mode is the type of the HUD's root (`SDHeadsUpPanel`
in `HUDPanel.json`, `HUDPanelHD` in `HUDPanelHD.json`) or, with no HUD, of the creation screen's
heroes (`CharacterCreateWidget`, `HDCharacterSelectorWidget`); neither found within 2 s, it says
only that the mode changed. `[legacy]` in the log. Heard live both ways, in play (F5) and on the
creation screen (its button).

## What changes

The game loads `<Panel>.json` instead of `<Panel>HD.json` (`game-data/global/ui/Layouts/`). The
panel's root name is the same in both, the widgets under it are not. `tools/gen_panel_widgets.py`
ranks the HD file first and takes a non-HD one only when a panel has no HD file, so
`src/generated/panel_widgets.h` describes the HD trees alone.

| Panel | Live in legacy, before | Now |
|---|---|---|
| `CharacterStatsPanel` | only Close and a click catcher were places; no row could be reached by keyboard. The rows are plain `<name>Title` boxes with no `Tooltip` stop; `Level`, `CurrentXP` are named by the stem, `NextLevelXP` and `UnusedPoints` have labels not called Title | `CharacterStats::LabelRow` makes each label a place of the keyboard's cursor (14 live: "Fuerza. 30", "Maná. 10 de 10", "Experiencia. 0" …); values by the stem; the next level's experience and the points line are read by `Rows` and `Points` (tests only: no points to spend live) |
| Option sub-screens (`VideoOptionsPanel` …) | a row said its value alone ("ALTA") or its widget's name ("Gamma") | the generic reader takes the caption from the box beside control `X` called `XText` ("CALIDAD DE ILUMINACIÓN, ALTA", "GAMMA", live) |
| `PauseLayout`, `SettingsPanel` | read by the generic reader, in capitals as drawn; fine | — |
| Inventory (Original and Expansion), skill tree | read as in modern graphics | — |

Not looked at live: the stash (`BankExpansionLayout`: `grid`, `gold_withdraw`, `BankTabs` against
the HD `basicstash_container` …), the hireling, trade, waypoints, the quest log, the mini menu
(`MiniMenuToggle` against `MiniMenu/MiniMenuCharacter` …, hard-coded in `keyboard_menu.cpp`).

Also seen:

- `[tooltip] show delay left alone: padding=5 delay=1000, expected 50 and 400` at each switch to legacy.
- `[cursor] the pointer did not go to …; a click is refused`: `KeyboardMenu::Land` compared the
  pointer's pixel with the rectangle's middle, which in legacy is in 800x600 units, so a pointer
  already on the place was taken as not there: the legacy Options menu opens with its second
  entry under the pointer that clicked "Options", and A on it was refused. `MenuMouse::Answers`
  now asks the game to put the pointer a step beside the point and back; a pointer that moves
  has landed. Live: A opens Video Options at once.
- A label has no focus rectangle (the game's slot is read only on widgets whose select is the
  stock one), and the legacy panel is anchored (`rect` -320,-240 from the screen's middle), so
  `KeyboardMenu`'s `PlacedRect` places a label from a widget beside it that does have one.
- On landing in an option sub-screen the cursor's own name for the row follows its reading once
  ("LightingQuality"). A slider's row says its caption; A says its value. Left as is.
- The `Level` box holds its label's text ("Nivel") until the game fills it, so the first words on
  opening can be "Nivel Nivel". Left as is.
- The plugin's own commands are listed in English in Options, Controls with `text_locale = esES`.
- In the menus the click is A, not Enter: Enter is the game's own.
