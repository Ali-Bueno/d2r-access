# Skill coverage, controller actions and stale descriptions

Target: native D2R 3.3.93787 / D2RLoader, 2026-09-16. This extends the
[controller navigation adapter](controller-navigation.md) and existing PRISM reader.

## Report and live evidence

The user reports improved D-pad movement, but several different cells announce
Aturdir or Órdenes de batalla, and controller assignment/level-up instructions
are missing. The 07:15–07:16 log repeats Aturdir while `[skill-grid] Selected`
changes between rows/columns. The 07:31–07:32 reproduction also contains correct
names on some selections. Merely checking that the captured tooltip's copied
focus pointer matches is insufficient: the stock tooltip can remain stale.

Read-only inspection found all ten buttons in each of the Barbarian's three
branches, including skills unavailable at the player's current level. The exact
thirty cell positions are regression fixtures: every skill is reachable from
every starting skill through cardinal movement. This verifies this character's
tree, without claiming live coverage for every class.

Porrazo's native `HotkeyText` contains U+E00F (A); the live controller footer
contains U+E011 `Asignar habilidad` (X), followed by U+E010 `Cerrar menú` (B).
The character currently has zero skill points, so the spend-point action is
absent. No points or bindings were changed during inspection.

## Cursor/description synchronization

The stock cursor commit `0x2346C0` calls position setter `0x14F690`, then
`0x8676E0(FocusManager, true)` to recompute hover, dispatch its messages and update
focus. The adapter previously called only the position setter and focus setter.
Its held-input reset suppressed the usual cursor commit, leaving another skill's
tooltip until the next stock hover refresh.

The adapter now includes the same hover refresh before selecting the target.
It passes false for the optional movement sound and never dispatches a click.
`FocusManager` is read from `PanelManager+0xD0` as in `0x846020`. The added call is
guarded by the entire 2,050-byte function fingerprint:
`0x3933341E0BAB49A8`. Existing gesture/release guards remain in place.

## Accessible control help

`ButtonLegendWidget` is a shared archetype, including `ButtonLegendScrollWidget`.
`native_controller_help.cpp` reads its active entries in native order, including
the shared footer in `ControllerOverlay` outside the focused panel. It requires
current focus ownership and visible legends; removed actions stay absent.

| Native fact | Evidence |
|---|---|
| Legend vector `+0x88`, count `+0x90`, entry stride `0x40` | `0x874270` adds actions, `0x873B00` removes them; `0x87B080` renders them |
| Entry glyphs `+0/+4`, glyph count `+8`, caption string `+0x10` | Same renderer, bounded chords of at most two controls |
| U+E000…U+E012 controller symbols | `0x872E50` registers physical controls; `0x1441F0` maps XInput masks; live Xbox footer validates face labels |
| Binding modifier | `0x14D4A0` formats alternate assignments as LT + the bound button |
| Spend/bind visibility and hold wording | `0x14C73C0` adds/removes action 0/1 using `ControllerPromptAssignSkillPoint`, `ControllerPromptHoldFormatStringStandalone` and `ControllerPromptBindSkillButton` |
| Current assignment | `0x14C72C0` updates the selected skill's `HotkeyText`; the reader copies that text |

`controller_text.cpp` decodes control symbols specifically for help/bindings;
ordinary text cleaning still removes decorative glyphs. The skill readout is:
name, cell and position/count, remaining points, current assignment, available
actions, then the full native description. Unlearned skills remain readable.
Hidden labels, invalid records and unknown button codes do not invent bindings.

There is no reset-points action in the inspected skill-tree footer. The game has
the separate `respec` action, localized as `Reiniciar puntos de estadísticas/habilidades`.
Akara provides the quest reset outside this screen; this patch does not add a
reset shortcut or change quest availability.

## Validation

Normal `build.ps1` compiled and deployed with the game closed. Fourteen UI suites
contain 435 passing checks, including 17 controller-help checks, 31 progression,
65 navigation and 58 native UI checks. The full shared build initially had a
failure in the other agent's in-progress `radar-precision` test; no radar code is
part of this fix.

Built/installed SHA-256 at verification:
`E4D8A3B6036CB540D3FF17D22721E1DD13429344E1BC6E9578BC4F2119298E06`.
The 07:42–07:43 live log after installation distinguishes Grito de batalla,
Órdenes de batalla and Resguardo lúgubre, then Dominio de los filos, Porrazo,
Ataque doble and Salto across branches. Porrazo reads `Asignada a A`,
`X: Asignar habilidad` and `B: Cerrar menú`, followed by its own description.
The user then confirmed the requested controller recheck: “sí, parece que ahora
funciona bien!” Point spending was not requested; the zero-point state cannot
validate live spend-button output.

## Naming a skill before its tooltip

MEASURED 2026-10-02 (sorceress): `[field] focus` to `[interaction]` took 420–456 ms on
every D-pad move, because the reader waited for the stock tooltip to supply the name.
The cell's generated name already says where the skill sits:
`SkillButton<SkillPage> <SkillColumn-1>,<SkillRow-1>` (live: Blizzard `SkillButton3 0,4`
= page 3, row 5, column 1; Chilling Armor `2,4`; Frozen Orb `0,5`; Barbarian Howl
`SkillButton3 0,0`, all matching `game-data/global/excel/skilldesc.txt`).

| Native fact | Evidence |
|---|---|
| skills row `+0x2C` u8 charclass: PlayerClass index (ama … ass, war = 7), `0xFF` for none | The only offset matching all 429 rows of CASC `skills.bin` (3.3.93787) |
| skilldesc row `+2/+3/+4` u8 SkillPage, SkillRow, SkillColumn | All 263 rows of CASC `skilldesc.bin` |
| Player class | Player unit `+4` (`GroundItemLayout::UnitClass`) |

`NativeSkillBar::ClassSkillName` finds the class skill at that place and names it the
way the LT bar does (two skills in one place return nothing). The reader then speaks
name, points, binding, actions and position after one poll, so the legend the game
rebuilds for the move is the new skill's. When the tooltip renders it only supplies the
detail: offered to right stick up, or queued behind the name with automatic detail on.
A tooltip naming another skill replaces the table name and logs
`[skill-tree] table name … differs from the tooltip …`. Common and item-granted skills
have no cell and still wait for their tooltip.
