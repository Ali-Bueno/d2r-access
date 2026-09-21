# STATUS — Diablo II: Resurrected (d2r-access)

> Per-mod status ledger. Open first when resuming. Dashboard, not docs.

## Terrain overview — 2026-09-21

The accessible automap: **Ctrl+O** or the **L3+R3 chord** speaks the level name, the player's place in it
(screen thirds: `borde izquierdo`, `centro`…) and, per screen direction, the distance to the level's outer
edge or to an already generated neighbour level, plus where unexplored terrain starts (rooms never streamed
near the player this session). No undiscovered level names, no routes (`docs/decisions.md`). A short R3
click stays the beacon, an R3 hold reviews the ways out, L3 is the survey; the chord is told from any of
them (`src/stick_chord.*`). `src/terrain_overview.*`, `src/explored_rooms.*`,
`src/terrain_service.*`, `docs/terrain-overview.md`. 46/46 CTest suites. Why it was needed: in Blood Moor
the exit review only names Cold Plains once the game generates it (~40 m from the border), by design.
RE of the act level list and the `Level` rect (`+0x24..0x30`, tiles) in `docs/act-levels-re.md`; the
overview uses the RoomEx union and only cross-checks the rect (`[terrain] … differs` warning).

**Next step:** listening check in Blood Moor with the controller: L3+R3 right after entering
(expect `Blood Moor, borde izquierdo. adelante: …`), walk toward an unexplored direction, repeat; confirm
the exit review names Cold Plains once ~40 m from its border. Judge the chord and the wording.

## Four radar readings, ported from the Among Us sonar — 2026-09-21

The radar now says what a route *means*, not only whether it is free: **exit** (tin: into another chamber
of a maze level, or to a level exit), **floor** (a soft breath: walkable, leads nowhere new), **wall**
(tock) and **door** (two knocks, the game's own DOOR bit). No dead band any more: one threshold each way
and the committed reading stands in between, so the first observation is always a verdict. The doorway
width is the level's own (median opening width of the streamed rooms, clamped to the two-tile opening
depth); the wall distance stays at the validated two tiles. The doorway just walked through is taken as
read instead of sounded behind the player; the cue queue plays walls and doors first, then exits, then
floor. `access-radar test` auditions all four. `docs/radar-readings.md`,
`reference/audio-navigation/among-us/passability-sonar.md`. 46/46 CTest suites.

**Next step:** listening pass. In the Den: walk a corridor towards a room (expect tin only once the
route really enters it, breath before), face a closed door (knocks, not tock), enter a room and check
the doorway behind stays silent. In the camp: tin only towards the Blood Moor edge or a waypoint,
breath elsewhere. Judge the breath's level under the game's ambience. `way_through=` in `[radar-motion]`
shows the derived doorway width.

## Exit review and room hysteresis — 2026-09-20

The ways out are reviewed one at a time, nearest first: an R3 hold (>= 350 ms, `StickChord::RightHoldMs`)
or Ctrl+V speaks `paso, 8 metros, a las 12, recto, 1 de 3` (a room opening) or the exit's own name; a
short right-stick click or Ctrl+B then guides to it. On maze levels the list is the room's openings plus the exits in it;
elsewhere, and off-grid, the level's exits. Outdoor boundaries (camp → Blood Moor, no warp unit) now come from the act's
level areas (`src/native_level_areas.*`, `levelBearings`); upstream's own RoomEx border walk was dropped
for it when `bladestorm` was merged. Room changes pass grimdark's hysteresis (settle 1000 / dwell 400, dwell only inside the first second
after a change), so a room is spoken on entering, not a second later. `src/exit_review.*`,
`docs/rooms.md`, `docs/beacon.md`, `docs/distant-exits.md`. 45/45 CTest suites.

**Live-verified 2026-09-20 (unattended, camp):** `Waypoint, 38 meters, 4 o'clock, around, 1 of 2` then
`Blood Moor, 136 meters, 4 o'clock, 2 of 2` (beyond the 96-cell route box, so no reach word); Ctrl+B locks
either with the same `reach=`. Hysteresis still needs the user (maze level, controller).

Ported from `grimdark` after a code-level pass that corrected our reference: its ping never calls the
pathfinder ("path" is inferred), exits never say "blocked", and the hysteresis is inverted. Our "around"
is verified by the route search. `reference/audio-navigation/grim-dawn/navigation-spec.md`.

**Pending (2026-09-20):** listening check with the controller. In the Den: enter a room (expect its ways out at
once), hold R3 or Ctrl+V (next way out), click R3 (guide to it). Judge the 1000/400 ms
hysteresis by ear. Known: `[items] exits nearby` counts warp units only, so it says 1 where the review
says 2.

## Collision semantics audited against D2MOO — 2026-09-20

