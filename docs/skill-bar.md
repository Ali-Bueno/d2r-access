# Controller skill bar: hearing the LT bar

Target: D2R 3.3.93787, native C++ / D2RLoader, existing PRISM sink. 2026-09-25.

Holding LT (L2) alone for 0.6 s during play reads the six LT-bar slots in HUD
order: control, the game's skill name, and the count the HUD draws (scrolls in
a tome, a throwing stack). Empty slots say "Vacío". Releasing LT cuts the
reading. Pressing any other button during the hold, or while LT is already
held, makes the hold a combo and it stays silent; a combo pressed while the bar
is being read cuts the reading. A hold that starts in a menu, dialogue or pause
(`RadarUi::InPlay`) never reads. `access-skill-bar on|off` toggles it. The
feature only reads memory: it binds nothing, casts nothing, changes no HUD.

Code: `skill_bar.*` (hold gate and phrase, tested), `native_skill_bar.*`
(game reads), `skill_bar_service.*` (wiring), `InputDirection::LeftTrigger`.

## Stock controller map (for reference)

From the game's own controller help (`Controller/HelpPanelHD.json`,
`ui-controller.json` `ControllerHelp*` glyphs):

| Control | Stock action |
|---|---|
| A / X / B / Y | Skills 1–4 (A also interacts and loots) |
| RB, RT | Skills 5, 6 |
| LT | "Skill toggle": while held, the six skill buttons use the LT bar (12 slots in all) |
| LB | Show item labels (hold or toggle per the "Show Items on Ground" option) |
| D-pad left/up/down/right | Belt slots 1–4; with LT held, the potion goes to the mercenary |
| L3 / R3 | Run-walk toggle / weapon swap (hold: emote wheel) |
| View / Menu | Automap / game menu |

Assignments on the LT bar are shown by the game as "LT + button" (`0x14D4A0`).
With "Weapon Swap Separate Skill Buttons" each weapon set has its own 12.

## Native evidence

Full mapped image and Ghidra decompiles of `D2R_live`, plus CASC `skills.bin`,
`skilldesc.bin` and `hd/global/excel/controllersettings.json`. RVAs from image
base `0x140000000`. Every function below is fingerprinted in
`native_skill_bar.cpp`.

- **Bindings.** Controller mode (`0x77E10`) bindings are 32 records of `0x1C`
  at `.data 0x2A48A90`: `+0` skill id (-1 empty), `+4` owner item id (-1
  innate). Records 0..5 are `SkillButton1..6`, 6..11 the LT bar
  (`SKILL_BUTTON` enum, `0x8BC500`); 16..31 repeat for weapon set II.
- **Getter.** `0x117D20(index)` returns the skill id; it adds the weapon-set
  offset from `0x1195C0` (16 on set II with separate bindings, else 0) and
  resolves Town Portal pseudo-skill 359 to 219 (scroll, when any are held) or
  220 (tome). Bounds-asserted to 0..15. The HUD's `SkillSelectButtonWidget`
  update `0x2399F0` uses the same getter with `index = skillButton + 6` while
  the LT bar is shown. Its displayed-skill field `+0xC08` is **not** reset for
  an empty binding (`0x239B2B..82`), so the table is read, not the widget.
- **LT bar shown.** `0x2399F0`: `0x14CF40() && 0x13DE60(0x13CE90())`.
  `0x14CF40` is the LT-held query (held-button mask `0x400`; Storm's XInput
  poll `0x1210140` emits `0x400` for LT past `XINPUT_GAMEPAD_TRIGGER_THRESHOLD`).
  `native_belt.cpp` calls the same function `SelectionMode`.
- **Name.** Table bank `0x300A90(Unit+0x1BD)`: skills rows at `+0x11B0`
  (count `+0x11B8`, stride `0x2EC`), row `+0x23C` int16 skilldesc index;
  skilldesc rows at `+0x1190` (count `+0x1198`, stride 300), row `+8` uint16
  name string id (`0x88850`); `0x28BCF0` yields `0x1506` when there is none.
  Resolved with `Localization::Text(id)`, so it follows the game language.
- **Count.** `0x239380`/`0x239110` show `QuantityText` only when the count is
  above 0 and write it as `"%d"`; hidden text is stale. Town Portal shows
  scrolls + tome charges. Read only while the game itself shows the LT bar.
- **Control per slot.** `controllersettings.json`
  `controllerSkillButtonMapping[0]` (PC always uses variant 0: `0x120FF40`
  returns `0x1000`) maps `SkillButton1..6` to A, X, B, Y, RB, RT — the same
  glyphs as `ControllerHelpSkill1..6`, which the reader decodes.

## Validation

Build and CTest 77/77 on 2026-09-25, including `skill-bar` (hold gate, combos,
menus, phrase order and locale punctuation, help-line decoding). Installed
DLL SHA-256 `561EE346…` matches the build. The user confirmed it works live
(2026-09-25). Not yet checked live: a weapon swap with separate bindings on,
and a charged-item skill (count hidden).

## Rebinding (2026-10-18)

The game's rebinding window (`ButtonBindingModal`, holding X on a skill) said only "Press Button To
Rebind Skill closed". `SkillBarService` reads both bars as it opens and again as it closes, and the
slot that changed is said: "Prayer bound to RB", "LT" before a button of the LT bar; its close is
no longer announced (`[skill-bar] binding window closed -> "..."`).
