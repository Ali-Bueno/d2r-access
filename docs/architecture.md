# Game and plugin architecture

For the current complete feature/source map, use the
[feature index](feature-index.md). Shared input/settings/localization and output
ownership are detailed in [runtime services](runtime-services.md). The
[2026-10-19 integration record](upstream-2026-10-19.md) distinguishes retained
keyboard behavior from upstream additions and pending runtime confirmation.

Source survey, 2026-10-03, local `keyboard-local` checkout. This is a map of the
current implementation and the game structures it reads. Feature documents
retain their detailed evidence and validation history. Start with the
[workspace handoff](../../HANDOFF.md) and [status](../STATUS.md) for current work.
The older maintainer guide and
older sections of the ledger describe earlier controller implementations.

## Runtime boundaries

`d2rl-d2r-access.dll` is a native C++20 D2RLoader client plugin, ABI 4. The loader
hosts the game and supplies lifecycle, input, UI-message, localization, threading
and hook services through `external/PluginSDK`. `plugin.cpp` wires services,
registers console commands and controls their lifetime. Behavior belongs in the
services and their smaller policy modules.

There are two important game interfaces:

- The modern native widget tree handles menus, inventory, dialogue, the journal
  and HUD. The SDK's widget handles are name paths, not native object pointers;
  it does not expose the current text or actual focus. `native_widgets.cpp`
  reaches the game's PanelManager and lookup functions through D2RCore.
- The gameplay simulation uses typed units, paths, streamed collision rooms,
  logical rooms and the act's dungeon generator. World readers copy this state
  into plugin-owned values. An object's name alone does not establish whether
  it is an exit, container, quest mechanism or teleport pad.

The plugin does not have an independent authoritative game simulation. Offline
play also contains server-side game functions in the mapped executable, but the
readers use client-visible world state. Quest solutions should be exposed through
the game's displayed information, such as the visible journal image, rather than
by querying server quest state.

## UI updates and workers

`UiThreadPump` queues one callback through `ThreadService::runOnUiThread`, then
requeues after each root-widget update. `plugin.cpp::OnUiTick` is the authoritative
service order. Input is sampled first; `KeyboardService` and `KeyboardMenu` post
gestures before menu/text readers consume them. `GameTextReader::Tick` runs before
the older `FocusTracker` and returns whether it owns UI speech. That ownership
also defers ambient and quest announcements. Interaction/tooltips are handled
within the text-reader path. World, quest, radar, catalogue and combat services
follow in the same update.

`callback_boundary.h` contains C++ exceptions at that SDK `noexcept` callback.
An allocation failure unwinds the temporary snapshot, cancels keyboard mod
movement/queued actions through the shared cancellation helper, and waits one
second before retrying. It never returns a false empty/fully-explored map or
clears learned map data. A mod-side notice is queued on a later attempt.
Timing-destructor formatting uses fixed stack storage; `AsyncLog` enqueue is
best effort and cannot throw during recovery or unwinding.

Native widget access and gameplay commands run on this thread. Hook callbacks
must copy bounded data and respect their producer's thread; quest packet captures
use atomics/session epochs and a protected cache. Do not move native traversal
onto a background worker merely to reduce an update's duration.

| Worker | Owns | Producer boundary |
| --- | --- | --- |
| `screen_reader.cpp` | PRISM backend and synchronous speech calls | `Speak` queues owned text |
| `async_log.cpp` | Runtime SDK log delivery, which flushes disk | Callbacks enqueue bounded messages |
| `item_audio.cpp`, radar/beacon/entity audio modules | Audio rendering and buffer/device management | Services submit copied sound state |
| `explored_store.cpp` | Exploration archive I/O and migration | `TerrainService` submits room/place snapshots |

All static thread owners, and the terrain service's archive-store owner, use
`ProcessLifetime<T>` (`process_lifetime.h`). Normal service unload still stops
and joins workers; terrain then resets the owner to release its archive cache.
The trivially destructible wrapper performs no cleanup under the loader lock
on forced process exit. Windows reclaims process resources. A normal DLL unload
still requires `D2RLoaderUnloadPlugin` first; never replace that lifecycle with
a wait/join in `DllMain`. Local/test `ExploredStore::Store` RAII is unchanged.
See [stability repair behavior and tests](stability.md).