Our grid reading is the game's: mask 0x1C09 read live, five-cell cross for the player, 0x27 for no
room, doors as live bits. Two divergences fixed: a unit's NO_PATH marker survived as terrain on water or
under a missile (one-cell false wall), and a refused displaced-stamp removal orphaned the player's own
marker, which is the false "down blocked" of 2026-09-16. A point pattern for the local player is now
rejected as a misread. The game's own path type for the player (STRAIGHT: ray, wall-follow 73 steps,
A* only within 18 subtiles, nothing past 100) is documented; it is not called, and our BFS is a
"where can I go" answer, not the route the game would take. `docs/collision-semantics.md`.

**Two more defects found by an unattended live run the same day and fixed** (`docs/radar-redesign.md`,
"Live analysis"): (1) a false **open** through wall pieces welded into a stone alcove — the clutter filter
erased any ≤5-cell component with a way round inside its window; now a component is clutter only if it
**stands alone** (no other blocking cell within body radius + 1 of its box). Fixture
`tests/recorded_alcove_grid.h` replays the live snapshot. (2) A wall entered from the dead band (7.8, between
close 7 and open 10) never cued, because the dead band left the silent baseline unclaimed and the first wall
was swallowed as baseline; the first available observation now claims it. Second live run: alcove reads
blocked in all four cardinals with the character immobile; the spawn wall cues once. Zero orphan stamps,
zero `pattern=0`, 4 cues in 9 moves, no crash.

**Next step:** the user's listening pass. Known limit, by design of the four screen cardinals: in a pocket
whose only way out runs along a world axis (a screen diagonal), all four read blocked; the exit review or
the scan is the answer there, not the radar.

## Room exits, in maze levels — 2026-09-20

Walking into a dungeon room speaks its ways out: screen directions for the plain openings, the
game's own destination name for a stairway or portal inside it. No segmentation algorithm — a maze
level (`Levels.txt DrlgType = 1`) is built from `LvlPrest` cells and one streamed room is one cell,
so D2 has already done it. Excluded elsewhere: in towns and the open acts the room partition cuts
across open floor and means nothing. Ctrl+M, `access-rooms`. `docs/rooms.md`.

An opening is only spoken when the cell across the border belongs to another **streamed** room:
inactive rooms have no collision grid at all (`Room1+0x38`, freed on unload), so a doorway into
ground D2 has not loaded yet stays unspoken rather than guessed.

**Next step:** listening check in the Den of Evil and the Crypt. Expect one line per room, about a
second after entering, never twice for the same room, and silence in the camp and the Blood Moor.
`[room] entered` in the log carries the rectangle and the opening count.

## Beacon says how you get there — 2026-09-20

Locking a target now ends with one word: straight, around, or no route. Three states out of data the
route search already computed and threw away (`Route.reachable`), plus a body-aware straight-line
test over the same field, so the two answers cannot disagree. Beyond the search box it says nothing
rather than claim no route. No new sounds, no new key. `docs/beacon.md`.

Ported from the Grim Dawn mod `grimdark`, whose review ping answers exactly this and nothing else:
`reference/audio-navigation/grim-dawn/navigation-spec.md` §6.

**Next step:** listening check. Scan a target and click the right stick in the camp (expect recto),
then through a wall (rodeando), then a sealed or far target. Check `reach=` in `[beacon] locked`.

## Container loops — 2026-09-18

Chests, caskets, urns, barrels and corpses loop through the item mixer. Ctrl+K,
`access-items chests-on|chests-off|chests-dump`, `[chests]` in `radar.toml`. Opening one silences
it: the game stops letting it be clicked. Container rows come from the game's own `objects.txt`
`OperateFn`, and `chests-dump` checks the live row counts against the tripwire in
`src/entity_layout.h`. `docs/ground-items.md`.

**Next step:** `chests-dump` in the Rogue Encampment, Cold Plains and the Crypt, against rooms whose
contents are known; open one and hear the loop stop; judge the 0.59 s sample's texture.

## HUD labels gated by sight — 2026-09-18

The monster health plate and the world target label are dropped when every entity of that name
within reach is behind a wall, so they agree with the right-stick scan. The NPC announcer defaults
to `line_of_sight = true` for the same reason. `[target]` in `radar.toml`. `docs/target-sight.md`.

**Next step:** controller check in the Den — names in the next room stay silent until the doorway
opens, `[interaction] behind a wall` in the log. Watch for the opposite failure: our line of sight
uses terrain walkability, so a see-through fence hides what is behind it until `BLOCK_LOS` is
verified, and the camp is fenced.

## Loader 1.3.0-beta — issue #1 — 2026-09-17 (live-verified)

