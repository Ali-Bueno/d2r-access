# Character attributes and controller skill trees

Evidence: D2R 3.3.93787, Spanish, live controller navigation on 2026-09-15
at 22:27–22:34. This reader extends the existing native action pipeline and
PRISM sink. It does not change controller navigation or allocate any points.

## Attribute actions

The reported failure is reproduced in the log: every selected attribute
announced only `Gastar punto de estadística`. The actual type is
`CharacterStatLevelUpButtonWidget`, derived from `ButtonWidget`.

`CharacterStatsPanelHD.json` and its controller override define the companion
names below. Native reads confirmed the labels and values in the current game:

| Button | Label | Value widget | Observed value |
|---|---|---|---|
| StrengthPointButton | StrengthTitle: Fuerza | StrengthStat | 30 |
| DexPointButton | DexTitle: Destreza | DexStat | 20 |
| VitalityPointButton | VitalityTitle: Vitalidad | VitalityStat | 25 |
| EnergyPointButton | EnergyTitle: Energía | EnergyStat | 10 |

The reader resolves these live companion widgets by their shared naming
convention. It also reads the title's `Tooltip` child and `CombinedPointsInfo`
(observed: five points remaining). Values are never hardcoded. Stat actions
have a distinct archetype, including inherited subclasses. Missing companions
fall through to the existing native action reader; unavailable state is retained.
After allocating a point, only the changed value reads (right stick right: the points left).
2026-10-12 (tester): the new value was silent until focus left and came back, because the
announcement waited for the readout's text to change; the value change is now spoken on its
own. Opening the panel with points focuses Strength's "+" (`stat-points-focus`), and the
points left now end that button's announcement (`CharacterStatsReader::PointsFor`); a separate
line 600 ms later was cut off by it. Alone only if no announcement comes within 2 s
(`[stats] points alone`).