`NativeMemory::ReadScope` shares page-permission checks for one synchronous read
pass. It caches permission information, never game contents or pointers between
updates. `NativeMemory::Copy` still contains access faults. The `[pump]` timing
report names slow services and page checks, making it the first place to look
for UI stalls. See [navigation latency](navigation-latency.md).

## Native access and build guards

`CoreSlots::Resolve` scans D2RCore's writable PE sections for descriptors carrying
known **game RVAs**. The descriptor locations change between loader versions;
hardcoded offsets into D2RCore are obsolete. A descriptor must occur once and
resolve to an appropriate game address. See `core_slots.h` and
[native UI](native-ui.md).

Reader-specific `*_layout.h` tables name recovered offsets and their source
functions. `LayoutCheck::Verify` compares complete-function FNV hashes, masking
relative call/jump operands because the loader can redirect them. A detour is
allowed only for an entry explicitly marked as a function the reader calls;
code whose instructions justify a field offset must still match. Failure
disables the affected reader rather than guessing a replacement offset.

For offline RE, use workspace `re/D2R_image.exe` or `D2R.image.bin`: file offsets
equal live RVAs. `tools/image_xref.py` in the **workspace parent of the checkout**
uses profiling/assertion strings, pointer references and `.pdata` ownership to
find functions without a new Ghidra analysis. `re/decompiled/` came from the older
text-only image; it is useful for control flow but lacks correctly placed data
and names. See [decompile provenance](decompile.md) and workspace `re/README.md`.
The local CASC extractor is also at workspace `tools/casc_extract.exe`; extracted
tables/layouts live in workspace `game/` and ignored checkout `game-data/`.

## Text, focus and graphics

`NativeUi::Snapshot` classifies a bounded visible tree using native class ancestry.
It reads live strings and values into `UiText::Node`; `UiText::Describe` and
`UiText::Reader` implement announcement/settling policy over those copies. Native
widget pointers are transient. `NativeUi::CurrentFocus` accepts the keyboard's
plugin-owned cursor where the game supplies no keyboard focus, while `GameFocus`
still reports the stock focus. Front-end `FocusTracker` additionally uses the
generated layout catalogue and focus-indicator geometry.

Keyboard menu entry follows the verified native input-mode getter; idle XInput
presence does not defer placement in keyboard mode. Standalone player inventory
enters its item grid. H consumes `TerrainService::ExplorationRooms`: all levels
count physical visits/current room. The beacon still consumes `ExploredRooms`.
`ExploreGoal::Next` selects uncovered walking floor, then streaming frontiers
and door approaches, using observation coverage independent of room entry.
One H request chains floor legs through `ExploreArrived` until exhausted;
`ExploreGoal::Reached` finishes a floor leg only after actual observation.
Arrows, either fighting key, End, menus and lifecycle changes cancel the run.
Closed doors/pads retain their explicit D stops. See keyboard-play/exploration docs.

`native_tooltips.cpp` observes the verified draw path, copying rendered primary
and comparison descriptions. `interaction_reader.cpp` combines those with grid
occupancy, equipment location, tabs and the game's action help. A focused occupied
cell waiting for a tooltip is different from an empty cell. See
[native tooltips](native-tooltips.md) and [keyboard menus](keyboard-play.md).

Game text resolves through `Localization::Text`; mod vocabulary uses
`speech_strings.*` and the 13 `strings/<locale>.h` tables. Every table has a
compile-time completeness check. Generated catalogues under `src/generated/`
come from extracted game data; update their generators when a mapping changes.
Quest button identity specifically comes from **classic MENU filenames**, whose
numbering differs from HD display-slot filenames.

Not all visible information is text. The Tal Rasha quest marker is an
`ImageWidget` frame. Its guarded adapter appends a plain symbol name to the
copied objective in `NativeUi::Snapshot`. The generic quest reader then retains
its existing title match, 150 ms settle and repeat behavior. See
[Tal Rasha symbols](tal-rasha-symbols.md) and [quests](quests.md).

## World and coordinate model