A collaborator reported the whole native layer down on **D2RLoader 1.3.0-beta**, the only build
`d2rloader.net` still distributes (issue #1). It turned out to be **four** independent causes, plus a
latent field-width bug. All fixed and confirmed on a live 1.3.0-beta run.

- **Localization.** The SDK was pinned at tag v4 while upstream had moved on. `LocalizationService`
  is the **only** one of the 18 services whose ABI went from 1 to 2 (namespaced strings), so 1.3.0
  refused a v1 query and panel titles fell back to internal names. Submodule now at `6fbabfc8`; the
  renames (`XServiceV1` -> `XService`, `QueryService(&svc)` deriving id and ABI from the type,
  `apiVersion` -> `abiVersion`) applied across the 14 files that query a service.
- **Namespaced string tables (the regression this first caused).** ABI 2 did more than rename the
  service: a bare key now searches the namespace named by `PluginInfo.id`, i.e. the plugin's own,
  which holds nothing. Every key the mod looks up is one of the game's, so `Localization::Text(key)`
  asks for `d2r:<key>` (SDK `localization.h`: "Use d2r:Key or d2rloader:Key for game or loader
  text"). Until that landed, *every* key lookup returned empty with no warning at all — the service
  queried fine — so panel titles, character creation and quest names all fell back to internal names
  ("quest 0", "quest 1") and the speech-language probe silently defaulted to English. Confirmed
  fixed live: the probe now resolves (`probe "close" = "close"`). A mechanical rename was not enough;
  the semantic change shipped in the same commit.
- **D2RCore slots, all six.** The four widget slots *and* the two the sonar uses were hardcoded
  offsets into the loader's own DLL, which move with every D2RCore build. They are entries of a
  table of 0x18-byte descriptors whose `+0x08` carries the **game RVA** the loader resolved the
  entry from — a fact of the game build, not the loader's. `src/core_slots.h` finds each descriptor
  by that RVA. Verified against both DLLs offline and live: it reproduces the six offsets that were
  hardcoded for 1.2.1-beta and yields 0x67DDC0, 0x67DDF0, 0x67F4D0, 0x680AA0, 0x680D28, 0x683810 on
  1.3.0-beta.
- **The sonar was failing silently.** `NativeWorld::Initialize` returned false with no log line when
  its slots did not resolve, so the collaborator's report could not show it — the sonar was down on
  1.3.0 too, invisibly. It now says so.
- **The fingerprints were hashing the loader's own patches.** D2RLoader redirects some of the game's
  calls into code it allocates at run time, in a block **past the image's `SizeOfImage`** — an
  address that is not part of the game and need not repeat between launches. `LayoutCheck::Hash` now
  zeroes the 4-byte operand of every `E8`/`E9` before hashing. Under that rule **57 of 59 checks are
  byte-identical across the two loader builds**; only the two the loader genuinely patched differ.
  All 60 stored hashes were re-derived (`tools/layout_fingerprints.py`).
- **Latent bug found on the way: `ObjectData+8` (a portal's destination level) widened from uint8 to
  uint16** on 1.3.0 — the game's own label builder now consumes it as `ax` (0xC17E0+167 `b6`->`b7`)
  and 0x34AD40's byte getter is hooked. The offset is unchanged; the field grew into its padding.
  `native_entities.cpp` read it as `uint8_t`, which truncates any level id above 255. Harmless on
  vanilla (highest is 137) but wrong, so it is now a `uint16_t`.
- **0x2F5020 (`UNIT_GetStat`) is fully detoured** (`jmp [rip+...]`). The reader only ever *calls* it,
  so its body is the loader's, not ours to fingerprint: the entry is marked `mayBeDetoured` and a
  thunk is accepted and logged instead of re-hashed.
- **Diagnostics.** The three layout guards each duplicated the FNV-1a loop and named only the
  **first** mismatching RVA, which is why reproducing the report needed a debugger. `src/layout_check.h`
  holds one check that names *every* changed or unreadable RVA. The SDK's `buildVersion` is the
  **game's** build ("3.3.93787") and the API reports nothing about the loader, so `CoreSlots::LoaderVersion()`
  reads D2RCore's own file-version resource; the first log line is now
  `d2r-access 0.1.0 on game 3.3.93787, loader 1.3.0-beta (plugin ABI 4)`.

- **Missed on the first pass, caught by a collaborator (PR #2, `e83d444`).** `plugin.rc` still used
  `D2RL_PLUGIN_API_VERSION`, which SDK 0.2.0 renamed, so a **clean** build died at the resource step
  (`RC2104: undefined keyword or key name: D2RL_PLUGIN_API_VERSIONL` — `D2RL_PLUGIN_RESOURCE_DWORD`
  pastes `##L` onto the unexpanded name). Incremental builds never recompiled the `.rc`, so none of
  the verification here could see it: `main` was broken for anyone starting from a fresh clone.
  **Verify an SDK bump with a deleted build tree, not an incremental build.**

Build succeeds, **42/42 CTest suites pass** (two new `sdk` suites come with the updated SDK).
Live 18:56 run: descriptors resolved, ground items, monster/object/exit scan, sonar, tooltips,
skill grid and footfall all verified, no layout warning, localization up. Install SHA-256 recorded
in the build log. Only remaining warning is the pre-existing transient
`UI pump: runOnUiThread returned 4; will retry`.

**Next step:** answer issue #1 (not posted yet) and have the collaborator confirm on their install.

## Button hints follow the item they act on — 2026-09-17 (third pass)

User feedback on the second build: the controller hints ("A: pick up, X: drop")
were heard **before** the item, because an occupied cell speaks as soon as the
focus moves while the game's own tooltip is still ~350 ms away, and the hints
went out in that first utterance. They are now held back whenever an occupied
cell or slot is still waiting for its description, and are appended to the
utterance that carries it, so the order is always item, then position, then what
you can do with it. A cell that never renders a description therefore no longer
speaks its hints at all — the trade the user asked for.

Build/deploy succeeds, **40/40 CTest suites pass**; the two merchant-action
expectations that encoded the old order were updated. Install SHA-256:
`442FC7E5694176BB64CA62B39C6C487068AF82DDC02EA118EFEDE975CC855A7A`.

## Character panel, equipment slots and placement — 2026-09-17 (second pass)

From live feedback on the inventory screen:

- **Equipment slots no longer go silent.** `NativeUi::EquipmentLocation`
  returns 0 for every failure alike, so a missed read during a panel rebuild
  simply dropped the slot name, and when `EquipmentItemPresence` was `Unknown`
  in the same poll the whole move was silent. A slot's location never changes
  and widget names survive rebuilds, so the last location seen under each slot
  name now stands in for a failed read (logged once per slot), and a slot with
  nothing else to say announces the generic equipment word instead of nothing.
- **The character panel reads.** New `CharacterStats`: the controller layout
  makes each `<name>Title` focusable through a `Tooltip` child, and only the
  four raisable attributes had a reader. Every row now speaks its label and its
  value — defence, stamina, life, mana and the four resistances included, with
  current/maximum joined as "45 of 120". Values resolve generically as the
  title's stem plus `Stat`/`Text`, so unlisted rows still read; only
  `ArmorClass`, the three current/maximum pairs and the four resistances need
  the table. The body carries the panel name plus character, class and level, so
  entering says the header once and navigation then reads one row at a time —
  the "header, then navigate" the user asked for.
- **The mini-menu buttons have names.** `MiniMenuCharacter` and its six
  siblings carried a null label because the layout gives them only an icon.
  `tools/gen_panel_widgets.py` now maps a `MiniMenuButtonWidget`'s own `menu`
  field to the game's matching `minipanel*` key; exactly those seven catalogue
  entries changed.
- **Releasing an item says where it went** (`itemPlacedIn`): the slot name for a
  paperdoll slot, row and column for a grid cell, the belt for the belt —
  instead of a bare "item released". This also covers using an identify scroll,
  which on controller is the same carry-and-release mechanic.

Two new phrases in all 13 locales. `./build.ps1` succeeds and deployed the
plugin; **40/40 CTest suites pass** (new `character-stats` suite). Build/install
DLL SHA-256 matches:
`F45144E2BA0B29A162E2B46D48FAD569564824C5195766C1D07D645C1CBFC858`.
`docs/character-panel.md`.

**Known gaps, not fixed:** the belt is a single `BeltWidget` grid, so which of
its four cells took a potion is still unknown — placement says only "belt".
Assigning a potion or the belt to a controller button, and throwable flasks,
have no reader. The mouse-side `ContextMenu` (`OptionsList`, filled at runtime)
is unread, so the right-click identify/use rows are not covered.

## Menu fixes, life/mana and broken equipment — 2026-09-17

Four items from the audio demo feedback:

- **Grid position is spoken last.** `UiText::Readout` gained its own
  `coordinates` field, appended after the body, the option and the description,
  so "fila X, columna Y" (and a skill's branch position) no longer lead an
  announcement. It affects equipment slots and the skill tree, where they came
  first; described inventory cells still omit them entirely, as before.
  `ProgressionText::SkillReadout` no longer takes a position argument.
- **Item actions beyond the primary one.** On controller the vendor panel has
  no Buy/Sell/Repair buttons at all: `Controller/VendorPanelLayoutHD.json`
  drops them, and every per-item action becomes an entry of the
  `ControllerOverlay` footer — a `ButtonLegendScrollWidget`, a different class
  from the `ButtonLegendWidget` whose `+0x88` entry vector the reader knew.
  `NativeControllerHelp::Legend` now falls back to the sub-text slots the
  layout declares (`Legend1`..`Legend8`, walked until one is missing) when that
  vector yields nothing. No new offset. **Live check pending** — this is the
  likely cause of hearing only "equipar", not a confirmed one.
- **Life and mana by steps.** New `VitalsService` + pure `Vitals`
  (`access-vitals on|off|status`, on by default): steps 75/50, 40/30/20/10,
  5/4/3/2/1; crossing one speaks the real percentage once, in either direction,
  interrupting at or below 10 %. Values come from the game's own HUD orbs
  (`HUDPanel/HealthBall/ValueDisplay`), so the *show life/mana orb text*
  gameplay options must be on; with them off the mod says so once, naming those
  options with the game's own labels. No stat id, offset or fingerprint added.
- **Broken equipment.** The loader's inventory service (`getLocalPlayer` +
  `forEachInventoryItem` filtered to `ItemContainer::Equipment`) is swept once a
  second; an item whose durability reaches zero from a positive value is
  announced with its slot. Indestructible items and items worn already broken
  are silent.

Six new phrases in all 13 locales. `./build.ps1` succeeds and deployed the
plugin; **39/39 CTest suites pass** (new `vitals` suite, 33 checks; new
scrolling-legend and coordinate-order cases). Build/install DLL SHA-256 matches:
`2F99F7AE08F2B99E23C4AC8F3CB813A6CF7AD09A6061EF70C43CB06F0B2CEFAB`.
Live NVDA/controller confirmation pending for all four.
`docs/vitals.md`, `docs/inventory-npc-followup.md`.

## Three live bugs fixed — 2026-09-16

From the 22:00-23:08 controller session, all three diagnosed against the live log:

- **Foreign-level exits (walked into a wall).** In D2R the far side of a level
  passage is a neighbour room of the player's room. The native unit walk read it
  with no level check, so inside the Den of Evil the scan offered the Blood-Moor
  side "To The Den of Evil" 2476 subtiles away; the beacon clamped that
  unreachable goal to the nearest cell in its search box and guided into solid
  ground. `native_entities.cpp` now keeps only neighbour rooms whose Level
  (Room1 -> RoomEx -> Level) is the player's. The distant/preset walk was not at
  fault - it already refuses foreign rooms. `docs/level-exits.md`.
- **"An evil force" every few steps** (300 times in that session: 221 proximity,
  79 scan). 42 monstats rows share the placeholder `NameStr` "dummy" - camp rogue
  scouts, the Den's evilhole spawners, critters, town extras - which the string
  table resolves to ui.json id 5382, a label the game never shows. `ReadMonster`
  now drops any unit carrying it; every announce path funnels through there.
  Flags alone cannot separate them (rogue1/act2guard1 are npc/killable yet
  unnamed; real NPCs are npc **and** interact). `docs/entity-scan.md`.
- **Den of Evil kill counter never spoken.** The mod never obtained it: the
  journal readout kept only `ShortDescription`/`LongDescription` and discarded
  the rest of the pane, and the counter value appears nowhere in the 28 MB log.
  The readout now speaks every other text node of the quest panel in screen
  order (static heading and quest-icon labels excluded). **User-confirmed live
  on 2026-09-17: the remaining kill count is spoken.** Which widget carries it
  is still unpinned - a per-selection tree dump was tried and removed, because
  it fires while the shared pane is still empty - but the whole pane is read, so
  the value arrives wherever the game puts it. `docs/quests.md`.

Build/deploy succeeds, **38/38 CTest suites pass**; four new checks (cross-level
neighbour room, placeholder-named units). Install SHA-256:
`6942BC59FE38F190A25A5720B3950FB9121180A2EE7EF04A0157A7E9FDCB644C`.
Live re-check of all three pending.

## Journal details and distant stairs installed — 2026-09-16

Quest selection now reads one title with its current objective, waits for the
shared detail pane to populate, suppresses stale text from another selection,
and speaks same-quest progress/counters without repeating the title or interrupting.
Blank objectives explicitly report missing text in all 13 mod locales. The live
21:44 controller session confirmed Den of Evil's title/objective and Akara's
narrative; the other selected quest detail panes were actually empty on screen.
Current counters come from the game, never inferred from the catalog.

Fixed stairs/level passages bypass the scan's range and walls. A read-only
logical-room/preset walk adds inactive-room exits with known destinations.
Coordinate identity preserves cycling/beacon locks across unit recreation.
Ordinary targets and proximity loops keep their previous range. Six new full
native-function fingerprints guard the layout. Never-generated preset data is
still unknown; guidance still uses local collision, not a complete unexplored
route. `docs/distant-exits.md`, `docs/quests.md`.

Build/deploy succeeds, **38/38 CTest suites / 9353 checks pass**. New native-exit
suite covers 22 checks; scan, native entity, UI, reader integration and loop
regressions also pass. No new dependency or game/config modification.
Build/install SHA-256:
`02D09EA88FE150C2E889D566DF8B646ADEB9FBB020619020D139EC07D4500007`.
User-led controller/NVDA check of this build and dungeon exit coverage pending.
Startup at 22:08:39 confirms `[scan] Verified native monster/object/exit reader`:
all original and six new native guards pass in the running installed game.

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

- **Engine / framework:** native C++ x64 (D2R.exe, patch 3.3) / **D2RLoader** (closed-source loader, **v1.3.0-beta 2026-09-14**, https://d2rloader.net) + its open **PluginSDK** (C++20 header-only, MIT, cloned at `../external/PluginSDK`, pinned at `6fbabfc8` / SDK 0.2.0, plugin ABI v4).
- **Screen-reader transport:** plugin DLL → `LoadLibraryW(<pluginDirectory>\prism.dll)` + `GetProcAddress` (load in `D2RLoaderLoadPlugin`, never in `DllMain`).
- **Build command:** `.\build.ps1` (CMake 4.2 + Ninja + MSVC Build Tools 2022 14.44; vcvars64 from `C:\Program Files (x86)\Microsoft Visual Studio2\BuildTools`). POST_BUILD copies `d2rl-d2r-access.dll` + `prism.dll` into the game. PRISM v0.18.2 vendored in `third_party/prism/`.
- **Mod install path:** `D:\games\steam\steamapps\common\Diablo II Resurrected\d2rloader\plugins\` (global) — D2RLoader zip extracted into the game folder, launch `D2RLoader.exe`.
  The loader is **three** files in the game root, all dated 2026-09-14: `D2RLoader.exe` (23,590,248), **`D2RCore.dll`** (18,345,304 — resolves the game pointers the widget and world readers depend on) and `d2rloader.mpq` (10,816,549). Both builds we have are kept outside the repo in `../loader-backups/` with their hashes and restore notes; **1.2.1-beta can no longer be downloaded**, that copy is the only one. Everything else in the root is the game's. Packaging the mod for someone else needs all three, plus `d2rloader/plugins/d2rl-d2r-access.dll`, `d2rloader/plugins/d2r-access/{prism.dll, sounds/item loop sounds/*.wav}` and a `d2rloader/config/d2rloader.toml` with the console enabled. Never ship `D2R.text.bin` (dumped game code, regenerated on first run).
- **Run / test:** Steam D2R 3.3.93787 (app 2536520, Spanish) installed. D2RLoader 1.3.0-beta installed into the game folder 2026-09-17 (SHA-256 verified). Start NVDA, launch `D2RLoader.exe`. Loader config (`d2rloader/config/d2rloader.toml`): `enable_console = true` (Ctrl+` in-game), developer logs on with `widget_panels` + `extension_details`. Logs: `d2rloader/logs/d2rloader.log`. Offline/single-player only (online ban risk undocumented).

## Section status

| Section / feature | Status | Notes |
|---|---|---|
| Adapter + Speech (PRISM) | done | NVDA backend; all PRISM calls on a dedicated speech thread (`screen_reader.cpp`), `Speak` only queues. Console `access-say` |
| Mod localization (13 locales) | built and installed - live check pending | Locale headers, game-string language detection, beacon/quest messages and missing-objective text; 5763 checks pass. |
| Quest log / automatic quest tracking | counter confirmed live 2026-09-17 | Whole detail pane read in screen order (2026-09-16 fix), so a counter rendered anywhere in it is spoken; panel tree dumped once per selected quest to find where the game writes it. Current objective attached to selection, no duplicate title; explicit missing-objective text. Generic update/primary-goal alerts remain. Unloaded journal coverage limited. `docs/quests.md` |
| Room exits (maze levels) | built 2026-09-20, listening check pending | Ways out of the native room the player walks into: screen directions plus named stairs/portals. Maze levels only, from `src/generated/level_flags.h`. Streamed rooms only — an unloaded neighbour is not claimed. Ctrl+M, `access-rooms`; review on an R3 hold or Ctrl+V. `docs/rooms.md` |
| Target beacon guidance | reach verdict added 2026-09-20; listening check pending | Says recto / rodeando / no se llega when it locks, and nothing beyond the search box. Only rooms of the player's own level are walked, so a passage's far side no longer offers its exits. Scan then right-stick click / Ctrl+B. Logical-room presets expose existing inactive-room stairs; stable position identity. Never-generated exits and unexplored routes remain unavailable. `docs/distant-exits.md`, `docs/beacon.md` |
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
| Friendly NPC announcer | line of sight on by default 2026-09-18; live check pending | Nearest friendly NPC spoken once when in range (15 subtiles, 3 hysteresis), distance + screen direction, no interrupt; silent in menus/dialogue/UI speech. Ctrl+N, `access-npc`, `[npc]` in `radar.toml`. `docs/npc-proximity.md` |
| Sonar radar | wip — 2026-09-16 redesign installed, incl. 18:10 forensic fix; live listening pending | One confirm-by-travel state machine per direction; connected-passage scan is now the only classifier, with 1-cell lateral tolerance (was 3), openDepth 10/close 7 (was 8/5), clutter removal capped at 5 cells (was up to 20), and `stick=` logged per cue. Four screen-cardinal directions and the original tin/tock sound design are kept — an eight-direction/new-sound-design attempt was tried live and reverted after the user listened. `docs/radar-redesign.md` (current model); `docs/sonar-radar.md` and its follow-ups are superseded history. |
| Connected navigation research | done locally - runtime integration built | Native body graph's bounded directional passage query (`ScanPassage`) is now the radar's only classifier, driving all four directions. No persistent doorway IDs/full-map guidance. `docs/D2RNavigationResearch.md`, `docs/radar-connected-passages.md` (historical), `docs/radar-redesign.md` (current use). |
| Stalled local footsteps | wip - second sliding case verified; suppression pending | 23:52:33/40 grids show up/right blocked with real travel along world X; +11.008728 then -9.091965 X in five-second intervals. No stalled decisions. No filter change; Ctrl+T now distinguishes recent movement from straight-ray blockage. Full stationary suppression remains unverified. `docs/footsteps.md` |
| Local computer use | done — initial live check | Python stdlib capture + bounded Windows inputs; menus, scroll, Ctrl+T and four movement directions verified with NVDA active. F9 up, F10 right, F11 down, F13 left assigned through game UI. Twelve mocked-input tests pass. `tools/computer_use/README.md`; no audio capture/analog controller emulation |
| Radar cue delivery log | wip — log format updated for the redesign | `[radar-audio]` still retains ID/origin through queue, submission, completion, cancellation or drop (no retries now); `[radar-direction]`/`[radar-state]` replace the removed opening-hold/wall/guidance/reference lines. Live validation of the new log pending. `docs/radar-redesign.md` (current fields), `docs/radar-event-log.md` (superseded) |
| Radar / footstep automated tests | done (local) | Six radar suites / 1439 checks against the confirm-by-travel engine (state machine, corridor stability, recorded camp/return grids, snapshot RLE, audio, readout), plus footsteps. Expectations derived from geometry, named constants and captures; timing printed, never asserted. |
| Automated text tests | done (local) | Nine CTest suites, 255 checks: text/notifications, guarded memory/tree and cell queries, gameplay lifecycle, tooltips, action help, equipment locations, delayed item speech without coordinates, shared tabs and a deliberately blocked SDK log sink |
| Highlighted NPC/object labels | wip — sight gate built 2026-09-18, live check pending | Shared primary tooltip capture plus visible target-name HUD, now dropped when every entity of that name within reach is behind a wall. `[target]` in `radar.toml`. `docs/native-tooltips.md`, `docs/target-sight.md` |
| Container loops | built 2026-09-18, live check pending | Chests/caskets/urns/barrels/corpses through the item mixer; opening one silences it. Rows from `objects.txt` `OperateFn`, tripwire checked by `chests-dump`. Ctrl+K, `[chests]` in `radar.toml`. `docs/ground-items.md` |
| Character panel (attributes, life, resistances) | built and installed 2026-09-17 - live check pending | Every `<name>Title` row reads its label and value through its focusable `Tooltip` child; header (panel, character, class, level) spoken once on entry. Mini-menu opener buttons named from the game's `minipanel*` keys via the generator. `character-stats` suite. `docs/character-panel.md` |
| In-game panels (inventory, skills, stash) | wip — basic selection and fluency confirmed | Shared item/grid archetype, native row/column; shared tab reader covers controller strip and unfocused stash changes (tab speech unconfirmed); skills/full inventory operations remain pending |
| Inventory actions / equipment slot names | wip — slot-name memory + placement destination installed 2026-09-17, live check pending | User confirms available slots read while holding equipment and prefers this navigation. Added SDK-driven pickup/release context notices. Controller item/vendor actions live in the `ControllerOverlay` `ButtonLegendScrollWidget`, whose sub-text slots are now read when its entry vector is not the one `ButtonLegendWidget` uses. Live notice/repair check pending. `docs/inventory-npc-followup.md` |
| Attribute allocation / skill-tree reading | wip — descriptions/actions confirmed by user | Stock hover refresh fixes repeated skill descriptions. Current binding, available action buttons and position/count precede the full description. All 30 live Barbarian class cells are reachable in regression tests. Live log distinguishes lower-row skills and reads Porrazo's A binding/X reassignment; user confirms it works. Zero points prevents live spend-action validation. `docs/skill-actions.md` |
| Item readout | wip — coordinates moved last 2026-09-17; blacksmith axes and coordinate suppression confirmed | Full rendered descriptions, stale cell rejection; occupied grids wait for the stock tooltip and read it without row/column. Empty/unknown cells keep coordinates. Full inventory/storage operations remain unverified |
| Nearby ground item speech | wip — native live detections observed | Localized base name/quality and gold amount within 3 subtiles; once per approach, rearm beyond 6; Ctrl+I. Grammar tags removed after live bolts/arrows evidence. Full generated equipment names remain outside this reader. `docs/ground-items.md` |
| Level exit loops (portal) | prior build installed - live detections logged; listening check pending | Type-5 warps + portal/waypoint/gateway objects as `Kind::Exit`, `portal.ogg` through the item mixer, Ctrl+P. Latest 19:16 session records nearby exits; audible placement and dungeon transitions remain unconfirmed. `docs/level-exits.md` |
| Life, mana and broken equipment | built and installed 2026-09-17 - live check pending | `access-vitals on/off/status`, on by default. Steps 75/50, 40/30/20/10, 5/4/3/2/1; crossing one speaks the real percentage once, interrupting at or below 10%. Numbers read from the HUD orbs' own `ValueDisplay`, so the game's show-orb-text options must be on (the mod says so once, using the game's labels). Worn items reaching zero durability announce their slot, via the loader's inventory service. No new offset or stat id. `vitals` suite, 33 checks. `docs/vitals.md` |
| Spatial item loops | wip — audibility confirmed; quieter landing-only follow-up installed | Eight supplied loops; fixed screen axes, distance, rear pitch 0.94; Ctrl+L. User confirms sound but requested lower level. Near gain 0.4 and pile limit 0.5; native falling mode excluded, no landing timer. 100 audio checks plus native landing transition regression. `docs/ground-items.md` |

## Derived facts (source: PluginSDK headers, path:line relative to external/PluginSDK/include/D2RLPlugin)

| Fact | Value | Source |
|---|---|---|
| Required exports | `D2RLoaderGetPluginInfo`, `D2RLoaderLoadPlugin(const PluginContext*)`, `D2RLoaderUnloadPlugin` | lifecycle.h:7-9 |
| Plugin flags | `PluginFlags::Client | NativeHooks` for the primary tooltip draw hook | lifecycle.h:13-20 |
| Service lookup | `ctx->QueryService(&svc)` — the pointer type carries `Service::Id` and `Service::AbiVersion`; every service is ABI 1 except `LocalizationService`, which is 2 | context.h:172 |
| Threads | UI thread vs game thread; **no per-frame callback** → self-requeue `runOnUiThread` as pump; all widget calls UI-thread only | threads.h:40-41, widgets.h:62 |
| Widget API (all of it) | `findPanel`, `findWidget(parent,name)`, `getWidgetRect`, `setWidgetVisible`, `setWidgetEnabled`, `dispatchUiAction` — **no tree enum, no text, no focus** | widgets.h:55-60 |
| Only game-UI event | `UiMessageEvent{targetHash, commandHash, target, command, text}`; stock panels use `PanelManager:TogglePanel/ClosePanel:<name>` | shared_events.h:91-131 |
| Localization | `getStringById(id 0-65535)`, two-call BufferTooSmall. **Keys are namespaced since ABI 2**: a bare key searches the namespace named by `PluginInfo.id` — this plugin's own, which is empty — so the game's strings are `d2r:strCancel` (still without `@`). A bare key returns nothing and warns about nothing | localization.h:29-31 |
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

- `docs/research-roadmap-chatgpt.md` (+ `-extended.md`, 744 lines) — user-supplied research roadmap (ChatGPT, 2026-09-14). No offsets/RVAs anywhere; focus tracking is its declared biggest unknown (extended §4.5, line ~171): experiment list + external repos to mine for later phases (MapAssist / D2RMH for unit tables & map data, diablo-access, D2RL-Plugins). Its C#-companion-via-IPC idea is superseded: PRISM speaks in-process.

- User-supplied `docs/sonar-radar-implementation-spec.md` v2 drove the first radar prototype. Native map evidence, parameters, controls, limits and controller test: `docs/sonar-radar.md`.

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

Live-check the 2026-09-17 changes, in this order:

0. In the inventory: cross the equipment slots repeatedly — the slot name must
   never disappear now (if it does, `[interaction] equipment location
   unreadable` in the log says which slot). Pick an item up and drop it: it
   should say where it landed. Open the character panel with its mini-menu
   button (the button itself should now be named) and move through the rows:
   expect the header once, then one row at a time.


1. Turn on *show life orb text* and *show mana orb text* in Gameplay options
   (with them off the mod says so once). Take damage and spend mana: expect a
   reading around 75 %, 50 %, then every 10 % and point by point from 5 %, each
   said once; at 10 % or below it interrupts. `[vitals]` in the log.
2. Wear an item down to zero durability: expect its slot announced once.
3. Open the inventory and the skill tree: "fila X, columna Y" must now come at
   the end of the announcement, never before the item, slot or skill.
4. At the blacksmith, highlight an item and listen for the full action list
   (buy/sell/repair), not only the primary action. If it is still only one
   action, send the `[interaction]` lines: the scrolling-legend fallback is a
   hypothesis, not a confirmed fix.

Then continue the pending live checks already listed below.

With the new build, select Den of Evil and confirm one title/objective utterance;
other blank quests should explicitly report no objective text. A visible counter
change should read only the counter. Scan known stairs from over 15 subtiles,
including behind a wall, then click the right stick / Ctrl+B and verify guidance.
Confirm native `[scan] Verified` startup and check `[scan]` / `[beacon]` logs in
multi-floor dungeons. No game-control input during the user's test.

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
