# Quest log

How the mod names quests and reads objectives, and what is still open.

## Quest objects — 2026-10-03

The objects a quest needs used (the Compelling Orb, the Chaos Sanctuary seals,
the Altar of the Heavens, the Hellforge, Anya, the orifice, the Gidbinn altar,
Cain's Gibbet, the Kurast sewer lever and stairs) have the quest cue, their own
scanner category and priority for D. The seals are numbered and counted. See
[quest objects](quest-objects.md), which also lists the live checks still to do.

## Tal Rasha symbol — 2026-10-03

Selecting The Seven Tombs now appends the visible journal graphic's plain
symbol name to its objective; repeat includes it. The verified ImageWidget
frame is read only from the visible tree with matching quest details. The
seven entrance labels use that same vocabulary, including restored map places.
Native evidence, the mapping and validation: [Tal Rasha symbols](tal-rasha-symbols.md).
Built/deployed with the game closed; 96/96 suites pass. Live speech pending.

## Missing Cain quest and controller actions — 2026-09-22

The latest live session repeatedly focused `Quest3` while `ShortDescription`
held `The Search for Cain` and `LongDescription` held its full current objective.
The journal reader said `Empty`: its catalog named that same button `Tools of
the Trade`, so the stale-pane guard rejected the valid details. The quest update
event and background text identification both worked; the quest was present.

HD `QuestIcons` filenames number the displayed icon positions. They do **not**
always use the quest string-table numbering. The classic `MENU` filenames do:
both classic layouts associate `Quest3` with `MENU/a1q4`, matching the live Cain
title `qstsa1q4`. The generator now uses those classic layout indices. This
corrects Act I `Quest2`/`Quest3`/`Quest4` and Act III `Quest12`–`Quest15`; the other
20 mappings agree. The guard still excludes empty sockets and stale details.

Quest selections now include the game's current controller footer, with the
button and its action caption together. When that footer is unavailable, the
visible, enabled `Speech` button supplies its own caption (`playQuestDialogue`);
its layout action is `QuestLogPanelMessage:PlayDialogue`. No controller binding
is guessed. Action help is separate from progress, so a counter update does not
repeat it; manual repeat includes it. The detail sweep also excludes tab labels
(the stray `I` in live journal utterances) and duplicate title/objective text,
while retaining additional text and linked-text progress nodes.

Sources: `game-data/index/QuestLogPanel{Original,Expansion}.json.index.json`,
their HD counterparts, `game-data/global/ui/Layouts/QuestLogPanelOriginalHD.json`
(`Speech`), and the 2026-09-22 session in `d2r-access.log` (quest update at 22:44:35).
Validation: build/install succeeded; 67/67 CTest suites pass, including `quest-log`,
`ui-text`, and `game-text-reader`; installed DLL hash matches. The user confirmed
the journal correction works in live play on 2026-09-22.

## Only the quests the player actually has — 2026-09-19

Navigating the journal read out the **whole act**, icon by icon: on an Act 1
starter the player heard "Tools of the Trade", "Sisters' Burial Grounds" and
"The Forgotten Tower", quests that character has never been offered. The cause
is the catalog fallback below — a quest button is a nameless icon, so the mod
names it from the string table, and it did that for every socket in the tab
whether or not the game had put a quest in it.

**A first attempt tested the availability byte (`+0x50`) and did not work:** the
quest buttons report available whether or not the player has the quest, so the
guard withheld nothing. Do not try that offset again.

What does distinguish them is the shared detail pane. A quest the player has
populates `ShortDescription`/`LongDescription` when its icon is selected; one
they have not been given leaves the pane empty, on screen and in memory — the
2026-09-16 controller session recorded exactly that ("other selected quest
icons had empty detail panes"). So `UiText::Describe` now applies one rule:

> speak only while the pane's heading names the focused icon. Anything else —
> a pane still holding the icon focus has just left, or a socket with no quest
> in it — is reported as empty after the pane settles.

That one condition replaced two. The old code announced the new icon's name on
its own while the pane caught up ("retain ownership"), and *that* announcement
is the bug rather than a symptom of it: an empty socket carries a catalog name
just as a real quest does, and the pane takes about as long to clear as the
150 ms settle, so the name escaped before the emptiness was ever visible.
Fixing only the empty-pane case would have left the act reading out icon by
icon exactly as before. A quest is now announced once, whole, when the pane
resolves, and both paths out of the branch carry the settle.

Only the pane's own heading decides it. The readout also sweeps up whatever
else the panel holds, to reach the counter (below), but that sweep gets no vote
here: any furniture the panel happens to render would otherwise keep every
empty socket alive as a quest.

Empty sockets are reported as empty. Earlier silence released focus and let the
fallback reader say the panel title again, so the reader retains ownership.

The rule depends on the correct catalog identity. The Cain report exposed the
HD-icon numbering mistake fixed above. `questNoObjective` still covers a pane
that names the selected quest but carries no objective line.

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

## Reaching the kill counter (2026-09-16, working 2026-09-17)

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
**Confirmed live (2026-09-17):** the user hears the remaining monster count for Den of Evil with this
readout. The game does render it in that pane; reading the pane whole is what reaches it.

Which widget carries it is still unpinned. A per-selection `[text-tree]` dump was added to find out and
then removed: it fires on the first frame the quest icon takes focus, while the shared detail pane is
still empty, so its 1220 lines in one session showed only the tab and the quest buttons — never the
populated pane. The on-demand `RequestDump` remains for that question. Pinning the widget is not needed
for the readout, which takes whatever the pane holds; it would only allow a narrower read. No native
quest-state field or offset has ever been recovered and the SDK exposes only `QuestCompleted`, so
counting live monsters or inferring progress from the catalog stays forbidden.

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

The **classic** quest button's `MENU` filename supplies the string-table act/quest
pair: `Quest3` → `MENU/a1q4` → `qstsa1q4` (The Search for Cain), `Quest2` →
`MENU/a1q3` (Tools of the Trade), and `Quest4` → `MENU/a1q5` (The Forgotten Tower).
Act III `Quest15` maps to quest 4 (The Golden Bird), and `Quest12` to quest 1
(Lam Esen's Tome). HD icon numbers instead follow display positions; using those
produced the missing-quest bug above. Do not derive string keys from HD icons.

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
- The SDK's completion event identifies a quest-state row but carries no
  objective text or counter. Classic D2 quest flags alone cannot select the
  exact current journal sentence or its live number. The older
  `re/D2R.rebuilt.exe` has stale or misplaced data sections (`docs/decompile.md`),
  so missing string references there said nothing about the live quest path.
  The full mapped image exposed the stock builder and packet path described
  below; its captured text still needs live validation.
  The 2026-09-22 live log shows Cain's real objective with
  `QuestLogPanelExpansion closed` and `identified=1` at 23:03:12, but `no panel`
  and `identified=0` at 23:27:07: closing the panel is already supported;
  unloading it is the remaining gap.
- The older RE dumps in `re/` contain quest strings but do not place all data
  where the running code expects it. Their zero-reference results were not
  evidence that the stock quest builder lacked a readable call path.
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
- If the panel is missing, the guarded native quest-packet capture can provide
  the exact title and objective the game's own quest-text builder just produced,
  including its formatted counter. The title and objective must both match a
  unique localized catalog quest. Missing, ambiguous or unidentifiable text
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
  pending speech. Native memory reads remain on the UI thread.

### Native packet text capture (2026-09-23; live quest update pending)

In D2R 3.3.93787, quest-packet handler RVA `0x16C570` updates quest flags and
calls the game's text builder at `0x16D3EB` (`0x16BD20`, return `0x16D3F0`).
The builder's output holds the title string ID at `+0` and a bounded UTF-8
objective at `+2`; the game's formatter supplies live numbers in that text.
The plugin observes only this packet-handler call after the original returns.
It never invokes the builder itself: the builder also changes the game's
prior-step marker. The hook checks complete-function fingerprints, compares
the output title ID with the quest-record table (`0x1CDA7A0`, 41 records,
`0x18` bytes each), and rejects captures from another session. An update
alert accepts a recent, unique quest-row capture; different rows in one alert
window fall back to the generic message. Journal rendering calls are excluded
by the native return address, so visiting other quests cannot change the alert.

This identifies the source and establishes a safe capture path; it does not yet
prove that every quest update in live play takes that packet path. The previous
closed-panel reader still handles retained journal text and the generic alert
remains available. Confirm with NVDA and `[quest] packet objective` logs after
an update while the journal is unloaded.

The 2026-09-23 build passed all 67 CTest suites, including packet correlation,
late competing rows, session reset and toggle regressions. Live startup on
3.3.93787 verified and installed the hook; objective delivery still needs the
in-game update check above.

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
the new packet capture's unloaded-journal objective/counter coverage still
requires live confirmation. Quest changes that bypass the guarded packet call
continue to use the generic alert.

## The player's quest flags (found 2026-10-18, read in the image, not yet measured live)

The client keeps the local player's flags for the difficulty being played (`NativeQuestFlags`):
`*(base+0x2A48778)` is the quest record (null outside a game; made at `0x10B538`, cleared at
`0x10B44C`), its first field points to 0x60 bytes, one little-endian 16-bit word per quest
record row (the row the text builder `0x16BD20` takes; `0x1CCEE0` checks `[rec+0x14] == row`).
The getter `0x325C50(rec, quest, bit)` tests bit `quest*16+bit` through byte `n>>3` and the mask
table at `0x1D996D0`, filled at run time: the reader waits until it reads 1, 2, 4... before it
trusts a word. Packet 0x28 type 6 (handler `0x10E6E0`, copy at `0x10E715`) fills the words from
the server's `[playerData + 0x40 + difficulty*8]`; before it arrives every word is 0.
Bits: 0 completed, 1 reward pending, 2-6 the steps since the quest began (the server's status
function `0x5E1441` maps 2, 3, 4, 5, 6), 13 primary goal done, 15 done. The Forgotten Tower is row
5 (the server sets 5:13 and 5:0 at `0x5E1726`, after `cmp eax, 0x19`, Tower Cellar 5); its ways are
quest ways only while `QuestRunning` (`EntityRadar`), and always when the flags cannot be read.
Log: `[quest] flags row=5 word=XXXX` when the word changes; `[quest] Flag bit order verified.`
To confirm live: a new character's word before the tome (0), after it (bit 2), in the cellars,
after the Countess (0x2001); an in-progress character right after joining.