`NativeWorld` resolves the local player through the same D2RCore lookup as other
readers, then copies collision rooms into `Radar::World`. A unit's type determines
how its `Path` is interpreted: dynamic player/monster paths contain packed 16.16
positions; static object/item/warp paths use their own coordinate fields.
Collision coordinates are subtiles, **not** the projected position fields.
Logical room rectangles are in tiles; one tile is five subtiles. Use
`NativeRooms::Subtiles` when crossing that boundary.

World X projects down/right and Y down/left. Screen direction, collision axes,
UI canvas coordinates and native projection are separate frames; use existing
projection/direction helpers. Distances are spoken in steps, not a claimed
physical metre conversion. Detailed source offsets: [sonar](sonar-radar.md),
[collision semantics](collision-semantics.md), [keyboard actions](keyboard-actions.md).

The act's DRLG retains logical `Level`/`RoomEx` metadata independently of active
`Room` collision. Unloading a room frees its collision. A logical room's existence
does not prove detailed walkability or player discovery. `NativeLevelExits` reads
existing warp nodes and presets; `NativeLevelAreas`/`LevelAreas` find adjoining
outdoor area borders, which have no ordinary warp unit. Distant preset reads are
explicit opt-ins, not an ambient full-map reveal.

`NativeEntities` names loaded NPCs, enemies, objects, players, corpses and exits
from unit state and active game tables. `NativeGroundItems` supplies item copies.
`EntityRadar::Entity` is the common value type used by scans, the catalogue,
beacon, keyboard walking and cues. `kind` describes the native/logical entity;
`category` describes its navigation role. `placeId` identifies a fixed position
across unit recreation; `id` and `unit` identify a currently loaded game target.
`destinationLevel` is an exit's destination, not an object-class identifier.
`worldName` retains a HUD label before composed wording; `nameKey` allows stored
places to resolve their base label in the current language.

## Navigation, discovery and persistence

| Layer | Responsibility |
| --- | --- |
| `radar_grid`, `radar_snapshot`, radar policy modules | Local collision/body, structural scans and audible openings/blocks |
| `Navigation::Grid` | Reachable cells and step counts in the current collision window |
| `NativeRooms`, `RoomGraph` | Logical room identity, connectivity and room exits |
| `MapSurvey` / `MapSurveyService` | Discovery/catalogue policy, category selection and spoken targets |
| `TerrainService`, `ExploredRooms`, `NavigationPolicy` | Ground visited/charted, exploration mode and fixed places |
| `LevelRoute` | Remembered whole-level ground, walking plans and reachability |
| `PadLinks` | Observed same-class pad pairs in one room, joining route legs |
| `AutoWalk`, `KeyboardService` | Execute a selected route through native movement, arrivals and interaction |

