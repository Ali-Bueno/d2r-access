# Quest log

How the mod names quests and reads objectives, and what is still open.

## Journal detail reading — 2026-09-16 follow-up

The controller session at 21:44 proved `ShortDescription` / `LongDescription`
are the correct sources: Den of Evil populated `Den of Evil` / `Kill all the
monsters in the Den.`. Other selected quest icons had empty detail panes both
on screen and in memory. Activating Den of Evil's speech populated the separate
`QuestLogSpeechPanel` with Akara's full narrative.

The QuestButton archetype now associates the shared detail pane with the focused
quest, waits 150 ms for repopulation, and reads title and current objective once.
A changed counter/objective on the same quest reads only the change. Previously
the title repeated after the objective, with a possible bare-label announcement
during repopulation. Mismatched headings wait for the selected quest's text.
Empty objectives explicitly say no objective text is available, localized in
all 13 mod languages. Empty text does not prove completion or non-start, so no
such status is invented. Actual quest text/counters still come from the game.

No counter is fabricated when the journal shows none. Icon reading no longer
assumes the ordinary ButtonWidget embedded caption offset; the icon tooltip
or catalog name suffices. Pure UI tests cover selection changes, stale details,
live numeric progress, completion text, missing objectives and deduplication.

## Reaching the kill counter (2026-09-16)

The Den of Evil kill counter is never spoken because the mod never obtains it: the journal readout
took only the `ShortDescription`/`LongDescription` text boxes and discarded the rest of the panel, and
`QuestLog::Counter` has no production caller. Across the whole 28 MB live session log the counter
value never appears anywhere — every `[quest] watch` line for this quest shows only
`short="Den of Evil" long="Kill all the monsters in the Den."`, and the game's own UI messages seen are
`QuestLogPanelMessage`/`SelectQuest` (123), `SetQuestAlert` (5), `PlayDialogue` and `DialogueFinished` —
none carrying a counter.

`src/ui_text.cpp`'s quest-button branch now also reads every other text node of the quest panel in
screen order (deduplicated, excluding the panel's static "Title" heading and the quest-icon text) and
appends it after the objective, so a counter rendered anywhere in that pane is spoken.
`src/game_text_reader.cpp` now logs the quest panel's whole widget tree once per selected quest (the
existing `[text-tree]` dump, no longer only on demand), so the next live session shows exactly which
widget, if any, carries the counter.

**Still open:** whether D2R renders the counter in that panel at all is unresolved — no native
quest-state field or offset has ever been recovered, and the SDK exposes only `QuestCompleted`. If the
next log's `[text-tree]` dump shows no counter widget, the remaining route is a targeted RE pass seeded
from the `qstsa1q14` string reference. Counting live monsters or inferring progress from the catalog
stays forbidden.

## Why the quest tab was silent

Two separate causes, both now fixed:

1. **The panel was discarded before it was read.** `NativeUi::MatchType` never gave the
   `QuestLogPanel` type a `Surface`, so `game_text_reader.cpp` dropped it (`surface == Other`)
   along with everything inside it. Confirmed by the trace: no `[native]` line ever mentioned a
   quest. Fixed by classifying `QuestLogPanel` as `Surface::Menu` in `native_ui.cpp` — the type,
   not the panel name, so both `QuestLogPanelExpansion` and `QuestLogPanelOriginal` are covered.
2. **The quest buttons have no caption.** A `QuestLogButtonWidget` is an icon. Focus landed on a
   control with nothing to say. They are now their own archetype (`Kind::QuestButton`) and are
   named from the game's string table when the widget itself carries no text.

A red herring to not chase again: the SDK's `Widgets::findPanel` returns `Unavailable` (3, *not*
`NotFound`) for this panel. That happens inside D2RLoader and is not specific to the quest log —
`WaypointsPanelExpansion` and `SkillsTreePanel` do it too, and resolve fine in other sessions. The
mod's own native route (`NativeWidgets::FindPanel`, the game's PanelManager lookup, which also
searches the managed-panel vectors at `+0x88`/`+0xA0`) reaches more than the SDK does and is what
the quest code uses. Do not "fix" `widgets.cpp` for this.

## Which quest a button stands for

Every quest button carries its icon filename, and that filename **is** the act/quest pair:
`Quest0` → `QuestIcons\a1q1`, `Quest5` → `QuestIcons\a1q6`. That is the whole mapping, and it is
read from the game's layout rather than counted, because **the button indices are not in quest
order**: `Quest2` is act 1 quest **5** (The Forgotten Tower), and act 3 is scrambled outright
(`Quest15` is quest 1, `Quest12` is quest 4). Counting positions would name the wrong quest.

`tools/gen_quest_catalog.py` turns that into `src/generated/quest_catalog.h` (27 quests: 6/6/6/3/6
across acts I–V). Re-run it after a CASC re-extract, like the other generators.

## Where the text comes from

Quest names and objective text come from the game's own string table:

| key | content |
|---|---|
| `qstsa<act>q<n>` | quest name — `qstsa1q1` = "El Cubil del Mal" |
| `qstsa<act>q<n><step>` | the objective lines, in shipped order |

Resolved live through `Localization::Text`, so the player hears their own language. Two quirks the
catalog handles and that are easy to trip over:

- **Act 5 quest 1** has its title under `qstsa5q1` but its steps under `qsta5q1…` — Blizzard
  dropped the second `s`. The generator collects both prefixes.
- **Live counters** ship as printf lines: `qstsa1q14` = "Monstruos restantes: %d", with the
  last-one-remaining wording as its own key, the counter's key plus a trailing zero
  (`qstsa1q140` = "Falta un monstruo."). `QuestLog::Objectives` leaves counter lines out — speaking
  a raw `%d` would be worse than silence — and `QuestLog::Counter` fills one in when a value exists,
  though it has no production caller yet (see "Reaching the kill counter" below).

`ShortDescription` and `LongDescription` are empty in the layout: the game fills them at runtime
with the selected quest's objective **already formatted**, counter included. That is why no quest
state had to be reverse-engineered to read an objective.

## What was looked for and does not exist

Recorded so it is not re-investigated:

- The UI message event carries no numeric argument (`UiMessageEvent` is target/command/text only),
  so `SelectQuest` and `SetQuestAlert` cannot tell us *which* quest. The loader builds that event;
  the mod cannot widen it.
- The SDK has no quest API and no `Quests` data table, and D2 never shipped a `quests.txt` — quest
  logic is hardcoded in the game.
- The RE dumps in `re/` contain the quest strings (`SetQuestAlert`, `questRecords`,
  `D2Client\src\Quest\Quests.cpp`) but with zero usable cross-references, so no quest state struct
  or offset has been recovered. A fresh targeted Ghidra pass is the expensive last resort, not the
  starting point.
- `QuestFlag`/`QuestFlagEx` are `Levels.txt` column headers and `questflag A/B/C` an item-table
  column — neither is player quest state.

The SDK's `GameplayEventKind::QuestCompleted` does exist and carries a `questRecordId`, but only on
completion, and the row order behind that id is unverified.

## Automatic announcements (implemented 2026-09-16)

Enabled by default. Console `access-quests on|off|status` controls this feature
independently of journal navigation. No need to open the journal for an alert.

- `QuestLogPanelMessage:SetQuestAlert` / `QuestUpdate` schedule one update
  announcement. Bursts are grouped over 800 ms and duplicate signals within
  1000 ms are coalesced. These are policy timings, not native offsets.
- A 500 ms watch reads the native journal text. It accepts an objective only
  when its localized heading identifies a catalog quest and every objective
  line matches that quest's own strings (including actual numeric counters).
  A stable change to the same quest while the journal stays closed is spoken
  as quest name plus current objective. Text settles for 150 ms first.
- If the panel is missing, stale, ambiguous or not identifiable, the event
  still says **"Diario de misiones actualizado"** (localized in all 13 locales).
  It never fills in an objective from the quest catalog without a live value.
- SDK `QuestCompleted` says **"Objetivo principal de misión cumplido"**. This
  means `PrimaryGoalDone` changed, not necessarily that a reward was collected
  (`external/PluginSDK/include/D2RLPlugin/lifecycle_events.h:76-83`). Its row is
  used only to deduplicate within the session/difficulty, never guessed as a
  quest-name index. Other players and stale sessions are rejected.
- Speech uses the existing PRISM sink with `interrupt=false`. Dialogue, UI
  speech, pause and loss of game focus defer delivery. Counter changes while
  deferred coalesce to the latest reading. Journal selection/tab/close events,
  initial text and changes of quest identity establish silent baselines.
- Leaving/joining a game, disabling alerts or changing text locale discards
  pending speech. Native memory reads remain on the UI thread; no extra hook,
  unverified offset or new dependency was introduced.

`quest_progress.cpp` contains the text recognition and announcement policy;
`quest_service.cpp` adapts UI messages, lifecycle, native reads and speech.
Logs use `[quest] update event`, `primary goal done`, `watch` and `announce`.
The watch logs only changed snapshots, including `identified=0|1`.

Validation: full build and 37 CTest suites / 9266 checks pass. New suites cover
52 policy checks and 22 service checks, including missing panels, localization,
raw counters, mismatched quest text, navigation, duplicate events, deferred
speech, session teardown, toggles and partial subscription failure cleanup.

### Remaining live check

Trigger a quest update with NVDA active and the journal closed; inspect the
`[quest]` lines for one spoken alert. Check an NPC reward conversation and
verify that the goal announcement waits until the dialogue closes. If the
journal remains reachable, check the Den of Evil counter for changed objective
speech. Automatic generic alerts are implemented regardless of that result;
full objective/counter coverage while the journal is unloaded still requires
recovering the native quest-state reader. No live confirmation is recorded yet.