2026-10-13 (tester, still silent): the fix above only covered a value that changes under a
showing "+". The last point spent hides every `CharacterStatLevelUpButtonWidget` (all four sit in
`LevelUpInfo` in `Controller/CharacterStatsPanelHD.json`, held with A per `controllerRequireHold`; a sheet with no
points has none; the tester's 14:42:27 log: `UseSkillPoint` on Energy with five left, likely X's
five, then nothing; at 17:50:30 ten left, five spent, "42" said), the focus goes
with it, and `NativeProgression::Read` returns nothing for a hidden focus. `CharacterStatsReader`
now remembers the last "+" and its value, and says "Energy. 27" when it changes after the button
went (`[stats] value after its button went`; `[stats] … left unwatched` if focus left the panel
first). Same day: a "+" with focus no longer gives the right stick to the stat's explanation and
controls; it pages the statistics as everywhere on the sheet (`CombinedPointsInfo`, the last
row, is still right's).

## Skills and tabs

Controller `TabWidget` objects inherit `FocusableWidget`, not `TabBarWidget`.
Their localized native help string contains the branch name. The previous
reader handled tab bars but omitted these individual tabs. They now use the
same native focus/action pipeline.

Class skills are plain `ButtonWidget` objects beneath `SkillButtonContainer`
inside a `TabWidget`. Their live names encode the game's selected cell:
`SkillButton<tree> <column>,<row>`, with zero-based coordinates. Examples observed
include `SkillButton3 0,0`, `SkillButton3 2,4`, and `SkillButton1 1,2`.
The controller layout declares six `skillRow` positions and three `skillColumn`
positions. The reader announces the actual cell with one-based row/column;
the reader initially followed the existing free cursor. The user subsequently
confirmed that both stick and D-pad still moved the hand freely; discrete D-pad
navigation is implemented in the [controller follow-up](controller-navigation.md).

`CommonSkillsContainer` and `ItemSkillsContainer` contain granted/general skills
(`SkillSelectButtonWidget`, also derived from `ButtonWidget`). These use the
same description reader. Their names lack coordinates, so none are invented.
The owning tab supplies branch context; `SkillCounter` supplies remaining points.

2026-10-13 (tester: "no longer announces available skill points"): no code path ever said them
on entering with autodetail off. Since 2026-09-21 (`e54f4c6`, the user: heard on every highlight)
they are right stick right's. The tester had heard them from `ModalReader`'s whole-panel fallback
("5, 6, 1 Skill Choice Remaining, Javelin and Spear, ..."), which runs only when the game has not
focused a skill yet on the first poll: 2 of 14 openings in the tester's log (16:49:25, 18:07:39).
The 1.5 s queue behind a title (`cd03aa1`) removed nothing. Now the branch leads as the tree opens
(queued behind "Skill Tree") and on each LT/RT change (interrupting), with the choices left when
the branch is a class tree (`SkillButtonContainer`) and the count is above zero: "Defensive
Auras. 1 Skill Choice Remaining". General Skills (`CommonSkillsContainer`, `ItemSkillsContainer`)
is named alone. Log: `[skill-tree] branch opened|changed -> "..."`.

## Description order

Despite its name, the visible `TooltipTextBox` widget remained empty during
navigation. Skill descriptions reach the existing primary tooltip renderer
instead: [native tooltip hook](native-tooltips.md). The old speech trace already
contained Aullido, Buscar poción, Aturdir and others, but in bottom-to-top order.

A read-only sample of the transient record confirmed reversed newline order and
the native `U+E07E` font-size commands followed by `1` or `2`. For example, the
stored Aturdir text ends with its wrapped description, `U+E07E2Aturdir`, then
`Todavía no has aprendido esta habilidad`. The old generic glyph cleaner removed
the marker but left the size digit attached to the name.

Skill tooltip lines are joined for speech with a full stop between them
(`ProgressionText::JoinLines`), the same pause an item tooltip's lines get, so
stats no longer run into each other; a line ending in `. : ! ?` is not given a second one.

Synergies are drawn only on the skill that receives them: `skilldesc.txt`
`dsc3` lines, headed by string `Sksyn` ("%s recibe bonus de:"). Resist Fire has
none; Holy Fire lists Resist Fire (+`Param8` = 21 % fire damage per level) and
Salvation (+`Param7` = 10 %). The reader keeps every body line, so no filter
drops them (checked against CASC data, 2026-09-25; live readout still pending).

Only skill descriptions are reordered. The skill name leads, then learned state,
description, requirements, levels and bonuses in displayed order. Font-size digits
are removed without deleting actual levels or numbers. Row/column and remaining
points follow. Focus identity must match the render capture; pending skills wait
without reading a prior skill or prematurely speaking just coordinates.

## Textual skill prerequisites - 2026-10-04

The native tooltip capture contains skill text but not the prerequisite icons
and tree connections. The mod-side `NativeSkillBar::TreeSkill` now carries the
skill's localized name and direct prerequisite names. `ClassSkillAt` selects
the current player's class/page/row/column in the **active** table bank;
`ReadClassSkill` reads the selected skills.bin record's `reqskill1..3` fields
(signed 16-bit ids at `+0x21E`, `+0x220`, `+0x222`, row stride `0x2EC`). Each
id follows the existing skilldesc/name-id localization path. This supports
all current class rows without hardcoding names or depending on tooltip text.

Native `DataTbls.cpp` parser RVA `0x302380..0x3074C1` explicitly maps the three
source fields to those offsets (descriptor setup `0x305A6A..0x305BFB`). Its
separate call-normalized fingerprint guards prerequisite reading; a mismatch
omits those new fields while preserving the established skill-name reader.
`-1` denotes no prerequisite. Zero is a valid skill id. Duplicate ids are
collapsed; invalid, self-referencing or unreadable dependencies omit the list
rather than state incomplete requirements. Only direct dependencies are
listed; the description for each preceding skill explains its own parents.

`NativeProgression::Details` copies that list for the current class-tree
button. `ProgressionText::SkillLines` adds a localized
`Prerequisite: 1 skill point in <name>` line for each; `SkillDescription` uses
the same lines. Both J/right-stick detail and Ctrl+arrow line stepping use
that representation after the existing current-tooltip ownership checks.
The name/headline and existing native text order are preserved. No extra
arrival speech, list positions or synthetic descriptions for pending skills.
Starting/common/granted skills with no table dependencies add no lines.

Stock Holy Fire's prerequisite is **Might** (`skills.txt` `reqskill1`), while
Resist Fire and Salvation remain its separately rendered synergy bonuses.
The new text does not replace or reinterpret those native synergy lines.

Validation: active-memory table fixtures cover localized lookup, modified
requirements, all three slots, absent/invalid ids, missing names and ambiguous
cells. Reader tests cover J, Ctrl+Right/Up, current ownership and clearing on a
new skill; formatting tests preserve native text. All 13 phrase tables are
translated and pass placeholder/completeness checks. Full CTest **105/105**,
timeout 20; affected suites **4/4**. Normal local four-job BelowNormal build
and POST_BUILD deploy ran with D2R/D2RLoader closed; plugin matches final build,
PRISM and loader config unchanged. The final relink differs from the tested
staged DLL only in 4 PE timestamp bytes. User confirmed prerequisite descriptions work perfectly in game.
Records, original source, native descriptors, logs, hashes and old runtime:
`../re/skill-prerequisites-2026-10-04/`.

## Validation

- `progression-tests`: 27 checks for all four attributes, live value changes,
  hidden/missing companions, skill container roles, malformed cell names,
  actual captured text ordering, formatting commands and learned/general skills.
- Native UI and interaction suites cover classification, tab labels, stat
  value-only updates, disabled state, grid speech, stale descriptions and repeats.
- Isolated patch build: fourteen CTest suites pass. Integrated build, including
  concurrent radar work: sixteen suites pass. Normal `build.ps1` deployed the DLL;
  its SHA-256 matched the installed artifact.
- User navigation established the original failure and data sources. A live
  controller/screen-reader confirmation of the corrected build is still pending.