The local navigation grid is bounded by `Navigation::MaxRadius` (150 subtiles).
`LevelRoute::Memory` learns streamed ground inside logical room bounds; unseen,
open, blocked and door cells remain distinct. `Reach` can pass a door when asking
whether a destination can eventually be reached; walking plans still treat a
closed door as blocking, while the beacon's guidance plans past one and leads
up to it (`KeyboardService::RememberedMap` shares the map; see
[walkable guidance](beacon.md#walkable-guidance)). Pad links participate in reachability and planning;
the player stops at a pad and uses D to take it. Ctrl+U's optional scanner filter
and the mandatory checks before walking use the same reachability policy.
See [keyboard routing and pads](keyboard-play.md).

`TerrainService::KnownPlaces` restores fixed landmarks; `RememberPlaces` records
ones the scanner found. The disk archive belongs to `ExploredStore`, under
`<pluginDirectory>/d2r-access/explored`, and keys each map by seed, level and room
layout digest. This differs from session-only caches: a new game must invalidate
live units/commands, but a matching saved map can retain discovered landmarks.
Layout validation prevents another generated map from inheriting them. Never
infer detailed collision from a remembered landmark. See
[exploration](exploration.md) and [multiplayer](multiplayer.md).

Keyboard movement/fighting/interaction use `native_actions.*`, the game's own
player commands and projection functions. Menu actions use their native widget
paths and bindings. Physical controllers retain their existing reader/gesture
paths; `pad_filter` is narrow input filtering. There is no synthetic keyboard
XInput pad. Ctrl+R in gameplay walks to the remembered way into the level;
in a merchant menu it means repair all. Context decides the command.

## Working on a feature

Trace the producer first: native draw/packet/message or copied world state;
then its service, pure policy, speech/audio sink and lifecycle reset. For new
information, establish whether it is text, an image frame, a game-table property
or a transient interaction result. Preserve the entity's identities and original
label when adding wording, including its disk restore path.

Tests in `tests/` generally exercise pure policy or production native readers
against synthetic memory layouts. They prove those paths, not live game speech
or a new offset on another build. `CMakeLists.txt` registers the suites. The local
`build.ps1` runs four compilers at BelowNormal priority and deploys in POST_BUILD;
check that the game is closed before building. Machine-specific scripts remain
local. Never commit or push simply because a task has finished.

## Additions integrated from upstream — 2026-10-03

Named preset places/corridors (`place_service`, `places`, `passages`) supplement
the room reader. `death_place_service` records death locations and known exits
toward them, with disk snapshots written by a dedicated worker. Town NPC spawn
presets supplement live units while preserving the local level-wide read and
map memory. Boss life (`boss_life_service`) and optional growls/reach cues add
combat information. Inventory hand/socket and arrival readers add feedback to
the existing interaction path. Skill-tree table names can speak before the
matching tooltip renders. [Merge review](upstream-2026-10-03.md) records scope,
deferred replacements and validation.

## Area-crossing and room-entry routing - 2026-10-04

`Entity::Border` distinguishes native area bearings from collision-measured
crossings and ordinary warp/unit exits. `ZoneTransitions::FoundAtCrossings`
discovers the measured copy; registry/scanner/keyboard/beacon refresh preserve
it over later bearings. `ExploredStore` version 4 persists the measured flag,
reads versions 1-3 and allows measured border coordinate corrections only.
`LevelRoute::Plan::Into` uses a cached multi-source field for any standable cell
inside an exploration room; point routes retain their existing cache and rules.
`Memory::Learn` leaves cells outside the native logical-room union untouched.
See [routing evidence](exploration.md#act-3-5-routing-corrections---2026-10-04).

The 2026-10-04 dock/H follow-up adds `ZoneTransitions::ApproachBounds` for
unmeasured outdoor bearings. `GoalBounds` shares that region between the whole-
level reach gate and remembered plan. Only collision-measured exits can be taken;
SetOut refreshes an exit before checking it. The pure H tests include the user's
captured Spider Forest grids/layout/visits. [Follow-up evidence](exploration.md#dock-departure-and-h-follow-up---2026-10-04).

## Moving town characters and resident floor - 2026-10-05

`NativeLevelExits::ReadNpcMarkers` maps Kurast's HratliStart/HratliEnd and
NatalyaStart object presets to their quest NPC classes. `NativeEntities` merges
these approach locations with monster presets and actual live town characters,
using stable class identity across movement/recreation. Same-level path-room
transfers remain readable. `TownNpcSightings` keeps actual positions over marker
fallbacks, retains alternate approaches on unloading, and exposes no stale live
unit type. Keyboard walking searches native approaches/new floor and switches
to the first current unit; beacon refresh ignores fallback coordinates. Existing
discovery/session/guard policies remain. [NPC evidence](entity-scan.md#moving-town-characters---2026-10-05).

Whole-level entity and local collision readers supplement the connected active
neighbor traversal with existing resident rooms of the current logical level.
`LevelRoute::Memory::Extend` follows layout growth, keeps floor after unloading,
and skips absent Blank collision. Native body/wall/ownership bounds remain.
`ExploreGoal::Coverage` is a session floor bitmap independent of room visits and
automap visibility. `Next` routes to uncovered standable floor in any room,
then streaming frontiers/door approaches; temporary failed points cannot remove
whole rooms or suppress new frontiers. Keyboard H uses point goals, current
movement collision and automatic reselection, with doors/pads stopping for D.
All five acts use this path. T's mapped percentage is distinct from coverage.
[Exploration evidence](exploration.md#resident-floor-and-exploration-reach---2026-10-05).
