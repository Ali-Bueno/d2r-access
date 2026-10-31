# STATUS — Diablo II: Resurrected (d2r-access)

> Per-mod status ledger. Open first when resuming. Dashboard, not docs.

## Updates keep the player's config — 2026-10-31 (not released)

A tester: every update reset D2RLoader's extra shared stash pages. The zip carried
`d2rloader.toml` and `d2r-access.toml`; both are now left out, and the loader writes each when
missing ([test releases](docs/test-releases.md#package-contents)). `test-release.ps1` refuses any
staged `*.toml`; the tester README no longer says to redo the language after an update.

**Next step:** release with the next build; live: a fresh install without `d2rloader.toml`
(the loader should write its default on first start).

## The tester's notes on build test-2026-10-30-2 — 2026-10-30 (released as test-2026-10-31 from 6fbbd72)

Release: 129/129 CTest, 29 entries, 63,020,648 bytes, SHA-256 `1d0e291b…f430c847`; news
`../tester-news-draft-2026-10-31.txt`.
From `D2R Access test build 2026-10-30-2 notes.txt` and a new `descriptions.txt`
([decisions](docs/decisions.md)). 129/129 CTest; installed by the build. **Unverified live.**
- Difficulty window said as the game's "Select Difficulty" (`strselectdiff`, `PanelTitle` in
  `ui_message_monitor.cpp`).
- Descriptions: the cows and placeholders for Pandemonium x3, Uber Tristram, Colossal Summit; every
  level answered ([descriptions](docs/descriptions.md#act-v-2026-10-30)).
- BACK+Y in hand-made maps: already answered by test-2026-10-30-3; Chaos Sanctuary (outside per
  `Levels.txt`) still says "Not in a dungeon room".

**Next step:** live: the character select's difficulty window; BACK+Y in the Throne (`[room] chamber:`).

## The tester's notes on build test-2026-10-30 — 2026-10-30 (released as test-2026-10-30-2 from 45af590)

Release: 129/129 CTest, 29 entries, 63,021,084 bytes, SHA-256 `feb0ee47…ef944a58a2`; news
`../tester-news-draft-2026-10-30b.txt` (sent).

From `D2R access-2026-10-30.7z` ([decisions](docs/decisions.md)). 129/129 CTest; installed by the
build. **Unverified live.**
- BACK+RT: party members first, switch `party-first` (on) (`VitalsService::PartyMembers`).
- Act V wells loop: OperateFn 22 is a well (`WellOperateFn`) ([ground items](docs/ground-items.md)).
- BACK+Y on an inside preset level (Throne of Destruction): released as a sentence (`roomPreset`),
  then the chamber's passages (`Rooms::Chamber`) ([rooms](docs/rooms.md)), released as
  test-2026-10-30-3 from 0db0fa6: 129/129 CTest, 29 entries, 63,020,677 bytes, SHA-256
  `a83037ba…7dec987`; news `../tester-news-draft-2026-10-30c.txt`.
- End screen read whole (`SelfReadScreens`, `[modal] EndScreenPanelExpansion ->`); character rows
  with their title (`Titled` in `focus_tracker.cpp`).
- Descriptions: 5 areas missing (cows, Pandemonium x3, Colossal Summit) ([descriptions](docs/descriptions.md#act-v-2026-10-30)).

**Next step:** live: BACK+Y in the Throne of Destruction and Mephisto's level (`[room] chamber:`),
a Worldstone Keep well, the end screen (Normal beaten on a new character, or Nightmare), the
character select over a Slayer, BACK+RT in a party with summons.

## The tester's notes on build test-2026-10-28 — 2026-10-30 (released as test-2026-10-30 from e81a47a)

Release: 129/129 CTest, 29 entries, 63,016,422 bytes, SHA-256 `25ad6d3b…52ecff6e6`.
From `D2R Access 2026-10-28 notes.txt` and a new `descriptions.txt` ([decisions](docs/decisions.md)).
129/129 CTest; installed by the build. **Unverified live.** News draft
`../tester-news-draft-2026-10-30.txt`. Confirmed by the tester: the Fortress's inside/outside, the
Harrogath town portal, Nihlathak from anywhere in town, "Barbarian", Terror Zones in the waypoint menu.
- Eclipse never cut off: `SpeechPriority::SayWhole` holds it for `SpokenMs` of its words
  ([speech priority](docs/speech-priority.md)).
- Act V words in `data/descriptions.txt`, 23 areas ([descriptions](docs/descriptions.md#act-v-2026-10-30)).
- Anya not found from the town portal by Raine: struck by the tester until a new log
  (`[survey] town NPCs`, `[quest] flags row=37` would tell whether Malah had rewarded it).

**Next step:** live: the eclipse in a fight (`[quest] eclipse said`).

## The multiplayer form as a list, QWERTY stick typing — 2026-10-28 (released as test-2026-10-28-2 from 19ebfef)

Release: 129/129 CTest, 29 entries, 63,015,807 bytes, SHA-256 `9c20221a…53de9b2d368ef`; news
`../tester-news-draft-2026-10-28b.txt`. The user: the lobby's options were a nightmare to move
through, down sometimes going down and to the right. A D-pad diagonal, a side press with nothing
there, and the left stick reached the stock travelling cursor. Now up/down (and the left stick) step
one control in the form's order, left/right stay within a row, and the stock cursor never travels
([multiplayer](docs/multiplayer.md#the-form-as-a-list-2026-10-28)). Stick typing walks a QWERTY
keyboard, switch `pad-typing-qwerty`, on
([multiplayer](docs/multiplayer.md#the-qwerty-keyboard-on-the-right-stick-2026-10-28)).
129/129 CTest; installed by the build. **Unverified live.**

**Next step:** live: the Host Game form with the D-pad and the left stick (the log's
`[menu-navigation] boxes: ... list:` gives the order heard); a game name typed on the QWERTY keyboard.

## The tester's notes on build test-2026-10-26-3 and the "no route" logs — 2026-10-28 (released as test-2026-10-28 from a013697)

Release: 128/128 CTest, 29 entries, 63,013,800 bytes, SHA-256 `e83e11d6…2570f8d6f8`; news
`../tester-news-draft-2026-10-28.txt`.

From `no route_d2r-access.log` and `D2R Access 2026-10-26-3 notes, descriptions and Act V fix
requests.7z` ([decisions](docs/decisions.md)). Tests pass; installed by the build. **Unverified live.**
Confirmed by the tester: the hit-chance comma (NVDA, SAPI 5 Zira), the Cott4 creak.
- Unexplored areas with no way in (Outer Cloister's roofed blocks) left out: `LeaveOutShutRooms`
  ([entity scan](docs/entity-scan.md)).
- Fortress interior: HD layout offset 22,22 in `tools/gen_places.py`; Hellforge (`SoulStoneForge`,
  words `hellforgeDescription`) and pentagram (`DiabloStart`, listed) landmarks
  ([places](docs/places.md#the-fortresss-hall-the-hellforge-the-pentagram-2026-10-27)).
- Portals never moved to a border (`Crosses`); portal-art objects are portals (Tyrael's)
  ([portals](docs/portals.md)).
- Durance door: opened warp looked for in neighbouring rooms ([exits](docs/level-exits.md)).
- Catapults ranged, combatants "Barbarian" ([cues](docs/entity-cues.md)); Act IV and Harrogath words.
- Nihlathak from anywhere in Harrogath: InitFn 68 rule from the RE of `0x548EE0`, levels entered
  this game (`GameLevels`) ([entity scan](docs/entity-scan.md#npcs-at-marker-objects-2026-10-25)).
Known issues (from the log, not reported): a room target can say "Arrived" one cell short of the
room (`route_follow.cpp` `EndUnits`), and keeps its "unexplored area" name once explored.

**Next step:** live: Outer Cloister unexplored areas (`[survey] ... no way in`), Fortress waypoint
to stash (`[places]`), a Harrogath town portal, Nihlathak from Malah's (`[survey] town NPCs`,
`Nihlathak@...`), the Durance door (`[exit] ... opened: to=100`).

## The tester's notes on build test-2026-10-25-2 — 2026-10-26 (released as test-2026-10-26-3 from a954dfe)

Release: 128/128 CTest, 29 entries, 63,007,156 bytes, SHA-256 `a181d76c…5dc50e69f41b69266672fd840c9ffd26`; news
`../tester-news-draft-2026-10-26c.txt`.

From `D2R access test build 2026-10-25-2 notes.txt` and a new `descriptions.txt`, no logs
([decisions](docs/decisions.md)). 128/128 CTest; installed by the build. **Unverified live.** Confirmed by the tester: Jerhyn and Natalya from anywhere in town, far unexplored areas, the
speech priority menu, routes in Act III's jungles.
- Area hint: switch `description-hint`, off; `descriptions` now the landmark words only; BACK + right
  stick up unchanged ([descriptions](docs/descriptions.md#the-area-hint-off-by-default-act-iii-and-the-fortress-2026-10-26)).
- Words for Act III and the Pandemonium Fortress in `data/descriptions.txt`, Kurast sewers `[92]`/`[93]`.
- Hit chance: a comma after the row's number (`WithChance`) ([character panel](docs/character-panel.md)).
- Kurast temple stairs: an opened OpenWarp object is named after its room's walk-through warp
  (`WalkThroughDestination`, `[exit] ... opened: to=L`) ([exits](docs/level-exits.md#unfolded-stairs-are-named-after-their-temple-2026-10-26)).

**Next step:** live: unfold a Kurast Bazaar temple's stairs
(`[exit] ... opened: to=94` or `95`; `to=0` means the warp is in another room), the character sheet's
Attack Rating with the right stick, a described area's banner with no hint.

## The tester's roundup of older issues on build test-2026-10-25-2 — 2026-10-26 (released as test-2026-10-26 from 847c660, then test-2026-10-26-2 from 9af6aaf)

Release: 128/128 CTest, 29 entries, 63,002,084 bytes, SHA-256 `486e44ea…f0ac6b073e`; news
`../tester-news-draft-2026-10-26.txt`. test-2026-10-26-2: 128/128 CTest, 29 entries, 63,002,005 bytes,
SHA-256 `71027ae5…a8d64322`; news `../tester-news-draft-2026-10-26b.txt` (the one to send).

From `D2R Access, older issues revisited.7z` ([decisions](docs/decisions.md)). 128/128 CTest;
installed by the build. **Unverified live.** Closed by the tester: the walked-distance click, the
low-life alert, the Forgotten Tower's exits, the trade gold window, the tower ruin, stables, the Lut
Gholein sewer grate.
- Buy confirmation: the shop's name no longer repeats; a dialogue panel over an open shop is a
  pop-up (`PopupOverInventory` in `interaction_reader.cpp`).
- Death screen: no "Life 0 percent" over it (`Vitals::Alerts`) ([vitals](docs/vitals.md)).
- Town NPCs at marker objects (Jerhyn, Hratli, Natalya, Anya): `ReadMonsters` objects,
  `generated/npc_markers.h`, `NpcSpawnRules` from the game's InitFns (RE'd); loaded town NPCs count
  as seen ([entity scan](docs/entity-scan.md#npcs-at-marker-objects-2026-10-25)).
- Item panning: 100 = the old 50 (`item-pan-half-25`) ([ground items](docs/ground-items.md)).
- `unexplored-everywhere` (Ctrl+S): every unexplored area listed ([entity scan](docs/entity-scan.md));
  released off, then on by default at the user's call as test-2026-10-26-2 (news
  `../tester-news-draft-2026-10-26b.txt`, which supersedes the first).

**Next step:** live: buy potions at Ormus, a death, `[survey] town NPCs` at Atma's and the Kurast
stash (Jerhyn/Natalya `/spawn`, `201x1`, `297x1`), a route to a far unexplored area.

## The tester's list on build test-2026-10-25 — 2026-10-25 (released as test-2026-10-25-2 from 1cf739e)

Release: 127/127 CTest, 29 entries, 63,000,828 bytes, SHA-256 `43d48f45…cc90b44fe`; news
`../tester-news-draft-2026-10-25b.txt`.

From `d2r access test build 2026-10-25 notes.7z` ([decisions](docs/decisions.md)). 127/127 CTest; installed by the build. **Unverified live.**
Closed by the tester: the black square (the game's "chat background" option), sewer descriptions,
the palace entrance, "Open sockets".
- Stairs: the pad takes a warp within 4 subtiles, any side (`controllersettings.json` RoomTile);
  routes to one end within reach and "Arrived" waits for it (`TakenWithinReach`, `Area::reach`,
  `RouteFollow` goal); exit labels no longer hidden by a monster sharing a word (`TargetSight::Find`)
  ([exits](docs/level-exits.md#a-stair-is-taken-within-4-subtiles-of-it-routes-end-there-2026-10-25b)).
- Hit chances on Attack Rating and Defense from the game's own builders (`NativeHitChance`, 8
  fingerprints); the sheet's hover text spoken too ([character panel](docs/character-panel.md#hit-chance-tooltips-2026-10-25)).
- Percent explored: 100 with no unexplored room listed; drawable counts kept, archive v4
  ([overview](docs/terrain-overview.md)).
- Kurast sewer stairs: "new exit" once a game when the lever opens them (`[terrain] exit revealed`).
- Trade grids say whose offer alone ([trade](docs/trade.md#whose-offer-said-alone-2026-10-25b)).
- Not in the archive: the "sewers_2" log.

**Next step:** live: route to the Flayer Dungeon stairs from each side and press A at "Arrived";
the character sheet's Attack Rating rows (`[hit-chance] Verified; hand 1 is left`); Mod+B in Black
Marsh after a reload; the sewer lever; a trade with Raine.

## The tester's list on build test-2026-10-24 — 2026-10-25 (released as test-2026-10-25 from f876659)

Release: 127/127 CTest, 29 entries, 62,999,158 bytes, SHA-256 `a974bc93…aad4e75f1bf`; news
`../tester-news-draft-2026-10-25.txt`.

From `d2r-access test build 2026-10-24 notes.7z` and its addendum ([decisions](docs/decisions.md)).
127/127 CTest; installed by the build. **Unverified live.** Closed by the tester: the black square
(the loader's; it shows with the plugin removed).
- Percent explored: drawn cells over the cells the game's AutoMap table can draw
  (`NativeAutomap::DrawableTiles`, 8 fingerprints) ([overview](docs/terrain-overview.md#the-share-counts-the-tiles-the-automap-can-draw-2026-10-25),
  [RE](docs/automap-reader.md#drawable-tiles-2026-10-25)).
- Tal Rasha's Chamber, and every object named after a level, is an exit (`ReadObject`) ([exits](docs/level-exits.md)).
- Duriel's larvae: level critters left out of the scan (`Generated::CritterMonster`) ([cues](docs/entity-cues.md)).
- Palace entrance listed from anywhere in town; dead ends only when walled across (`Passages::GoesOn`) ([places](docs/places.md)).
- Asheara's pillars: long-leg folds never cross a wall cell (`RouteLegs::Lengthen`) ([route](docs/route-description.md)).
- Cott4's door: a face may stop at an inner corner (`Rooms::Doorways`) ([doorways](docs/ground-items.md#doorways-2026-10-20)).
- Descriptions: the tester's corrected file plus Act III; `Name [Id]` lines (sewers 47-49, 92); the
  area hint, silent until now, fixed ([descriptions](docs/descriptions.md)). "Open sockets".
- Diagnostics: `[exit]` (each stair's LvlWarp row), `[stats] hover text` ([character panel](docs/character-panel.md)).
- Not in the archive: the "Kurast_" log (Lower Kurast to the Bazaar).

**Next step:** live: Mod+B in a fully seen Tower Cellar level, Tal Rasha's Chamber in the exits,
Duriel's lair quiet after the kill, a route to the palace from the square and in Asheara's hut,
Cott4's creak; the tester: a log at the Flayer stairs with A tried from each side.

## The tester's list on build test-2026-10-23-2 — 2026-10-24 (released as test-2026-10-24 from 45fd79d)

Release: 127/127 CTest, 29 entries, 62,980,665 bytes, SHA-256 `e2e0b0c4…905814a2f88`; news
`../tester-news-draft-2026-10-24.txt`.

From `D2R Access test build 2026-10-23-2 notes and bug list.7z` ([decisions](docs/decisions.md)).
127/127 CTest; installed by the build. **Unverified live.** Closed by the tester: NPC "!" chimes,
the vitals readout, the Cube, the stone tombs, status effects; the Sanctuary's stairs much better.
- Death screen: "You Died Modal" no longer said on open or close; "You have died…" interrupts
  (`SelfReadPanels` in `ui_message_monitor.cpp`).
- The tester's Act I and II descriptions come with the mod: `data/descriptions.txt`, embedded,
  under the player's file ([descriptions](docs/descriptions.md#words-that-come-with-the-mod-2026-10-24)).
- "Black square": the HUD notice list with 13 empty rows; one empty entry per loading screen,
  not written by the mod. Diagnostic `[notice] row without text` added
  ([native UI](docs/native-ui.md#hud-notice-rows-without-text-2026-10-24)).
- Bridges: "bridge" said 4 times in the log (Kurast Docks, Spider Forest); no comment from the tester.

**Next step:** the tester: the square with the plugin removed, and a log with it on screen;
live: a death, BACK + right stick up in an Act II area.

## The tester's list on build test-2026-10-23 — 2026-10-23 (released as test-2026-10-23-2 from efa1230)

Release: 127/127 CTest, 29 entries, 62,976,543 bytes, SHA-256 `c82af1f5…b482a8a4e`; news
`../tester-news-draft-2026-10-23b.txt`.

From `D2R Access test build 2026-10-23 notes.7z` ([decisions](docs/decisions.md)). 127/127 CTest;
installed by the build. **Unverified live.** Heard working by the tester: distances in steps, the
stash pages and gold, the Countess's cistern in both layouts.
- Stuck on the Arcane Sanctuary's stairs (multiplayer): the wall-slide hook refused the game's
  45-degree retries; let through in narrow passages (`WallBump::Squeezes`, switch `narrow-slide`)
  ([wall sliding](docs/wall-sliding.md#stairs-and-narrow-passages-2026-10-23)).
- Jungle log and plank bridges said "bridge", footprint from their collision mesh
  (`MESH_FOOTPRINTS`); the Moldy Tome's piece no longer "tower ruin"; Lut Gholein's palace entrance
  listed and routable (`Places::Tracked(PlaceInterior)`) ([places](docs/places.md)).
- Cott3's door two cells from its corner: flank may be a corner (`CornerRun` in `Rooms::Doorways`)
  ([ground items](docs/ground-items.md#doorways-2026-10-20)).
- Typing reel: digits 1-0 down from "a" (`PadTyping::Charset`); "Drop Gold closed" silent after a
  confirmed transfer; tutorial: tomes from Akara (1x2), hold X for five points ([tutorial](docs/tutorial.md)).

**Next step:** live: the Sanctuary's stairs with the stick off-line (`narrow_slides=`), a jungle log
bridge, a route to the palace entrance, Cott3's creak, a number password with the reel.

## The tester's list on build test-2026-10-22-2 — 2026-10-23 (released as test-2026-10-23 from f755839)

Release: 127/127 CTest, 29 entries, 62,972,811 bytes, SHA-256 `58dab81d…19afa0542`; news
`../tester-news-draft-2026-10-23.txt`.

From `D2R Access test build 2026-10-22-2 notes.7z` ([decisions](docs/decisions.md)). 127/127 CTest;
installed by the build. **Unverified live.** Heard working by the tester: the aura fix, the Dry
Hills stairs, secret drafts, cottage creaks, the crosshair, the delete count, waypoints, skill
binding, the main menu.
- Countess's cistern: was on her chest's dais; now the DS1's `BloodFountain` object, 3x3
  (`gen_places.py` reads DS1 objects via `objpreset.txt`) ([places](docs/places.md)).
- Tristram's blood fountain tracked (`Places::Tracked`); its houses still do not creak (known).
- Delete modal: "Character deleted" after `CharacterSelect`/`DeleteCharacter` ([modal](docs/delete-character-modal.md)).
- `route-long-legs` (on): legs under 5 units folded into a neighbour (`RouteLegs::Lengthen`);
  `distance-steps` (off): distances in steps of the click interval (`EntityRadar::SetUnitsPerStep`)
  ([route](docs/route-description.md)).
- Tutorial: the crosshair's sound in step 9, its use in step 11 ([tutorial](docs/tutorial.md)).
- Not in the archive: the "campfire_" logs the list mentions.

**Next step:** live: Tower Cellar 5 in both layouts (walk-in at the basin, route ends beside it),
a route to Tristram's fountain, a delete, routes with `route-long-legs` near doors (stuck or not),
`distance-steps` on.

## Crosshair beep and melee contact ping — 2026-10-22 (released as test-2026-10-22-2 from 28523c9)

Release: 127/127 CTest, 29 entries, 62,967,852 bytes, SHA-256 `cbe8dca3…6f170a68d`; news
`../tester-news-draft-2026-10-22.txt`.

The tester: the reach ping fired mid-screen for a Warlock and a Sorceress alike and ignored how
many are near; the pad's auto-target takes the nearest, never the shaman behind. Raine: a beep when
something is in the crosshairs. RE of the game's rules ([controller targeting](docs/controller-targeting.md));
tables in `game-data/hd/global/excel/`. 127/127 CTest; installed by the build.
- Crosshair beep: the game's own target (hover entry `0x2A41630`) landing on an enemy in sight,
  switch `crosshair` ([crosshair](docs/crosshair.md)). A monster that was the target within 3 s
  does not beep again (`Settings::repeatMs`).
- Live 2026-10-22 (user, barbarian, pad, 2 h): the target read in every game, the beep on the
  plate's monster, contact pings all by the rule. Without the repeat rule: 2077 beeps, 26 in
  10 s at peak (target trading); replayed with it, 1166. The rule itself is not heard yet.
- Reach ping: melee contact by the game's rule for every attack, Warlock +2; monsters assumed
  size 2 ([attack reach](docs/attack-reach.md#the-reach-ping)).
- Not done: the contact count (tester), the quick enemy list on RS down, the selected-enemy beep.
- First install crashed (13:04): the DLL's `speech_strings.cpp.obj` was from 08:12 and Ninja had
  no header deps for it, so the new `Strings::Table` fields shifted every string read elsewhere.
  All DLL objects rebuilt clean; `ninja -t deps` now VALID. After adding a `Strings::Table` field,
  check that object's time.

**Next step:** the tester's word on the beep's rate with the repeat rule, with a Sorceress and
a Warlock; then the quick enemy list.

## The tester's list on build test-2026-10-22 — 2026-10-22 (released in test-2026-10-22-2)

From `d2r access test build 2026-10-22 notes.7z` ([decisions](docs/decisions.md)). 126/126 CTest;
installed by the build. **Unverified live.**
- "Paladin Aura" at loading: the focus fallback read the HUD's aura widget; HUD panels are skipped
  (`PanelState::Hud`).
- Delete-character hold: 5 to 1 said while A is held (`ModalReader::HoldCountdown`); the Confirm
  button's text logged (`[modal] delete:`) ([modal](docs/delete-character-modal.md)).
- Cliff stairs: one "stairs" entry halfway, arrival at either end (`Places::Listings`); the
  announcer says "stairs" from two tiles. Countess's cistern: `BloodCistern` from
  `ritual_platform01` (INFERRED to be the vessel), listed and routable ([places](docs/places.md)).
- Doorways: scanner entries (`Doorways::Listed`, switch `doorways-listed`), and in Tristram and
  Act I's building pieces (`Doorways::Select`, `PlaceService::Buildings`) ([ground items](docs/ground-items.md)).
- Portals: named by owner only; `Recreated` takes the owner's new portal for the old one, in the
  registry and the beacon ([portals](docs/portals.md)).
- Jail secret doors (objects.txt OperateFn 18): never listed while shut; "You feel a cool draft."
  within 10 units (`SecretDoors::Draft`, switch `secret-drafts`) ([secret doors](docs/secret-doors.md)).
- Route to the Jail 3 stairs: a Jail door's cells carry OBJECT alone, so the kept map took it
  for a wall; shut door units are now noted into it (`Memory::NoteDoor`) ([route](docs/route-description.md)).
- Passage finder removed (Ctrl+V, BACK+A, `exit-finder`); `ExitReview` cut to `RoomSides`; the
  combination it used is free ([rooms](docs/rooms.md)).
- Known: doorway detection now also runs in the survey's pass (cost unmeasured); the cottages'
  and Tristram's doorways and the secret door's collision are unchecked.

**Next step:** live: a paladin loading in, a delete hold, the Dry
Hills stairs route, a cottage and Tristram (`[items] doorways nearby`), Jail 2's secret room
(`[items] secret door felt`), a two-player portal swap, a route through a shut Jail door
(`[route] shut doors noted`).

## The tester's addendum on builds 2026-10-20 and 20-2 — 2026-10-21 (released as test-2026-10-22 from 3f13f86)

Release: 125/125 CTest, 29 entries, 62,969,716 bytes, SHA-256 `c72439ae…1cf1808`; news
`../tester-news-draft-2026-10-21d.txt`. **Unverified live.**

From `tutorial and builds -20 and -20-2 feedback.7z` ([decisions](docs/decisions.md)):
- Doorways from the walls' gaps (`Rooms::Doorways`), not the streamed rooms' seams; gaps at door
  objects left to the doors ([ground items](docs/ground-items.md#doorways-2026-10-20)).
- Mod+B: drawn walkable tiles over walkable tiles (`AutomapCells::DrawnWalkable`, per-room walkable
  cache in `terrain_service.cpp`) ([terrain overview](docs/terrain-overview.md)).
- Map landmarks (Dry Hills stairs, id 0) routable: `BeaconService` `Identified`; an unroutable scan
  clears the offer.
- Jerhyn (no preset) remembered once seen (`TownNpcs::Merge`, `TownNpcSightings`); portals replaced
  by owner and dropped by the roster's portal ids, `[scan] portal dropped` ([portals](docs/portals.md)).
- The eclipse waits for a pause in speech (`SpeechPriority::Pause`, shared with the tutorial).
- "Unexplored area" in all 13 languages; `unexplored-spread` (Navigation, on) orders them one per
  direction (`MapSurvey::SpreadByDirection`).
- Known: room walkable counts are taken once, when first streamed; the doorway tile figures are
  INFERRED; whether a client's roster portal ids follow the host is unchecked.

**Next step:** live: Tower Cellar 1/3/4 halls (doorways where the screenshots show), Mod+B on entering
Catacombs 2 (`[terrain] ... drawn D of W`), a route to the Dry Hills stairs, Jerhyn from Atma's, and
a two-player portal swap (`[scan] portal dropped` on the client).

## The tester's tutorial list — 2026-10-21 (released as test-2026-10-21-3 from 0f0dd60)

Release: 125/125 CTest, 29 entries, 62,952,885 bytes, SHA-256 `17d6cbc8…f0ddae`; news
`../tester-news-draft-2026-10-21c.txt`.

From `Tutorial feedback list.txt` ([tutorial](docs/tutorial.md)):
- Fight ends on a kill (experience rising), not gold; the Den cleared passes the steps on the way
  (`Tutorial::PassedBy`); a message waits for a pause in speech 10 s at most (`PatienceMs`). The
  tester's "Den messages never came" was most likely the gold step (no log).
- Ctrl+Y moved into the Den (new step `DenRooms`); "go into the cave" names the Den; a waypoint step
  after the reward. Records carry `#version=2`; older ones are remapped (`FromVersion1`).
- Lessons' sounds: BACK + right stick left (K) plays the last again and holds, right (L) goes on;
  switch `tutorial-sound-wait`; read back, a sound's message plays it. Monsters' sounds before
  leaving the camp; gold, potion and weapon in the chosen item sounds (`GroundItems::Heard`).
- The town portal's button read from the bars (`SkillBarService::ButtonOf`); passives and the
  General Skills page (INFERRED: LT/RT) in the skill lesson; gold and shop lessons.
- Records moved to `tutorial_records.*`. 125/125 CTest; installed by the build. **Unverified live.**
- Known: `tutorial_service.cpp` is ~930 lines; the side lessons (level-up, inventory, gold, shop)
  belong in a file of their own. Not built: a gems/runes/stash-tabs lesson (controls unknown).

**Next step:** a new character through the whole tutorial with a pad: K/L on the sounds, the kill,
Ctrl+Y in the Den, the portal's button, the waypoint step; send the log (`[tutorial]`).

## The tester's list on build 2026-10-20 — 2026-10-21 (released as test-2026-10-21-2 from 083ce69)

Release: 125/125 CTest, 29 entries, 62,930,893 bytes, SHA-256 `f60bca7d…3bc708`; news
`../tester-news-draft-2026-10-21b.txt` (the tutorial and this list).

From `d2r access test build 2026-10-20 notes, feedback and logs.7z` ([decisions](docs/decisions.md)):
- Grave doorway (Tower Cellar 4) and Den props: the routes went round them; the follower said a
  turn at once after a first leg of 3 units or less, cutting the route off. A turn now waits for
  half its leg ([route](docs/route-description.md#following-it)).
- Unexplored rooms: the route's "Arrived" only at its end cell; now anywhere in the room (`InRoom`).
- Arrivals hold 1.5 s (speech kind Arrivals): the Cathedral door's name had cut them off.
- Walked distance: own volume past the mix headroom (above 125 % did nothing), a click as the
  default sound (`step-beep` keeps the beep), 5 units by default.
- Doorway creak cut before its clacks, +6 dB; `door-volume`, `doorway-volume` in Volumes
  ([ground items](docs/ground-items.md#doorways-2026-10-20)).
- Main menu news behind `main-menu-news` (on); waypoint act tabs said "Act 1" to "Act 5".
- The tutorial in every language: one header each in `src/tutorial_strings/`, the ten beyond
  Spanish and English machine-translated; `tutorial_tests` holds each to the English `{tokens}`.

125/125 CTest; installed by the build. **Unverified live.**
**Next step:** the tester walks a short-first-leg route (the grave), into unexplored rooms away
from the way's end, to the Cathedral doors, and Tower Cellar 5 with doorways on (`[route] follow`,
`[items] doorways nearby: N level=25`).

## New-character tutorial built — 2026-10-21 (released in test-2026-10-21-2)

[Tutorial](docs/tutorial.md): `TutorialService` (steps and history pure in `tutorial.*`, words in
`tutorial_text.*`, Spanish and English), two one-shot sounds made for it, speech kind `Tutorial`,
switch `tutorial`, BACK + right stick left/right and BACK + D-pad up (kept from the game by
`PadFilter`), keys K, L, Shift+K. 125/125 CTest; installed by the build.

First live test (2026-10-21, pad, steps 0 to 4): the steps advance and the sounds play. Changed
after it: Warriv first (scan him, talk to him; then Akara for the route, the beacon and the quest:
16 steps), the welcome waits for the arrival news and every message for a pause in speech (the
area's name had cut the welcome), the place names are the game's own (Spain's and Mexico's differ),
and the beacon keeps what the scanner named last until the next scan unless it is a monster, a
corpse or a player (Akara had expired after 15 s: `docs/beacon.md`).
Second test (2026-10-21): Warriv works. Then: until the radar step is done, NPC, stash, exit and
unexplored-room sounds keep quiet (`Tutorial::QuietWorld`); the delete-character modal names its
buttons, "A: SÍ, B: NO" (its legends hold no text; `docs/delete-character-modal.md`). Third: a
character made again under a deleted one's name resumed its tutorial; records are now keyed by name
and the save's creation time. Fourth: a lesson of the world's sounds after the radar's (step 2,
each sound after its name, then let in), 4 s after the wall; the radar's words say the tutorial
plays its sounds, and floor no longer means "nothing there". Fifth: a character deleted and made
again within 15 s kept the old save's creation time (file name tunnelling), so records of deleted
saves are dropped outside a game; the radar's new places and the NPC announcer wait while the
tutorial talks; the radar is 3 dB louder (`CueLevel` 0.34). Sixth: each radar sound's meaning
before it, shorter waits (45 ms a character), the radar 3 dB louder again (0.48), the unexplored
passage and floor in the radar lesson, and a level-up lesson (character sheet, points, lines) at the
first calm moment after level 2, which follows the player from the sheet to the skill tree
without shutting it (RB inferred); no spoiler of the Den before Akara;
the Den's step says the map is made anew each game and how to search it. Seventh: menu messages
were never said (`uiOwnsSpeech` is always true in a focused menu), fixed; Warriv's step without
spoiler; an inventory lesson when something to wear is picked up; all Act I quest rows logged, since
row 0, not 1, changed when Akara gave the Den. Eighth (2026-10-21): the user played a new character
through the whole tutorial, the Den cleared and rewarded: row 1 is the Den (201e cleared, 2001
rewarded), row 0's bit 0 is Akara's quest given (now a state for both talks); the skill tree lesson
says how to put a skill on a button once the point is spent.
**Next step:** the user plays a new level 1 character from the
welcome through Akara's quest and sends the log (`[tutorial]`, including `quest rows`). The beacon check of the section below still stands.

## Beacon ways end where the body fits — 2026-10-21 (released as test-2026-10-21 from d64065d)

Release: 124/124 CTest, 29 entries, 62,815,997 bytes, SHA-256 `d179e0f2…6209a9`; news
`../tester-news-draft-2026-10-21.txt`.

The cause of the user's three `lead=no-way way_cells=0` failures ([beacon](docs/beacon.md)):
- Stairs set in the wall: `Guidance` planned the remembered way to within 1 cell, where no body
  fits. The way may now end within the arrival distance (3) that the beacon's own test and
  `WayEnd` already used (`Recall::around`, `ArrivalMargin`); the loaded floor's check keeps 1 so
  nothing counts as reached through a wall, and at a corner of the way's end the loaded floor
  takes the last steps. Test: `StairsInAWall`.
- Lut Gholein's exit before the gate was seen: the goal was a box round the edge's middle, with no
  floor; the gate is 40 subtiles off. Now the whole shared edge (`LevelAreas::Crossing::first`/`last`).
  The service test fails against the old box.

124/124 CTest; installed by the build. **Unverified live.**
**Next step:** the user walks to stairs and to Lut Gholein's Rocky Waste exit and sends the log
(`[beacon] lead=`, `[beacon] refused`).

## Live play by the user — 2026-10-20 (build e280447): beacon bug found (section above)

Log kept outside the repo: `../logs/d2r-access-2026-10-20-user.log` (the game overwrites its own).
- **Beacon "no route through explored areas" still happens**, 3 times, `lead=no-way way_cells=0`:
  town exit to Rocky Waste and both Stony Tomb 1 stairs. No `[route] the map grew` in 45 minutes
  over 6 levels: the room list does not grow, so PR #4 is not the cause ([beacon](docs/beacon.md)).
- Identify: not a bug. The user tried it on items Cain had just identified; it works by holding A
  on the tome ([character panel](docs/character-panel.md)).
**Next step:** done 2026-10-21 (section above).

## PR #4 merged, its map growth wired to a fresh room read — 2026-10-20 (released as test-2026-10-20 from e280447)

Release: 124/124 CTest, 29 entries, 62,817,687 bytes, SHA-256 `076ea284…2a80a`.

PR #4 (mojsior): `Memory::Extend` and fuller `[beacon]` refusal/release logs for "no route through
explored areas". As merged, `Extend` got the per-level cached room graph and could not grow; it now
gets a fresh `NativeRooms::Read`. `Read` takes the expected level and fails on another level's rooms;
the survey's room cache retries a failed read. Whether the list grows mid-level is unproven
([beacon](docs/beacon.md)). 124/124 CTest. **Unverified live.**
**Next step:** superseded: the user's log refutes the growth (section above).

## The tester's list on builds 2026-10-19 and 19-3 — 2026-10-20 (released as test-2026-10-20 from e280447)

From `D2R Access build 2026-10-19 and 19-3 feedback and bug list.7z` (the user's Downloads: list,
screenshots, `creak_wook.mp3`; no logs). Choices in [decisions](docs/decisions.md).
- Doorway creak in crypt-type levels (`doorway-sounds`, on), opened doors' loops
  (`open-door-sounds`, off), a click every 1/5/10 units walked (`step-clicks`, off;
  `step-click-interval`, `step-volume`) ([ground items](docs/ground-items.md)).
- Route: whole length and legs optional (`route-total`, `route-legs`, on); each route logs the
  collision around the player ([route description](docs/route-description.md)). Grave doorway: awaiting a log.
- Cathedral exit kept at its door, not the colonnade arch (`ZoneTransitions::DoorAt`, [beacon](docs/beacon.md)).
- Discoveries: held behind the arrival banner, said with the scanner's name (tomb symbol)
  ([exploration](docs/exploration.md)).
- Tainted Sun eclipse line (`questEclipse`, row 11, Lost City) ([quests](docs/quests.md)).
- Sarcophagus is a raiser; Rogue fountain a map landmark; porch cottage without "cottage";
  waypoint window's acts hint; gold window's live title; select hint dropped after right stick up.
- Natalya: `[survey]` lists the town's NPC presets ([entity scan](docs/entity-scan.md)).

124/124 CTest; installed by the build. **Unverified live: all of it.**
**Next step:** the tester tries it; logs: `[route] from=` with `[radar-collision]` at the grave
doorway, `[survey] town NPCs on level 75`, `[modal] DropGoldModal title`, `[quest] Tainted Sun began`,
`[items] doorways nearby`.

## Route legs along the stick's eight ways — 2026-10-19 (released as test-2026-10-19-3 from ac561c3)

Release: 123/123 CTest, 29 entries, 62,665,603 bytes, SHA-256 `b89c2a63…d881`.

From the tester's "Fundamentals: Route finder (Mod+A)" (text, painted screenshots, `wf_` logs on
`test-2026-10-19`) and the earlier `BFB_mp`/`dani_mp`/`dani_sp` logs; choices in
[decisions](docs/decisions.md); news (with replies) `../tester-news-draft-2026-10-19c.txt`:
- Every leg is one of the eight ways, a doorway crossed along its axis (`RouteLegs::Search`:
  turns charged 3 units, diagonals need room on both sides; stretches of 96 cells); the route ends
  anywhere in the arrival area, and only a cell walked to from the place counts as there
  (`LevelRoute::Arrival`, the planner's goal too) ([route description](docs/route-description.md)).
- The next leg is said half a second before its turn at the player's pace (`RouteFollow::Follower`).
- The Crypt's burning star is told across its piece (`Places::Reach`, [places](docs/places.md)).
- Not built: the tester's optional click per unit walked (offered).

123/123 CTest; installed by the build. **Unverified live: all of it.**
**Next step:** the tester walks the four examples again (cottage door, Den of Evil, Barracks
room, Crypt doorway) with the stick's eight ways; logs: `[route] from=`, `[route] follow turn`.

## Route finder (BACK+A) into walls and after other players — 2026-10-19 (released as test-2026-10-19-2 from d1b047c)

Release: 123/123 CTest, 29 entries, 62,657,868 bytes, SHA-256 `8157c9a0…2b70`.

From the tester's chat (no log yet; choices in [decisions](docs/decisions.md)); news (with
replies) `../tester-news-draft-2026-10-19b.txt`:
- Every compass point is a way the stick and arrows walk (`SpokenDirection::Bucket`): doorways
  are NE/SE/SW/NW, and the scanner, BACK+RT and the route agree
  ([spoken directions](docs/radar-readings.md#spoken-directions-2026-10-13)).
- Legs straightened with the body's footprint; a doorway step is no longer folded away
  (`RouteLegs::Fold`); `Radar::InBody` shared by the three footprint tests.
- Route to what the scanner named re-reads it first; a followed target that walks on is
  re-planned (`FollowMovedTarget`).
- Item and place panning, `item-pan-25` in Ctrl+S Sounds ([ground items](docs/ground-items.md)).

123/123 CTest; installed by the build. **Unverified live: all of it.**
**Next step:** the tester tries a route through a doorway and to the other player, compares the
scanner's and BACK+RT's words with the way the stick walks, and item panning at 50 percent; logs:
`[route] from=`, `[route] follow moved`.

## The tester's list on build 2026-10-18-2 — 2026-10-19 (released as test-2026-10-19 from 2628b90)

Release: 123/123 CTest, 29 entries, 62,660,001 bytes, SHA-256 `7a02e52e…2adf`.

From `D2R Access test build 2026-10-18-2 bug list and feedback` (the user's Downloads: list with the
user's notes, `menu_walk_`/`death_`/`sewer_`/`wrong_house_` logs). Choices in [decisions](docs/decisions.md);
news (with replies and a multiplayer checklist) `../tester-news-draft-2026-10-19.txt`.
- Character select right stick up: out of a game nobody claimed it (`InteractionReader::TickFrontEnd`).
- Controller glyphs U+E013..E02A are UI actions (death "Press Menu", "Hold A to Open", level-up
  notices); tooltip lines keep buttons ([controller glyphs](docs/controller-glyphs.md)).
- Cube: open hint with the name; Y out of the cube to a tab said ([Horadric Cube](docs/horadric-cube.md)).
- Speech priority menu, 9 kinds x 4 levels (`SpeechPriority`; [speech priority](docs/speech-priority.md));
  status endings cut in, 500 ms after the end.
- Porch cottage is `Cott5`/`Cott5A`; sewer grate (`manhole01`) ([places](docs/places.md)).
- descriptions.txt read again on save, `descriptions-names.txt`, banner form "The Cave Level 1"
  ([descriptions](docs/descriptions.md)).
- Guidance at a shut door keeps the target ([beacon](docs/beacon.md)).

123/123 CTest; installed by the build. **Unverified live: all of it.**
**Next step:** the tester tries it; logs: `[detail]` at character select,
`[modal] YouDiedModal`, `[cube]`, `[items] sorted into the stash`, `[descriptions] read again`,
`[beacon] refused reason=` from the second player.

## The tester's list on build 2026-10-18 and the 2026-10-16 leftovers — 2026-10-18 (released as test-2026-10-18-2 from 2f2fab2)

Release: 122/122 CTest, 29 entries, 62,645,285 bytes, SHA-256 `4a7f71aa…e08d`.

From `D2R Access build 2026-10-18 bug list and extras.7z` (the user's Downloads: list, porch cottage
screenshots, `rc_`/`stairs_`/`stable_cottage_`/`hidden_cottage_` logs). Choices in
[decisions](docs/decisions.md); news (with replies) `../tester-news-draft-2026-10-18b.txt`.
- Character select: right stick up was the scanner's out of a game (`MapSurvey` now waits for
  `LocalPlayerReady`); the hint re-arms per character row.
- Vendor LT/RT under the trade menu said (`InteractionReader` conversation branch, `TabReader` baseline).
- Antidote/thawing "You are fortified" ([status effects](docs/status-effects.md)); `skill-report` on.
- Cliff stairs: full-width landings (`STAIRS_LANDING_SLACK`), `Places::HeldStairway`; buildings per
  piece, porch cottage, tower padding, sewer "tables" dropped ([places](docs/places.md)).
- Players take the NPC cue; Greater Mummies are raisers, raisers outrank elites (not bosses)
  ([entity cues](docs/entity-cues.md)); stone door loop re-cut ([ground items](docs/ground-items.md)).
- Horadric Cube notices: opened, grid name, moves, Y, transmute (`CubeState`, `CubeNotice`,
  `InventoryArrivals::Moves`; [Horadric Cube](docs/horadric-cube.md)).
- Border exits at the corridor mouth facing the player (`ZoneTransitions::CorridorMouth`; [beacon](docs/beacon.md)).
- Scanner order unchanged (explained to the tester).

122/122 CTest; installed by the build. **Unverified live: all of it.**
**Next step:** the tester tries it; logs: `[cube]`, `[tabs]`, `[places]`, `[vitals] fortifying potion
began`, `[stick]`, `[terrain] place remembered` at the Monastery Gate.

## The tester's list on build 2026-10-17 — 2026-10-18 (released as test-2026-10-18 from 4c1f287)

Release: 122/122 CTest, 29 entries, 62,634,562 bytes, SHA-256 `2719634c…f411`.

From `D2R Access test build 2026-10-17,bug list, screenshots and logs.7z` (the user's Downloads:
list, 19 screenshots, waypoint and lobby logs). Choices in [decisions](docs/decisions.md); news
(with replies) `../tester-news-draft-2026-10-18.txt`; settings tree `../settings-tree-2026-10-18.txt`.
- Waypoint words reach the speech (`NativeUi::Snapshot(..., named)`); emote wheel no longer
  matched (`NativeUi::DrawsNothing`) ([waypoints](docs/waypoints.md)).
- Places by piece file: cottage / stable / cottage on fire / tower ruin; blood fountain, burning
  buildings, crypt burning star, Hangman's Tree padded; cliff stairs up/down from the collision
  mesh, both tracked ([places](docs/places.md), `PresetFileNames`, `Places::Stairway`).
- Shop confirmation return silent, stash title dropped; rebinding says "<skill> bound to <button>"
  ([skill bar](docs/skill-bar.md)); select hint confined and short, controls on right stick up.
- Lobby: "Editing" said, stick up forward, A types ([multiplayer](docs/multiplayer.md)); main
  menu arrows kept from the character list (keyboard cursor).
- Heartbeat alert on its own voice; stamina potion notices; `skill-report` (BACK+X running skills,
  `tools/gen_skill_states.py`) ([status effects](docs/status-effects.md)).
- Radiant chests (`Generated::RadiantObject`, objects.txt chest + `Lit0`); shaman layer glides a semitone.
- Forgotten Tower ways are quest ways only while its quest runs: the client's quest flags
  (`NativeQuestFlags`, row 5, bits 2-6 started, 0/15 done; [quests](docs/quests.md)), the old
  rule when unreadable (`[quest] flags row=5 word=`).

122/122 CTest. **Unverified live: all of it.** Not done: Enter ending a lobby edit (no end-edit
call known), a sound for traps/spawners (waits for the tester's design), edge-exit notices (no
fault found; a log asked for).
**Next step:** the tester tries it; logs: `[waypoint] row`, `[places]`, `[skill-bar] binding`,
`[interaction] back from a confirmation`, `[typing]`, `[vitals] stamina potion`.

## The tester's list of 2026-10-16, waypoint evidence, lobby regression — 2026-10-17 (released as test-2026-10-17 from a3175c9)

Release: 122/122 CTest, 29 entries, 62,622,193 bytes, SHA-256 `1ad554f8…6fc2`. News (with replies to the
tester's questions): `../tester-news-draft-2026-10-17.txt`.

From `D2R Access build 2026-10-16 Bug list.txt` and `D2R access -fast travel menu accessibility
issues.7z` (the user's Downloads). Choices in [decisions](docs/decisions.md).
- Waypoint rows from the game's row table: "undiscovered", "you are here", "Terror Zone"
  ([waypoints](docs/waypoints.md), `NativeWaypoints`, `[waypoint] row ... -> open (tab= rows= level= open= player=)`).
- Town portals: level byte +8 (+0x78 high byte under the loader), not the +9 state flags; owner
  added ([portals](docs/portals.md), `[scan] Portal owners verified`).
- Forgotten Tower quest loop on the Black Marsh's door: chain 20-25 downwards (`EntityRadar::QuestWay`).
- Heartbeat recording re-cut to 103 a minute; "You are all alone"; select hint A, Start, X.
- Descriptions (approved by the user, switch `descriptions`): Hangman's Tree landmark (new
  `PlaceFeature`, place data regenerated: +2 seeds), BACK + right stick up, player's
  `descriptions.txt` ([descriptions](docs/descriptions.md)).

- Lobby regression ("text entry is a gamble", Direct Join password kicks back to the main
  window): the mouse click is gone (it reloaded every panel), the game's begin-edit is called
  instead, up/down walk the form's order (`MenuNavigationPolicy::Step`); `[menu-navigation] boxes:`
  says why a box is not a stop ([multiplayer](docs/multiplayer.md)).

Act II list (`D2R Access Act II bugs.7z`, the tester's log of 2026-10-17):
- Stash tab not said on opening: the panel is focused with no widget for ~500 ms, and the reader
  took that for the grid's own readout; it now waits up to 1 s for the focus (`[stash] opened`).
- Desert cliff stairs (Dry Hills, Far Oasis): a "stairs" place and a tracker entry with the discovery
  notice (`CliffStairs`, `PlaceService::MapLandmarks`; [places](docs/places.md)).
- Overview percentage from the automap and the rooms stood in, not the streamed rooms
  ([terrain overview](docs/terrain-overview.md): 100 % said at 48/94 stood in).

122/122 CTest. **Live (the user, 2026-10-17): Host Game typing works** (D-pad, A, keyboard, right
stick, B). Unverified live: Direct Join's boxes, the waypoint words, portal names and owners,
descriptions.
**Next step:** the tester checks Direct Join, waypoint rows, another player's portal, the cliff stairs and BACK+B.

## The tester's lists III and IV — 2026-10-16 (released as test-2026-10-16 from 876b39d)

Release: 121/121 CTest, 29 entries, 63,090,945 bytes, SHA-256 `882f62e3…e0bd`. News: `../tester-news-draft-2026-10-16.txt`.

From `D2R Access Build 2026-10-15, bug-list-with-multiplayer-issues.txt` and `... bug list IV.txt`
(the user's Downloads) and the host's log of a two-player game. Choices in [decisions](docs/decisions.md).
- Lobby D-pad reaches the text boxes again (`MenuNavigation`: only the focused box counts as typed in).
- BACK+B: a border is not repeated as a landmark; corner-to-corner areas are no crossing (`LevelAreas`).
- Forgotten Tower warp keeps the quest category when loaded. Gone targets: the beacon says lost at
  once; remembered quest objects seen gone are forgotten (`[beacon] gone`, `[terrain] quest place gone`).
- Status starts held 1.5 s (`ScreenReader::Urge`); shrine blessing end; low life heartbeat
  (`low-life-sound`, `low-life-below`, `low-life-volume`); per-class enemy volumes; `wall-bump-repeat`;
  `corpse-interval` (ms); experience "to level N"; burning bodies removed.
- "Main menu" named, panel titles held 700 ms; character hint after 5 s; stash tab on opening and
  shared pages (`[stash] opened`, `[stash] shared page`).
- **Loop speeds** group (the user): each pulsing loop kind paced 50-300 % (`LoopPace`), with the drop
  speed, wall knock repeat (from 250 ms) and corpse rattle moved there ([ground items](docs/ground-items.md));
  `radar-wall-repeat`: the radar's wall ahead again while heading into it ([radar readings](docs/radar-readings.md)).

121/121 CTest; installed by the build. **Unverified live: all of it.** Not done: locked waypoints
(diagnostic `[waypoint] row` lines), house entrances (needs a live tuning session), Cain's chime
(needs the tester's log).
**Next step:** the tester tries it; ask them for a log with the waypoint panel opened (a gold and a
grey row focused) and, if Cain chimes again, that moment's `[npc] exclamation mark` lines.

## The tester's ninth and tenth lists — 2026-10-15 (released as test-2026-10-15 from b5c6cf7)

Release: 121/121 CTest, 29 entries, 61,820,481 bytes, SHA-256 `a0017cd6…f9e2`.
Eighteen items from `../D2R Access test build 2026-10-14-2 bug list and improvements.txt` and
`../d2r Access Build 2026-10-15 bug list part II.txt`, worked alone (weekly limit; no agents, no
live run). Choices in [decisions](docs/decisions.md); news draft `../tester-news-draft-2026-10-15.txt`.
- BACK+B: the level's own sides (NE/SE/SW/NW), place by world thirds, logged landmarks listed
  ([terrain overview](docs/terrain-overview.md)).
- BACK+X without the mercenary (`mercenary-report`, off); BACK+RT's mercenary entry adds its level.
- Skill tree branch read from the visible container, cursor on a node or not (`[skill-tree] branch shown`).
- Settings list: "<group> sub menu", **Settings list** group (`settings-list-stops`, `settings-list-restart`).
- `corpse-repeat` (3 s): the rattle repeats while corpses stay near.
- Audible loot filter: LB in play, **Loot filter** group (`loot-filter`, `loot-potions`, `loot-gems`,
  `loot-equipment`); `Item::silver` from `IsSilver`.
- Forgotten Tower entrance: quest category/loop (`EntityRadar::QuestDestinations` = Levels.txt Id 21);
  exits found from twice the scan range (`MapSurvey::ExitSightFactor`).
- Chat: panel visibility opens/closes the announcement, InputBox echoed without focus (`[field] chat panel`).
- Captionless tabs as "Page N" (shared stash); locked waypoint rows say "locked" (layout heuristic);
  character select says Start/X or "No characters"; lobby list rows reachable with the D-pad.
- Pad typing in the lobby (`pad-typing`, `src/pad_typing.cpp`): A clicks a box the game does not
  edit, the right stick picks and sends characters ([multiplayer](docs/multiplayer.md)).

Built and unit-tested only (see the build line below); **unverified live: all of it.** Not done:
main menu left-from-list order (the game's own cursor; no mod code moves it), the pentagram
(needs `[scan] fire` from a Tower Cellar log).
**Next step:** the user builds a test release and the tester tries it; logs: `[terrain] ...
landmarks=`, `[vitals] BACK+RT`, `[skill-tree] branch shown`, `[corpses]`, `[items] loot filter`,
`[field] chat panel`, `[tabs]`, `[hint]`, `[terrain] place remembered ... (quest)`, `[typing]`.

## The tester's eighth list — 2026-10-14 (released as test-2026-10-14-2 from 3c33430)

Release: 121/121 CTest, 29 entries, 61,806,088 bytes, SHA-256 `13c79b9d…ea54`; also carries the sixth and
seventh lists below. News: `../tester-news-draft-2026-10-14b.txt`.

BACK+RT steps through mercenary, summons and party (life, place); BACK+LT reads the main skill
bar (triggers kept from the game with BACK in play); party panel and mercenary sheet on the
right stick; chat box read; town exits listed from anywhere in town; potions after other loot;
burning bodies named; `minion-report` now off. Choices in [decisions](docs/decisions.md).
121/121 CTest, installed. Unverified live: all of it. **Open:** the joiner's missing camp exit
may also be an unbuilt next area (needs that player's log); the crypt pentagram needs the
`[scan] fire` lines from a Tower Cellar run. Logs: `[vitals] BACK+RT`, `[skill-bar] main bar`,
`[party]`, `[stats]`, `[field] focus name="InputBox"`, `[scan] fire`.

## The tester's sixth and seventh lists — 2026-10-14 (released in test-2026-10-14-2)

Fifteen items, worked through while the user was away; choices in [decisions](docs/decisions.md),
news draft `../tester-news-draft-2026-10-14b.txt`.
- Settings list: groups within groups (drop sounds inside the glossary), ends wrap with a click or
  stop with a knock (`settings-list-stops`), every percent setting in quarters (saved levels kept).
- `pad-route-on-a` swaps BACK+A and BACK+down; `zero-points` says "0 Stat Points Remaining".
- Stash grids read while the stash is open: Y-sort from a stash grid named, `stash-notice`.
- Harmful states and curses by `states.txt` row, interrupting ([status effects](docs/status-effects.md)).
- Summons from the game's pet list (RVA `0x2A4DC10`, [summons and corpses](docs/summons-and-corpses-re.md)):
  kept out of NPCs and enemies (the 0x200 flag is the hireling's alone), listed in BACK+X
  (`minion-report`); a Necromancer's usable corpses: rattle (`corpse-cue`) and count.
- Hire window Y says the skills in full; kept quest objects listed again. ZIP name kept (the user).

121/121 CTest; installed by the build. An independent review's findings are fixed (a state's start
cut off the same hit's life alert; BACK+X lost the states with `vitals` off; a stale corpse read was
reused; the count and the rattle used different radii). Unverified live: all of it, and every new
game address is static RE (live checks open in the summons doc §7); also whether the stash listing
covers every shared page (if not, a page change within 400 ms of Y would be taken for a sort).
**Next step:** the user or the
tester tries it; logs: `[pets] Native readers`, `[scan] Verified usable corpse reader.`,
`[vitals] minions:`, `[corpses]`, `[vitals] effect began`, `[items] stashed`, `[items] sorted into
the stash`, `[stats] nothing to spend`, `[hire] Y ->`.

## Exploration room by room, routes to what is listed, turn prompts, Cairn symbol on focus — 2026-10-14 (released as test-2026-10-14 from 9c26f72)

Release: 119/119 CTest, 29 entries, 61,770,968 bytes, SHA-256 `b3e1a48b…2b9c`; also carries the 2026-10-13
section below. News: `../tester-news-draft-2026-10-14.txt`.

The user's rules ([decisions](docs/decisions.md)): a room is explored on stepping into it and the
scanner lists everything in it; what the scanner lists can be routed to (the target's room always
admitted, a guess across rooms never stood in when explored ones hold no way). A room target ends at
its first step inside (the way to a walled centre failed). Route **turn prompts** ("Now east, 21",
"Off the route to X"; `route-prompts`, on). Focusing a Cairn Stone says its symbol: the game's hover
entry (`NativeHover`, RVA 0x2A41630, READ not measured) or a clearly nearest stone
([target sight](docs/target-sight.md#the-games-own-target-2026-10-14)); Tal Rasha's entrances untouched.

Tester's log of 2026-10-14 ("the beacon loses the route"): the Stony Field was refused eight times
in the Cold Plains, 129 to 334 units away, after the tester had walked across it: the way went to the
border's middle, in its trees. Crossings seen are now kept for the game (`CrossingMemory`); an unseen
border is led to within 20 units of its middle ([area borders](docs/beacon.md#area-borders); since
2026-10-21, to its whole shared edge).

Tester's finder request: `passage-map-sides` (on, the user's choice) turns the finder's
stick 45 degrees clockwise and says passages by the room's walls, NE/SE/SW/NW = 2/4/8/10 o'clock
([rooms](docs/rooms.md)). Log: `[room] finder up (map)`.

119/119 CTest; installed by the build. Unverified live: all of it. **Next step:** try a route to the
unexplored room next door and to a far listed exit (Stony Field from Cold Plains), walk one with the
prompts, and focus each Cairn Stone with the pad and the mouse. Logs: `[hover] Verified.`,
`[route] follow`, `[beacon] lead=remembered-map-guess`, `[interaction] World target`.

## Radar wording, route description and the tester's fifth report — 2026-10-13 (released in test-2026-10-14)

The user's radar requests ([decisions](docs/decisions.md)):
- Directions as **compass points** (north = top of the screen) or **clock hours** (`direction-style`,
  Ctrl+S Navigation); distances in **units**, not steps ([spoken directions](docs/radar-readings.md#spoken-directions-2026-10-13)).
- **Route description**: BACK + right stick down / Ctrl+E says the way to the beacon's or scanner's
  target as legs with doors; no autowalk ([route description](docs/route-description.md)).

Tester's fifth list (two worktree agents, merged): version said behind the title; title pad press
logged, Space fallback 400 ms and only on the title (root cause not found: test D2RLoader without
this plugin); strengths in 25 % steps (keys `-25`, old saves start over); footstep volume in Volumes,
above 100 % the footfall is replayed (the stock step is already 255); brighter elite layer; "N gold
moved to the stash" leads; NPC bell without the tap switch and from the announcer; knock leaving the
gloves/right weapon; sheet pages on a "+" button; skill branch + choices left on opening and LT/RT;
last point's new value; hire window pad path (`KeyboardMenu::Engaged`); "Mercenary says" on the world
target line; `compare-aloud` switch (**off**: the user's 2026-10-03 choice). The cow stays (easter egg).

117/117 CTest; installed by the build. Unverified live: all of it. **Next step:** the user or the
tester tries it; logs: `[route]`, `[modal] title:`, `[pad] the game polled`, `[footsteps] boost`,
`[items] gold deposited`, `[npc] exclamation mark`, `[skill-tree] branch`, `[stats] value after its
button went`, `[hire] list entered, pad`, `[compare] aloud`.

## Tester's fourth report — 2026-10-13 (released as test-2026-10-13 from ebc58d6)

Release: 116/116 CTest, 29 entries, 61,745,440 bytes, SHA-256 `ac816e15…efeb`; the plugin says
"test-2026-10-13" at start.

Sixteen items; choices in [decisions](docs/decisions.md). Done, built and unit-tested only:
- Sounds: shrine/well swapped (well = the game's drink sound); enemy classes as layers over one
  growl, boss apart from elite (`enemy-class-sounds`, on) ([entity cues](docs/entity-cues.md));
  pan/pitch strengths 0-400 % (new keys, so a strength saved in the old steps starts over from
  the TOML once); pickup click out of the glossary and the list, and off.
- Settings list in seven groups; right/Space opens or changes, left/Backspace returns.
- NPC "!" bell (`npc-alerts`): overlay 72 read live, rings when it appears and after the NPC's
  name ([NPC proximity](docs/npc-proximity.md#the--over-an-npc-2026-10-13)).
- Experience percent in BACK+X (`experience-report`) and in the controller's mercenary panel
  (stats 13/30, [vitals](docs/vitals.md#experience-2026-10-13)); "Mercenary says:" (`mercenary-bark`).
- Footstep volume 25-300 % on the game's own footfall ([footsteps](docs/footsteps.md#footstep-volume-2026-10-13)).
- Inventory (worked in a worktree, branch `report4-inventory` `34de7b1`, applied here): the stash
  notice no longer cut by the cell; the backpack-to-vendor/stash knock judged per crossing; empty
  gem/rune/material slots say "<name>: 0"; gold says personal or shared stash.
- Hire window: skill names follow the row, Y says them or the game's tooltips ([character panel](docs/character-panel.md)).
- Item sound sets (`item-sound-set`: Diablo IV, Last Epoch gold and potions, Diablo II drops)
  and the drop loops' pace (`drop-sound-speed`, 50-300 %), with a glossary group of the 45 drop
  sounds named by the game ([ground items](docs/ground-items.md#the-games-own-drop-sounds-2026-10-13)).
- Volumes group: radar and walls, enemies and NPCs, items and places, guidance (0-200 %).
  LB/RB (Shift+Space) lower and raise a setting while the list is open.

- Cairn scroll: the order was read but cut off by the inventory the game reopens beside it
  (tester's log); it now leads for 1.5 s (`ScreenReader::Lead`, [Cairn](docs/cairn-stones-re.md)).
- Build: each shared `src` file compiles once for all tests (556 -> 121 test compiles), and
  `build.ps1` runs below normal priority on all but two logical processors.

116/116 CTest; installed by the build. An independent review's findings are fixed (the stash
notice now really queues: the reader ignored its own interrupt flag). Unverified live: all of it;
every new game address is static RE. **Next step:** the user or the tester tries it; they listen
to the glossary (enemy layers, NPC bell, drop sounds) and check the logs: `[npc] exclamation mark`, `[vitals] experience`, `[footsteps] volume stock=`,
`[hire] Y ->`, `[stash] empty slot`, `[stash] gold tab=`, `[grid-edge] crossed`.

## Tester's third report — 2026-10-12 (released as test-2026-10-12-3 from cd03aa1)

Release: 116/116 CTest, 29 entries, 60,255,237 bytes, SHA-256 `8fb17af8…100c`; the plugin says
"test-2026-10-12-3" at start.


Thirty items with the user's notes; choices in [decisions](docs/decisions.md). Done:
- Movement: walk/run only on L3 ([vitals](docs/vitals.md)); bump contact off the map and beside
  the line; footsteps muted while a push is refused ([footsteps](docs/footsteps.md)); radar +3 dB,
  door +3 dB; **wall tones** (`wall-tones`, off) ([radar readings](docs/radar-readings.md)).
- Beacon on walkable ground only, no straight fallback ([beacon](docs/beacon.md#walkable-guidance));
  scanner nearest first, fresh after 10 s ([scanner](docs/entity-scan.md)).
- Inventory: knock only on grids and between backpack and vendor/stash; shop named by its vendor;
  Y-sort says the stash tab; Strangling Gas name; gold amount on pickup; merc skills on Y and in
  the merc panel; death screen read; skill tooltips join wrapped sentences and split style blocks.
- Sounds: racks (and armor stands, bookshelves) loop; shrines and wells loop (placeholders);
  Last Epoch gold/potions switch; cows and camels named.
- Ctrl+S: mana alerts and pan/pitch strengths (settings of several values), BACK kept from the
  game's automap (`pad-back-to-mod`, on). Version spoken at start (`-Release` / test tag).
- Title screen: a pad press the title ignores for 1 s gets a Space in its place (`[modal]` log).

- Follow-up the same day: skill-tree LT/RT branch names were cut off by the skill's description
  (it rendered ~350 ms later and interrupted); what follows on the same focus within 1.5 s of a
  title now queues. Multiplayer (Raine's log): the captions were right; the tab reader, held while
  the quest log was open, woke a poll after Character opened with the bar still on Quest Log and
  said it over "Character". That stale caption is now adopted in silence (`TabReader`). Her log
  also showed 3651 slow updates in 70 min (VirtualQuery 0.2-0.3 ms there): page permissions are
  now kept for 1 s, empty pad slots asked every 2 s, and the scanner's pass skips menus and slows
  when standing ([latency](docs/navigation-latency.md)); `[survey] slow pass` names its parts.
- Low-quality ground items named as the game does ("Crude Short Sword"): ItemData+0x34 row,
  table bank +0x1688, fingerprinted and matched against the live dump ([ground items](docs/ground-items.md#low-quality-names-2026-10-12)).

116/116 CTest; installed by the build. Unverified live: all of it. Left: Act 1 cottage entrance
hints; update check (private repo).
**Next step:** the user or the tester tries it; logs to read: `[wall-bump]`, `[footsteps] summary
pushing=`, `[beacon] lead=`, `[modal]`, `[items] sorted into the stash`, `[vitals] movement re-synced`.

## Tester's second report — 2026-10-12 (released as test-2026-10-12-2 from fe4d171)

- Town NPCs everywhere listed only loaded units: unbuilt rooms' presets (`DrlgMap+0x58`) are
  now read, and a spawn is dropped only near the player ([scanner](docs/entity-scan.md#town-npcs)).
- Tome/stack count was lost when the headline was rebuilt (`TooltipText::Headline`); the size
  under autodetail is kept after it. Skill tooltip lines are joined with a full stop.
- "Silver" for socketed/ethereal white names (`silver-items`, [evidence](docs/silver-items.md)).
- Gem/rune stash slots say their drawn count (`AdvancedStashSlotWidget+0x618`, static RE only).
- Inventory edge knock judged taps too (a pad tap is shorter than the old 150 ms hold);
  `[grid-edge]` logs say whether focus stayed or left the grid.

- Live 2026-10-12: wall bump contact now reaches two cells on either axis (screen directions
  are world diagonals); `[wall-bump] silent refusal` logs what stays silent. Inventory buttons
  (gold) knock as one-cell grids. Stat points end the Strength button's announcement; a spent
  point's new value is said without leaving the button ([progression](docs/progression.md)).

114/114 CTest; installed DLL SHA-256 `8CAC2D96…` matches the build. Live checks: `[scan] Verified
unbuilt-room preset reader.`, `[survey] town NPCs on level 1`, `[stash] slot ->`, `[grid-edge]`,
tome headline in `[item]`.

## New tester's report — 2026-10-11 (released as test-2026-10-12)

A new collaborator's list (controller, English game) was worked through; what changed and the
defaults kept for the user are in [decisions](docs/decisions.md#2026-10-11--a-new-testers-report-what-changed-and-what-stays-the-users-choice).
- Pad: beacon moved to **BACK+LB** (L3 is the game's walk/run toggle). Walk/run, stamina and
  ammunition announcements ([vitals](docs/vitals.md)); weapon set number as a switch.
- Beacon arrival at 3 subtiles from the exact position; a target that vanishes on arrival is
  reached. Scanner enemies follow their unit and are dropped when dead ([scanner](docs/entity-scan.md)).
  "Right here" under 1.5 subtiles. English: up/down, "passages" between rooms.
- Growls from the screen edge, 6 voices; pan/pitch strength settings ([entity cues](docs/entity-cues.md)).
- UI: title prompt, typed-text echo, skill-tree branch first, vendor LT/RT tabs, stat points and
  Strength focus, tome quantity, no more "scroll controller", screen hints.
- Pickup-range click, inventory edge knock; town NPCs everywhere and the weapon set number are
  switches, on by default (the user, 2026-10-12).
- Wall bump thud (Last Epoch's design, from the wall-slide hook's refused moves) and a small
  obstacle knock on the radar ([radar readings](docs/radar-readings.md)); sound glossary at the
  end of the Ctrl+S list (`src/sound_glossary.cpp`).
- Tester package docs are English (`packaging/tester/README.txt`, `KEYBOARD.txt`); the published
  release notes were rewritten in English.
- The settings list (and its sound glossary) on the pad: **BACK+RB**, then the right stick
  (up/down step, left/right change); see [decisions](docs/decisions.md) 2026-10-12.
- bladestorm360's stability fix (`4c88812`, allocation containment and worker teardown) is
  applied on main ([stability](docs/stability.md)).

Every item is built and unit-tested only (112/112 CTest): no live check. Installed DLL
SHA-256 `F96726AC…` matches the build. An independent review found no crash or
hang; its four behaviour bugs and two minor ones are fixed. **Next step:** the user (or the tester)
checks BACK+LB and BACK+RB, the walk/run speech and stamina values in `[vitals] movement`, the growl range in
a pack, the title prompt and the text echo. Committed as `f6c7431` and released as `test-2026-10-12`.

## Test releases — 2026-10-11

`tools\test-release.ps1` builds `HEAD` in a separate tree (the install is untouched), runs CTest,
packages the complete tester ZIP and publishes it as the latest GitHub release. First one:
`test-2026-10-11` from `df45422` (106/106 CTest, 29 entries verified). Always-latest link and
details: [test releases](docs/test-releases.md). The installed plugin is still the
`port/bladestorm-npcs` build, which is not in that release. Second: `test-2026-10-12` from
`f6c7431` (112/112 CTest, 29 entries verified, English package and news). Third:
`test-2026-10-12-2` from `fe4d171` (114/114 CTest, 29 entries verified).

## Keyboard as a virtual controller — idea, 2026-10-05

The user is not satisfied with the keyboard cursor and plans to replace it with keys that
drive the game's own controller mode. Viable: a first version (2026-09-19) measured the hook
point (`xinput9_1_0!XInputGetState`, all four slots polled) and that controller mode is one-way.
It was withdrawn for displacing a real pad and breaking name typing; both have known fixes.
Code recoverable from `85aac6c^`. [controller mode](docs/controller-mode.md)
**Next step (own session):** restore the detour, inject only with no physical pad, and check
that movement reaches the game.

## Tester ZIP — 2026-10-05

`../d2r-access-completo-2026-10-05.zip` for a new tester: plugin from `df45422` (`B3FD5BB5…`,
matches the install; 106/106 CTest), loader 1.3.1-beta from `../loader-backups` (hashes match),
`radar.toml` as `d2r-access.toml`, the loader's default `d2rloader.toml`, PRISM and its licences,
four x64 VC 14.44 runtime DLLs, the Spanish keyboard list as `COMANDOS-TECLADO.txt` and a full
Spanish LEEME (install, Steam launch option, language, logs). 29 entries, 58,906,074 bytes,
SHA-256 `c80ac057f0c8551a659fb0e0dd9c1eb6ba08a62d4ed1db8f7d63970c98cdd29c`; every entry matches.
**Next step:** send it.

## Inventory lag in the keyboard build — reported 2026-10-05

A tester reports lag in the inventory and shops with the controller, worse on cells
with items. The grid census (a diagnostic) now runs only with `access-itemlog on`, and
`[pump] worst update` logs stalls from 8 ms. Built and deployed; **106/106 CTest
pass**; not committed. Suspects in
[navigation latency](docs/navigation-latency.md#keyboard-build-lag-report-2026-10-05).
The user felt no lag in their own inventory (2026-10-05 16:42; no shop opened): no tooltip held
back, no update of 30 ms, worst updates 9-15 ms (`InputDirection`, `KeyboardService`, `MapSurveyService`).
**Next step:** the tester tries a shop with the new zip; if it still lags, their `d2r-access.log`.

## Shortcut menu — planned, 2026-10-03

The collaborator's controller and keyboard schemes are adopted. The keyboard has ~60 bindings.
Planned: one menu with every mod action, rebindable or unbound, reachable from the pad too.
Toggles and duplicates go unbound by default. See [decisions](docs/decisions.md).
**Next step:** after the integration's live checks, design it.

## Quest objects — 2026-10-03

The objects a quest needs used now get the quest cue, a **quest** scanner
category and priority for D. These are the Orb, the seals, the Altar of the
Heavens, the Hellforge, Anya, the orifice, the Tainted Sun and Gidbinn altars,
Cain's Gibbet and the Kurast sewer lever. The seals are numbered, and "Seals
left: N" is spoken. The opened sewer stairs are an exit. Attackable objects
get the same use command as D sends (READ), so D is unchanged.
**106/106 CTest pass**; built and deployed with the game closed (installed DLL `CB822E57…` matches the build); not pushed.
**Next step:** the live checks in [quest objects](docs/quest-objects.md#live-checks-still-needed).

## Main's features restored on `integrate/bladestorm` — 2026-10-03

`integrate/bladestorm` is the collaborator's tree (with his controller and keyboard
schemes) plus main's build files. Restored from `main`, one commit each (`ebd575e`..):
- `radar.toml` and main's defaults: exploration-first, wall-slide prevention on, growls and
  reach ping on, `[boss_life]`.
- Radar breath rising up, falling down (`09fdf5f`).
- Scanner categories *Rooms* and *Unexplored rooms* (`src/room_catalogue.*`).
- Monster plate gate (reach, 10 s repeat, never interrupts, "•" as a pause) beside the
  branch's plate words; `status` says the reach.
- Belt cells: D-pad button, row, column count, automatic potion (`src/belt_reader.*`).
  "Item in hand" / "Placed in" behind `hand-speech` in Ctrl+S, on by default.
- Docs: `docs/rooms.md` back (exit finder keys), README sections back, stale keys fixed.

Main's lost keys stay reachable: named places and autodetail in Ctrl+S; room exits with
Ctrl+Y and the Ctrl+V finder (PageUp/PageDown); life and mana with Z and X. No key added.

**105/105 CTest pass.** Built and deployed with the game closed; installed DLL SHA-256
`2C65A2F5…` matches the build. Not pushed.

**Next step:** user-led live check of the growls and ping defaults, the plate in a pack, the
belt readout and hand speech, and the Rooms categories.

## Additive upstream integration, retreat and repair — 2026-10-03

**Built and deployed with D2RLoader 1.3.1-beta after the user saved/closed the
game. Full CTest 103/103 passes, timeout 20. Live checks remain user-led.**
The user authorized committing this reviewed work and pushing it to
`origin/bladestorm`; local development remains on `keyboard-local`.
Reviewed upstream through `b117bdf`, with
current local work retained and replacement policies/sounds deferred.
[Integration/evidence](docs/upstream-2026-10-03.md) lists the scope.

The user's death log confirms Ctrl+R engaged but repeated orders through
monsters. Retreat now routes/tests legs with live collision and changes an
obstructed order sooner. The actual anonymous collision capture is a passing
regression; escape/survival still needs user-led live confirmation.

Repair failed at quote 6919 and succeeded after reload at 7430. No plugin stat
write was found; retail reports match, but D2RLoader is not excluded. On the
collaborator's machine, installed binaries/startup before this update were
**1.3.0**; the 1.3.1-beta install recorded below (2026-09-26) is ours and stands.
Official 1.3.1-beta is now installed/hash-verified there too; its item/stat
fixes are not a proven repair fix.

New inventory names/socket/arrival feedback, town-NPC memory, death-place
memory/guidance, named places, boss-life and faster skill names are integrated.
Plain pickup/release remain silent. New ordinary growls and reach ping default
off. Local keyboard/controller, teleport routing, filter, custom sounds,
existing room/scan catalogues and Tal Rasha symbols remain.

Installed DLL SHA-256 `2C9AD3E6…` matches the final normal build; logs and
full deployment manifest in `../re/upstream-2026-10-03/`. Verified old runtime
backup: `../loader-backups/pre-upstream-2026-10-03/`. Only three loader root
runtime files and the built plugin/PRISM were installed; loader config hash is
unchanged and no saves/settings/other plugins were copied. Normal build target
is restored to the game install, four jobs/BelowNormal. Game remains closed.


## Tal Rasha symbols and architecture survey — 2026-10-03

The user's latest log speaks The Seven Tombs' symbol caption at 07:26:45 and
07:26:49 without naming the graphic. The visible `TalRashaSymbol` image frame
now supplies a plain shape name after the objective, including repeat. Loaded
and preset tomb entrances, plus restored fixed places, include their own
marking (e.g. "To Tal Rasha's Tomb: circle"). Shared mapping: levels 66–72
correspond to journal frames 0–6; vocabulary in all 13 locales.

**Built/deployed with the game closed; 96/96 CTest suites pass.** Installed DLL
SHA-256 `BB80B7CD…` matches the build. Live speech pending;
select The Seven Tombs, then compare its shape name with the Canyon exits.
Evidence/guards/tests: [Tal Rasha symbols](docs/tal-rasha-symbols.md).
New [architecture guide](docs/architecture.md) maps current UI/world/native
layers, threads, routing, discovery, persistence and RE inputs; it also
corrects the README's obsolete statement that keyboard support is planned.

## Skill tree reads at once — installed, 2026-10-02

User (sorceress): half a second before each skill in the tree is read. Log: 420–456 ms from focus
to speech on every move; the reader waited for the game's tooltip. Now the name comes from the
skill tables by the cell's page/row/column (offsets checked on every row of `skills.bin` /
`skilldesc.bin`), one poll after the move; the tooltip only adds the detail. **87/87 suites
pass**; installed DLL SHA-256 `0E4F069F…` matches the build. **Live 2026-10-02: user confirms it
works.** `[skill-tree] … differs` in the log would mean a wrong table name. `docs/skill-actions.md`.

## Named places and corridors — installed, 2026-10-02

User: names for parts of the map (bridge, tavern, palace entrance), then detail inside them (the
hall, the counter), then what the game does not name (corridors). Walking in now speaks:
- the named piece (`RoomEx+0x74/+0x40` → LvlPrest map, `src/preset_map_layout.h`);
- the interior and the landmark nearest by walking distance, from each preset's HD layout (props
  and prefabs: `TOWN_2_BAR` = Atma's tavern, `barstool`, `pf_town_blacksmith01` = Charsi's forge);
- in maze levels, corridor / dead end / bend / junction from the live grid, scaled by the maze's
  own doorway width, plus what a corridor leads to ("pasillo, hacia Escalera a …").

Toggled then by **Ctrl+U** (now `places` in the Ctrl+S list); `access-places where` asks; `access-places dump` logs the live grid. 578 pieces,
2418 landmarks, 82 interiors (`tools/gen_places.py`). **87/87 suites pass** (new `places`,
`passages`). Installed DLL SHA-256 `AB15AF70…` matches the build.

**Live 2026-10-02.** First runs: nothing spoken. The game keeps the File column as
`data\global\tiles\…`, the lookup missed it; fixed. Third run works:
- Lut Gholein: mesas, puestos, taberna, barra, mesas, fuera (east door), taberna.
- Rogue Encampment N1 (origin tile 1024,1256): forja at Charsi.
- Blood Moor: río on the river pieces (def 26), but twice in 5 s along a chain of pieces. Now a named
  piece's name holds within one piece size of it, and only a new kind is spoken.
- Den of Evil: no corridor spoken. Its piece is one 24×24-tile cave preset. The real grid rebuilt
  from `[radar-collision]` records shows caverns joined by necks, and the path stayed in the cavern.
  Corridors are still unproven in the Catacombs, Barracks and Jail.

- User: say "forja" when passing through the forge, not within 7 steps of a point. Landmarks are now
  footprints from their prefabs (forge 31×30, tents 10–26, wagons ~17×12), at = on it or one tile off.
  Replaying the camp walk: hoguera, forja, hoguera, carro, tiendas, carro, tiendas (was only forja).

**Next step:** walk the Rogue Encampment past the forge, Akara's tent, Gheed's wagon and the bonfire;
then the Catacombs, Barracks or Jail for corridors (`access-places dump` where one is wrong).
`docs/places.md`.

## Enemy growl, attack reach ping, quiet monster plate, boss life — installed, 2026-10-02

User: the plate spammed monster names, even out of reach. Friend: Diablo 1-style monster
sounds and a sound when an enemy is in reach (a ranged player was shooting blind). User: narrate
boss life like their own. **85/85 suites pass**; installed DLL SHA-256 `64A98CE7…` matches the
build. Unattended run reached only the title screen (the user was using the PC), but its startup log
shows `monster=1` (new fingerprints match under 1.3.1-beta) and the new settings registered.

- Every enemy has a cue: the three nearest growl, each on its own, after the priority classes
  (design of mojsior/diablo-access, the D1 mod the friend meant).
- A rising ping when an enemy in sight comes within attack reach; melee or ranged from the
  bound skills and weapon (skills.txt `Range`/damage, weapons `rangeadder`, found by table
  correlation). `access-entity-cues attack-melee|attack-ranged|attack-auto`.
- Plate names only within reach (bosses/elites always), never interrupting, no repeat while
  the target trades; the affix line's stray "5• 0" is gone.
- Boss life (bosses, superuniques) on the player's life steps, from the game's own plate fill.

**Live 2026-10-02 (sorceress, Blood Moor, Den of Evil).** Growls and 15 reach pings played.
- Enemy names barely spoken: 11 of 16 held back as beyond the 15-subtile ranged default, while
  Glacial Spike hit from 20–23. Ranged attacks now name what the auto-target picks; melee keeps the
  reach rule.
- Next run: the Fallen still unnamed. The gate admitted the name, the reader's 80 ms settle lost it
  to the next hop, and from then on it was "just heard". Admitted names now speak at once
  (regression test fails without the fix).

**Next step:** user fights a pack and a boss (Blood Raven / Rakanishu): each kind of enemy named once,
the ping distance for their class, the life steps. `docs/attack-reach.md`, `docs/boss-life.md`,
`docs/entity-cues.md`.

## Friend's tester ZIP v2 — 2026-10-02

`../d2r-access-completo-2026-10-02-v2.zip` (the only package kept; the user wants older ZIPs and
`.sha256` files deleted): the first 02/10 package (user confirmed it works) with the skill-tree
plugin (`0E4F069F…`, d97041e + the uncommitted fix; matches the build and the install, tested live
by the user; 87/87 suites). 26 entries, 58,468,395 bytes, SHA-256
`9dae07903e7d1956fc76a6faea4aafb4863aeed5532f678f01627fccea622ebe`. Loader 1.3.1-beta, PRISM,
licences, VC runtime and `d2r-access.toml` byte-identical to the first 02/10 package. Spanish
LEEME with the skill-tree news first. Every entry matches its SHA-256.
**Next step:** send it.

## Radar breath rises up, falls down (D4 mod's sound) — installed, 2026-09-28

User could not tell an opening ahead from one behind. The floor breath is now the D4 mod's, copied
unchanged: band-passed noise sweeping 900→2400 Hz up, 600→220 Hz down, steady 1200 Hz sideways,
180 ms. **82/82 suites pass.** Installed DLL SHA-256 `95251F45…` matches the build.

**Next step:** walk a corridor with openings ahead and behind and judge the two breaths under the
game's ambience; also check the falling breath is not mistaken for the enemy swoop.
`docs/radar-readings.md`.

## Way back to the last death place — installed, 2026-09-28

User died to Radament (Sewers Level 3), pressed *Save and exit*, and had to find the room blind: the
new game put the corpse in town. Now the death screen records the place where the player last stood.
It is kept per map in `explored\deaths.txt`. The corpse category lists "donde moriste" on that level.
On other levels it lists the known exit toward it ("camino a donde moriste, *level*: *exit*"), or the
waypoint when no walking route is known. It is forgotten once reached. **82/82 suites pass** (new
`death-place`). Installed DLL SHA-256 `3B06AFDC…` matches the build.

**Next step:** die somewhere two levels deep, *Save and exit*, rejoin. Check the corpse category in
town and on each level on the way, and `[death]` in the log. `docs/death-place.md`.

## Seen town NPCs from anywhere in town — installed, 2026-09-28

User, in Lut Gholein: the town is huge. They asked for tavern/smithy-style places and an unlimited
NPC range, only in town, then for NPCs not yet seen to stay out. **No district names:** the game has
none. Lut Gholein is one level from one preset (`docs/map-landmarks.md`). **Town NPCs:** in a town,
the scanner's NPC category lists every NPC already seen, from anywhere. Seen means in range and in
sight, or in a room the automap drew. It uses the loaded unit, or else the NPC's spawn point from the
level's presets. The beacon follows either. Full assistance lists all of them. Outside towns nothing
changes. **81/81 suites pass.** Installed DLL SHA-256 `50B089FA…` matches the build (it also carries
the sockets/cinematics entry below).

**Next step:** in Lut Gholein, cycle the NPC category from a far corner and follow one with L3.
Check `[survey] town NPCs`: the total should be every town NPC, and "seen" only those met. A missing
one points to rooms without presets. `docs/entity-scan.md#town-npcs`.

## Filled sockets read natively, cues paused in cinematics — installed with the town NPC build, 2026-09-28

**Sockets:** the RE found that an item's socketed gems and runes are its own inventory. The item
count sits at `Inv+0x4C`, verified live: the topaz sword read 1, the controls 0. The reader of the
item under a gem in hand now says one of:
- "Huecos libres: N";
- "Huecos llenos";
- "Sin huecos";
- only the total ("Con huecos: N"), if unreadable.

Socketing says "Engarzado en X. Huecos libres: N". New `SocketChecks` fingerprints:
`docs/inventory-npc-followup.md#socket-contents`.

**Cinematics:** act videos (`MoviePlayerMessage:Play` … `Cinematics:MovieComplete`) now pause every
cue, as a pause menu does. The user heard the NPC cue through the Act 2 video.

**81/81 suites pass.** Installed with the town NPC build (`50B089FA…`).

**Next step:** live checks. The Filo de la tormenta should say "Huecos llenos" with a
gem in hand, and the next act video should be silent.

## Socketing: the SDK's filled-socket count is wrong — built, not installed, 2026-09-28

User: the Rubí would not go into a sword the reader said had "Huecos libres: 1".

The sword's one socket already holds a topaz: its "Añade 1-8 p. de daño de rayos" line is the chipped
topaz bonus. The SDK read `socketedItemCount` 0, so the game rightly refused ("No puedo" on the worn
slot, a swap in the backpack).

Changes:
- **Hovering with a gem in hand** now says only the total ("Con huecos: N") or "Sin huecos".
- **"Engarzado en X"** comes from the gem leaving the hand onto a socketed item that stays.
- **A refusal** (swap or "No puedo") says "No se pudo engarzar".

**81/81 suites pass**; plugin objects compile. **Not installed:** the game was running.

**Next step:** `.uild.ps1` with the game closed. The filled count needs the item inventory list
offsets (RE). `docs/inventory-npc-followup.md#third-live-session-2026-09-28-the-sdks-filled-socket-count-is-wrong`.

## Bought and taken-off items say where they landed — installed, 2026-09-28

User: a bought or unequipped item sometimes "is not in the inventory" until the panel is reopened.
The log showed no stale read. The purchase was silent and the item simply went unsearched: the sword
was in column 3, only columns 1, 2 and 4 were searched, and potions may go into the belt.

While the inventory is open, items new to the backpack or belt, or moved there from the worn slots,
are now spoken with their place. For example: "Nuevo en el inventario: X, Fila 1, columna 3" or
"Desequipado: X, …". It uses the SDK's inventory walk every 100 ms. **81/81 suites pass.** Installed
DLL SHA-256 `A6998DF8…` matches the build (it also carries the socketing entry below).

**Next step:** buy a potion and a weapon, unequip a helm, and check `[items] arrived` in the log. If a
row or column comes out swapped, that is the SDK's `x`/`y`.
`docs/inventory-npc-followup.md#arrivals`.

## Socketing: the item under a gem is named, the result is announced — installed, 2026-09-28

User: with a gem in hand, the target sword was silent, and nothing said whether it was socketed.
The log showed why:
- **The sword was silent** because the game shows no tooltip while an item is in hand, and magic or
  better names waited for one. The name now comes from the last tooltip seen for that item, or else
  the table name and quality.
- **There was no socketing feedback:** it was announced as "Colocado en Fila 2, columna 1". The SDK's
  socket counts now tell it apart. It says "Engarzado en X", plus "Huecos libres: N" when sockets
  remain. A tome or stack gets "Añadido a X". A swap gets "Intercambiado. En mano: X".

**80/80 suites pass**, including the new `item-hand` suite. Installed DLL SHA-256 `7D793EA9…` matches
the build.

**Second live session:**
- **Every swap was correct.** The target, "de electrocución Mandoble", has no sockets (`sockets=0/0`):
  "Max Sockets: 2" is the RT range line, not sockets it has.
- **A gem thrown with X was announced as "Añadido a"**, now fixed.

Now, with a gem, rune or jewel in hand, each item under it says "Sin huecos", "Huecos libres: N" or
"Huecos llenos". The hand filler comes from the ItemTypes `sock` ancestry. "Añadido a" needs the
target's quantity to grow, and X is "Tirado al suelo". Installed `F2F32A41…`; 80/80 pass.

**Open:**
- Antidote, thawing and stamina potions: the game's own tooltip has only their name (price at a
  vendor). Nothing is lost by the reader.
- Buying: see the arrivals entry above.

**Next step:** socket a gem into an item with a free socket, then try one on an item with none. Check the log for `[item] hand outcome=`. `docs/inventory-npc-followup.md#socketing`.

## Monastery Gate: border found at its crossing, beacon survives a step across — installed, 2026-09-28

User report from the Monastery Gate, checked against the live log:
- **(1) Fixed: the scanner listed the gate late.** A border counted as discovered only at the middle of the
  shared edge, in a room the automap had not drawn. It now also counts at its walkable crossing.
- **(3) Fixed: L3 to the Outer Cloister "led back to Tamoe".** The route was right (east alcove, then the
  strip along the Tamoe border). One step into Tamoe released the lock. Now it parks and resumes on the
  way back.
- **(2) No change: an opened door does not read open.** This is the cardinal frame's known limit: gates
  are crossed on a screen diagonal, so the door posts block both rays beside it. The user found it
  works well enough.

Both fixes have a test that fails without them. **79/79 suites passed** at the time. Installed with the
socketing build (`7D793EA9…`).

**Next step:** in the east alcove, lock the Outer Cloister, then step into Tamoe and back. Expect `[beacon] resumed` in the log.

`docs/beacon.md#area-borders`.

## Friend's tester ZIP v2 — 2026-09-26

`../d2r-access-completo-2026-09-26-v2.zip`: the first 26/09 package with **D2RLoader 1.3.1-beta**
in place of 1.3.0 (same plugin `37E001FD…`, config, PRISM and VC runtime; every file matches the
live-verified install). 26 entries, 58,599,852 bytes, SHA-256 `ac1de101…` (full hash in
`.zip.sha256`). LEEME: loader news, "both players use this package", the 1.3.1 camp check, and
the language note softened (not rechecked on 1.3.1). Every entry decompresses and matches its
SHA-256. Supersedes the first 26/09 ZIP. **Next step:** the friend installs it.

## Loader 1.3.1-beta — installed, 2026-09-26 (live-verified)

There is no loader "3.0.1": the latest is **1.3.1-beta** (2026-09-24, plugin ABI 4, SDK 0.3.0), in
`../loader-backups/1.3.1-beta/` with hashes. The **unchanged plugin** (`37E001FD…`) runs on it:
- **Offline:** all six D2RCore descriptors resolve once each (0x7004A8 player index, 0x7004D8
  lookup, 0x701BB0 manager, 0x703250 findChild, 0x7034D8 findPanel, 0x705EC0 setText). The 51
  game RVAs the new loader adds hit none of the mod's 135 fingerprinted ranges.
- **Live A/B** (unattended, camp, esES, same steps on 1.3.1 and 1.3.0): every "Verified" reader
  installs, radar ready, NPC and exit review speak in Spanish, no new warnings, no crash files. The
  `[pump] slow accessibility tick` warnings (31–63 ms, about 1 per 5 s) happen on 1.3.0 too.
- **SDK kept at 0.2.0** (`docs/decisions.md`, 2026-09-26).
Rollback: copy `../loader-backups/1.3.0-beta/` into the game root. A friend on 1.3.0 and a
player on 1.3.1 may not be able to play together; the 26/09 v2 ZIP carries 1.3.1. The language
bug below was not rechecked on 1.3.1 (the runs set `esES` explicitly).

## Friend's tester ZIP — 2026-09-26

`../d2r-access-completo-2026-09-26.zip`, plugin from 6928fd2 (SHA-256 `37E001FD…`, matches the
install; 79/79 suites): 26 entries, 57,885,387 bytes, SHA-256 `64ad2784…` (full hash in
`.zip.sha256` beside it). The v2 package minus the `sounds` folder (23 sounds now embedded),
current `radar.toml` as `d2r-access.toml`, and a Spanish LEEME with today's news (discovery sound
and quest objects, weapon rack and stash loops, levels, following a friend, stalled audio), a note
to delete the old `sounds` folder, the loader-language workaround and "copy the log right after a
failure". Loader, PRISM, licences and VC runtime unchanged. Every entry decompresses and matches
its SHA-256. **Next step:** the friend installs it.

## Chest louder, stash softer and slower — installed, 2026-09-26

User: the chest is still too quiet, the stash too loud and too fast, and the stash should stop
while it is in use. Measured as heard (looped, LUFS): chest -18.6 against about -14.5 for the
loot loops. Runtime level trims per sound: chest +4 dB, stash -4 dB; the stash repeats once a
second instead of every 0.3 s and pauses while the stash panel is open, back on close.
**79/79 suites pass**; installed DLL SHA-256 `37E001FD…` matches the build. **Next step:**
listen at a chest beside other loot, and open/close the stash (`[items] stash open` in the log).
`docs/ground-items.md#weapon-rack-and-stash-loops`.

## Weapon rack and stash loops, louder corpse — installed, 2026-09-26

User supplied `weapon rack.ogg` and `stash.ogg`. Weapon racks (OperateFn 20, 4 classes) loop
their own sound instead of the chest (armor stands keep the chest); the stash loops with the
exits (Ctrl+P). The corpse click, measured 6 dB under the other loops, now gets a 42 dB
build boost instead of 24 (-17.7 → -11.8 dB). 23 embedded sounds. **79/79 suites pass**;
installed DLL SHA-256 `F5DA274F…` matches the build. **User-confirmed live 2026-09-26:** the new
audio cues work.
`docs/ground-items.md#weapon-rack-and-stash-loops`.

## Discoveries: quest objects and a discovery sound — installed, 2026-09-26

User asked whether the radar announces dungeon entrances. It already did: stairs and cave
mouths are exits to the scanner (live log 2026-09-25: "Al nivel 1 del Pasadizo Subterráneo").
New: an unused quest object (the quest/tome loop sources: Inifuss tree, Malus, quest chests,
Cairn Stones, Moldy Tome) is announced once per map ("…un objeto de misión nuevo: Árbol de
Inifuss"; the five stones are one find), and every announcing scan plays the user's
`sounds/one shot sounds/exploration/discovered.ogg`, embedded in the DLL like the others
(21 sounds at the time). `[discoveries] enabled` covers both. **79/79 suites pass**; installed DLL
SHA-256 `D7085FB7…` matches the build. **User-confirmed live 2026-09-26:** discoveries work
(`[item-audio] 23 sounds loaded` in the log). `docs/exploration.md#speech`.

## Multiplayer: following a friend, stalled audio — installed, 2026-09-26

User report from a two-player session (log lost: the loader overwrites it at each launch).
(1) On one map L3 could not follow the friend, later it could; (2) after both took a portal
back into a dungeon the radar went mute, possibly a desync. Code audit and fixes:
- **Friend placement:** a friend whose unit is loaded but outside the scan's room and its
  neighbours was placed at the roster X/Y, which the game refreshes only beyond 50 subtiles
  (stale, even from the previous level). The scanner now looks for other players across the
  level's loaded rooms, only when the roster lists someone else. Test fails without the fix.
- **Stalled audio:** radar/beacon/entity cue players waited forever on a buffer the device
  never returned (e.g. a headset switching profile for voice chat). Now one second past the
  buffer's length counts as a device failure and the output is reopened (`src/wave_out.h`).
- **Log:** `[radar] lifecycle=` prints `eventPlayer=`.
- **Following a friend** (user's choice): L3 now follows another player through rooms this
  player has not explored, even in exploration mode; before, the lock was refused
  (`outside-explored`) or ended after 3 s. Test fails without the change.
**79/79 suites pass**; installed DLL SHA-256 `273E7C36…` matches the build.
**Next step:** the next two-player session; copy the log right after anything odd.
`docs/multiplayer.md#diagnosing-a-multiplayer-problem-2026-09-26`. Launch research for a
future release: `docs/release-launch.md`.

## Loop sounds embedded in the DLL — installed, 2026-09-26

User request: players must not be able to swap the loot/exploration sounds. The 20 WAVs are
now RCDATA resources in `d2rl-d2r-access.dll` (`FOLDER_STEM` names, generated
`build/item-sounds.rc`) and `ItemWave::LoadEmbedded` reads them from the plugin's own module;
nothing is installed under `plugins/d2r-access/sounds/` any more (removed from the install).
DLL 1.4 → 13.4 MB. The item-audio suite opens the built DLL and checks all 20. **79/79 suites
pass** (same install as above). **Next step:** at the next launch
the log should say `[item-audio] 20 sounds loaded`; the next friend package drops the `sounds`
folder and tells them to delete the old one. `docs/ground-items.md#assets-and-build`.

## Friend's tester ZIP v2 — 2026-09-25

`../d2r-access-completo-2026-09-25-v2.zip`, plugin from 98df013 (SHA-256 `D2DC8ABA…`, matches
the install): 46 entries, 57,713,847 bytes, SHA-256 `31fc1e45…` (full hash in `.zip.sha256`
beside it). Built on the first 25/09 package: new plugin, current `radar.toml` as
`d2r-access.toml` (adds `[doors]`, `[discoveries]`, `[weapon_swap]`, `[trade]`), the 20 WAVs in
the `loot/` and `exploration/` layout, and a Spanish LEEME with this version's news (doors,
corpses, scroll/rune/gem, tomes, trade, stash gold, new exits, weapon swap) and a note that the
old sound folders can be deleted. Loader and PRISM match the install; VC runtime unchanged. Every
entry decompresses and matches its SHA-256. **Next step:** the friend installs it.

## Sound folders, corpse/scroll/rune/gem loops, door loops — installed, 2026-09-25

- **Folders:** sounds now live in `sounds/item loop sounds/loot|exploration/` (portal moved to
  exploration) and install at the same paths; `GroundItems::SoundFiles` carries each folder.
- **Loot:** corpse containers (25 classes, `Generated::CorpseObject`) loop `corpse`; scrolls,
  runes and gems (ItemTypes `scro`/`rune`/`gem `) their own loops. The four short clicks get
  +24 dB and a limiter at build time (user's choice) to match the other loops.
- **Quest tomes:** the Moldy Tome and Horazon's journal loop `scroll` until read
  (`Generated::TomeObject`, OperateFn 6/42).
- **Doors:** shut doors loop wooden/metal/stone/slime by the game's own opening sound, read
  from D2R.exe's per-class object sound table (`tools/gen_door_sounds.py`); `[doors]`,
  `access-items doors-on|off`. `docs/ground-items.md#door-loops`.

**79/79 suites pass**; installed DLL SHA-256 `D2DC8ABA…` matches the build; stale WAVs of the
old layout removed from the install. **Next step:** live check the Moldy Tome (Black Marsh)
before and after reading it, a shut and an opened door
(Act 1 Barracks/Jail, Act 2 tombs), a dead rogue in the Blood Moor, a scroll/rune/gem on the
floor; `access-items chests-dump` logs `corpse=` and `shut_door=` per object.

## Trade, stash gold, new exits, weapon swap — installed, 2026-09-25

Four requests from the user:
1. **Trade**: request prompts name the partner ("canaria quiere comerciar contigo"), the grids read
   "Comercio con canaria. Tu oferta" / "Oferta de canaria", the partner's items, gold and
   acceptance are told, so is this player's acceptance, and the end says completed or
   cancelled (hook on the S2C 0x2E "completed" reply). The busy notice gets its missing space.
   `docs/trade.md`.
2. **Stash gold**: the stash's withdraw button says "12345 de oro en el alijo" (the gold box
   cannot take focus). `docs/native-tooltips.md#stash-gold-2026-09-25`.
3. **New exit**: "El radar ha registrado una salida nueva: Cementerio" the first time the
   scanner lists an exit on a map (waypoint/stash: "un lugar nuevo"); not for restored places
   or the exit back. `docs/exploration.md#speech`.
4. **Weapon swap**: "Armas 2: Arco largo, Carcaj" from the game's own set flag. `docs/weapon-swap.md`.

New settings sections `[discoveries]`, `[weapon_swap]`, `[trade]`. **79/79 suites pass**; installed DLL SHA-256 `CA5E33B9…` matches the build.
**Next step:** live checks — a two-client trade (prompts, grids, partner offer, both endings),
the stash withdraw button on a personal and a shared tab (`[stash] gold` log line), a new
exit in an unexplored area, and swaps with full, half-empty and empty sets.

## Skill synergies — not a bug, live check pending, 2026-09-25

Resist Fire's tooltip has no synergy section in the game itself (`skilldesc.txt`
`dsc3` empty); synergies are drawn on the receiving skill, e.g. Holy Fire. The
skill reader keeps every tooltip line. No code change. **Next step:** read
Fuego sagrado or Purificación with the detail gesture and confirm "recibe bonus
de" is spoken. `docs/progression.md`.

## Friend's tester ZIP — 2026-09-25

`../d2r-access-completo-2026-09-25.zip`, plugin from 501dd67 (SHA-256
`b9be7e00…`, matches the install): 38 entries, 56,628,533 bytes, SHA-256
`c21cb1f1…` (full hash in `.zip.sha256` beside it). v4 package with the new
plugin, `radar.toml` as `d2r-access.toml` (adds `[radar] diagonals`) and a
Spanish LEEME that leads with the control change (guidance on L3, R3 back to
the game, Ctrl+V / Ctrl+O) and fixes the old R3 references. Loader, PRISM,
WAVs and VC runtime unchanged. Every entry decompresses and matches its
SHA-256. **77/77 suites pass.** **Next step:** the friend installs it.

## Hold LT to hear the LT skill bar — works live, 2026-09-25

User asked what LT/L2, RT, LB and RB do (stock map now in the doc) and chose
this feedback: holding LT alone 0.6 s in play reads the six LT-bar slots
("A: Tomo de portal a la ciudad, 12. X: Vacío…"), release cuts it, combos stay
silent. Bindings come from the game's table via its getter `0x117D20`, names
from skills/skilldesc rows, counts from the HUD's `QuantityText`.
**77/77 suites pass**; installed DLL SHA-256 `561EE346…` matches the build.
**Live 2026-09-25:** the user confirms it works. **Next step:** none; still
unchecked are a weapon swap with separate bindings and a charged-item skill.
`docs/skill-bar.md`.

## Radar back on the stick cardinals — installed, 2026-09-25

The user tried rays along the stick diagonals (walls run that way, so a corridor
reads open along its length) and preferred the cardinals: with every ray panned to
a side, walls and exits up and down at once were an overload of cues. Default
`[radar] diagonals = false`; `access-radar diagonal` still tries the other frame.
Known limit kept: a corridor along a world axis can read as wall in all four
directions. **77/77 suites pass.** **Next step:** live listening of the precision
fixes below; an exit passed in line should tin once.
`docs/decisions.md`, `docs/radar-readings.md`.

## Radar precision review against the Among Us sonar — installed, 2026-09-25

User: mostly right, sometimes imprecise, silent on very open ground. Four gaps
fixed, each with a test that fails without it: (1) lone stones/trees/posts were
erased as clutter while wall-slide prevention stops the character at them (Blood
Moor capture: 102 readings said open with an obstacle inside the warning
distance) -- the filter now runs only while the game slides; (2) a gap passed
within 500 ms of its wall cue was lost (interval counted from the wall) --
counted between openings now; (3) a reading back to the last cue repeated;
(4) the way-back mark never lapsed. Holding the retreat was tried and reverted
(phantom breaths along D2's 45-degree walls). **76/76 suites pass.** **Next
step:** live listening in Blood Moor/Cold Plains: stones and trees ahead should
tock, and walking along a wall close by should breathe at each gap even when
running. `docs/radar-readings.md#precision-review-against-the-among-us-sonar-2026-09-25`.

## Magic names with only a suffix read in order — installed, 2026-09-25

In esES a magic item with a suffix and no prefix was read "de llamas [fs]Maza":
the game's own esES `ItemNameMagicFormatSuffixOnly` is `a0n1:%1 %0` (esMX has
`a1n0:%0 %1`). The item and comparison tooltips now rebuild it from that format:
"Maza de llamas". An audit of all 13 locales found no other broken format.
**76/76 suites pass.** **Next step:** live check on a suffix-only magic item in
esES. `docs/native-tooltips.md#magic-names-with-only-a-suffix-in-eses-2026-09-25`.

## Beacon keeps its route in combat and at area borders — installed, 2026-09-25

The user got lost on the way from Dark Wood to Black Marsh: three
`known-route-unavailable` releases, then `no-route` refusals. Three route
fixes, then two the user asked for next:
1. Room sub-goals are chosen on the terrain layer. Before, a monster in a
   doorway broke the room route mid-fight (`docs/room-routing.md`).
2. An outdoor border is routed to its real walkable crossing, when that
   crossing is loaded. Before, the target was the border's midpoint, which is
   often inside the tree line (`docs/beacon.md#area-borders`).
3. Exploration mode releases only after `routeLossMs` (3 s) without a proved
   route. Meanwhile the ping is silent.
4. The scanner measures a border to that same crossing (distance and direction).
5. Remembered places read in the language being played. `explored.dat` is now
   `D2REXP03` and stores each name's game string key (`docs/exploration.md#on-disk`).
   Maps were already language-independent.
6. A text-language switch in the menus, without restarting, is followed: speech
   re-detects it (`SetTextLanguage`), and the monster placeholder label is read
   again (`PlaceholderName`), and so are the item tooltip's stat, price and
   requirement labels (`interaction_reader.cpp` `Labels`; interaction-reader
   test switches to English). No live switch has been tested yet.

The missing Underground Passage entrance is not a bug. The Dark Wood map was
the same (seed `6DFC5AA1`), and its 69 visited rooms loaded. But
`explored.dat` holds only the seven places found in this session, so the
dungeon was explored before places were saved. The entrance will be listed
once it is seen again. **76/76 suites pass.** **Live 2026-09-25:** the user
reports the route now holds; items 1–4 are committed. **Next step:** switch the
game to English and check the scanner names remembered places in English
(items 5–6, uncommitted).

## Unmet requirements and refused equips — installed, 2026-09-25

Red requirement lines (strength, dexterity, level, class) are said with the
item's name ("No se puede equipar: (Solo asesina). Destreza necesaria: 33"); an A
press on an equipment slot that leaves the same item in hand says "No se puede
equipar" and the reasons. First version failed live (it looked for `ÿc1`); the
`[item-markup]` log showed the real marking, style command `U+E07E` + palette `1`.
**76/76 suites pass**; installed DLL SHA-256 `2A4FC82D…` matches the build.
**Next step:** live check on the claws / "Libro viejo". `docs/native-tooltips.md`.

## R3 removed, equipped items, NPC speech on audio only — installed, 2026-09-25

R3 and the L3+R3 chord have no mod gesture (the exit review stays on Ctrl+V, the
terrain overview on Ctrl+O); L3 is the only stick button. The R3 pad filter is
removed, so R3 is the game's again (weapon swap / emote wheel). An item that starts being worn is announced with slot
and name (`docs/vitals.md`). With the game's NPC Speech option on Audio Only,
dialogue text is not read (`docs/test-ingame-text.md`). **75/75 suites pass**
(stick-chord and pad-filter suites removed with their modules). **Next step:** live check:
equip by hand and with the controller action; NPC Speech on Audio Only.

## Five UI fixes from the user's play session — installed, 2026-09-25

1. Create-game text boxes: the box being typed into is named (the game's
   active editor, not the focus a form refresh moved), and a box is one phrase:
   label, "cuadro de edición", contents. `docs/multiplayer.md`.
2. Scanner readout: eight screen directions ("arriba y a la izquierda").
   `docs/entity-scan.md`.
3. Beacon on **L3** (R3 opens the emote wheel); L3 on a newly scanned target
   switches in one press, on the same target cancels. `docs/beacon.md`.
4. Radar cues louder, breath most; the downward breath almost an octave darker.
   `docs/radar-readings.md`.
5. Focus: the stats cursor survives focus on the menu tab strip; a panel that
   draws nothing (the closed emote wheel) no longer holds focus and mutes world
   speech. `docs/character-panel.md`, `docs/native-ui.md`.

**77/77 suites pass**; installed DLL SHA-256 `BD1F669C…` matched that build.
**Next step:** live check of all five; for (1) log `[field]`/`[focus]` while
pressing A on the password box; confirm L3 does nothing in the game itself.

## Exits, waypoints and the stash found on a map stay listed in later games on that map — installed, 2026-09-24

User asked for already discovered exits (maps and dungeons) to be readable from
afar, only on the same map; a regenerated map shows none until found again.
Then: waypoints too, and anything fixed, but not objects, which change. Kept:
exits, waypoints, the stash (`MapSurvey::Fixed`); waypoints and the stash now
carry a place identity (`Entity::placeId`, renamed from `levelExitId`).
The scanner already listed them from anywhere within one game. A new game
depended on the game having their rooms' data again (a room not yet generated
this game has no presets, `docs/distant-exits.md`; not measured live). Discovered fixed
places now go into `explored.dat` (format `D2REXP02`, old archives load) under
the map's seed/level/layout key, and the scanner lists them from there. Also
fixed: an area border to the east or south fell just outside the level's
rooms, so it never counted as charted (the Cold Plains never listed the Blood
Moor or the Burial Grounds). **77/77 suites pass**; installed DLL SHA-256
`9C851034…` matches the build. **Next step:**
find an exit and a waypoint, start a new game on the same character, check the
log's `[terrain] level=… places=N` and that the scanner lists both at once from afar.
`docs/exploration.md#on-disk`, `docs/entity-scan.md`.

## Player corpses from anywhere on the level — installed, 2026-09-24

User asked for a detector of players' corpses, like the player one, visible
from any distance. New right-stick category **corpse** (`cadáver`) after
players: "your corpse" and "X's corpse" (name from the corpse's PlayerData).
A corpse read nearby is remembered per level for the whole game. A party
member's corpse is placed even unseen, from the roster at the moment it
appears. Pickup removes it at once. New static RE: roster life +0x4C, corpse
ids +0x78, corpse created at the end of the death animation.
**77/77 suites pass**; installed DLL SHA-256 `9502F540…` matches the build.
The new `roster-corpses` fingerprints match every captured image, not yet the
live game. **Next step:** in a party, die far from the friend, check that
their scanner lists the corpse at once, then retrieve it.
`docs/player-corpse-re.md`, `docs/entity-scan.md`.

## Routes to unexplored rooms in dungeons — installed, 2026-09-24

User reported the pathfinder struggling to reach unexplored rooms in dungeons.
The live log showed room routes through rooms that meet only at a corner. The
beacon waited at a dead end for 30 s, then released
(`known-route-unavailable`). Routing now links only rooms sharing an edge that
collision does not prove walled off. It heads for the doorway of the nearest
reachable room, and to a room's edge only while that room is unloaded. The
unexplored-room list uses the same rule. **77/77 suites pass**; installed
(same DLL as above). **Next step:** in a cave, follow several unexplored rooms;
`[beacon] hop=` should advance room by room. `docs/room-routing.md`.

## Unexplored rooms in the scanner — installed, 2026-09-24

User: keep the radar; list unexplored rooms even when unloaded, never their
exits. New right-stick category **unexplored room** (`sala inexplorada`),
right after the ways out. In exploration mode it lists only the rooms bordering
explored ground (stood in, drawn on the automap, or underfoot), loaded or not.
The beacon may enter such a room and arrives on entry. **76/76 suites pass**;
installed DLL SHA-256 `CC84620F…` matches the build and is in the v4 ZIP
(see the entry below). **Next step:** get lost on
purpose, follow the nearest unexplored room, and check that its exits appear once
charted. `docs/entity-scan.md`, `docs/beacon.md#exploration-mode`.

## Other players in the scanner — installed, 2026-09-24

User, playing over LAN: show the other players on the right stick, from far
away when both are on the same map. New live **player** category (`jugador`)
between NPCs and enemies, followed by the beacon.
- **Nearby:** loaded units, named from `PlayerData+0` (`PlayerChecks`).
- **Far, same level:** party members from the client party roster, the same
  data the automap uses for party markers (`RosterChecks`,
  `docs/entity-units-re.md` §11–12).

Every fingerprint matches three captured images. **76/76 suites pass.** The
installed DLL hash matches the build. No game launch has verified the checks
yet. **Next step:** play a two-client game in a party; check
`[scan] Verified party roster reader.` and the category near and far.
Friend's package: `../d2r-access-completo-2026-09-24-v4.zip`, plugin from
2fbc9c9 (players and unexplored rooms), SHA-256 `c393c764…` (full hash in
`.sha256`). Same 38 files as the first ZIP of the day; only the plugin and the
LEEME changed. Every entry verified; the DLL matches the install. v2 and v3
are superseded. `docs/entity-scan.md`.

## Complete tester ZIP — 2026-09-24

`../d2r-access-completo-2026-09-24.zip`, plugin from 7df3311: 38 files,
56,591,584 bytes, SHA-256 `bd0bdb0d…` (full hash in `.sha256` beside it).
Previous package plus the new plugin, `quest item.wav`, the current loader
config and the repo's `radar.toml` as `d2r-access.toml` (adds `[navigation]`).
Spanish LEEME lists what is new. Every entry decompresses and matches its
SHA-256; the DLL matches the installed build. **76/76 suites pass.** **Next
step:** the friend installs it and a two-client session is tested.

## Scanner follows the automap — installed, 2026-09-24

Exploration now counts a room as explored when the stock automap has drawn in it
(saved with the character) or the player stood in it. The scanner lists
landmarks there, and the beacon routes through them. Large objects such as the
Cairn Stones are now lockable (footprint-aware arrival); refusals log
`[beacon] refused reason=`. **76/76 suites pass**, clean rebuild installed and
hash-verified. **Next step:** play a new area and check that the scanner lists
charted exits after a reload. `docs/entity-scan.md`, `docs/beacon.md`.

**Build fix:** under the OEM code page, cl prints the Spanish `/showIncludes`
prefix with a different byte for "ó" than the UTF-8 prefix in `rules.ninja`.
Ninja then recorded no headers, and a header-only change linked stale objects.
`build.ps1` now runs `chcp 65001` first; `VSLANG=1033` has no effect without
the English pack. Any other build command must run under code page 65001 too.

## Native automap reader — installed, awaiting live validation, 2026-09-24

Static RE found the current layer, its four cell trees and the reveal rules
(tiles and objects once drawn on screen). A read-only reader and diagnostics
are installed: `access-automap status|dump|watch`, Ctrl+J toggles the watch. No
scanner change yet. **76/76 suites pass**; installed DLL hash verified. **Next
step:** live route A–G with the watch on, then wire the scanner to automap
cells. Live sessions: A–G pass. Stones decode within about 1 subtile; new terrain is saved and
revealed while the automap is hidden; object glyphs are session-only, re-added when seen. `docs/automap-reader.md`.

## One scanner on the right stick — installed, 2026-09-24

User asked the scanner to follow the automap without L3. The right stick now
always drives the catalogue: up/down categories, left/right objects. Enemies are
a live nearest-first category; ground loot and ordinary chests left the scanner.
The directional scan, `access-scan` and Ctrl+E are gone. Discovery still
approximates the automap (range plus sight); a native automap reader is being
researched. **75/75 suites pass**; user confirmed the controls work. **Next
step:** see the automap reader entry above.
`docs/entity-scan.md`.

## Ground item speech on every pass — installed, 2026-09-24

User reported names read only once and too late. Stepping beyond 4 subtiles now
rearms a name (was 6); a newly reached item speaks at once. Grouped names read
quest items, consumables, gold, then equipment. Installed in the combined build
with the pathfinder mode: **75/75 suites pass**, installed DLL hash matches the
build. The user confirmed it works on 2026-09-24. **Next step:** none for this
fix. `docs/ground-items.md`.

## Quest landmarks on automap — investigated, 2026-09-24

D2R visuals and local tables confirm Cairn, Inifuss and Malus map glyphs;
tables identify further quest landmarks. Exact discovery timing remains
unverified, as does a native reveal-state reader. Research only; no gameplay
changes. **Next step:** compare Cairn glyph visibility before/after approach and
room unloading. [Evidence and limits](docs/automap-quest-markers.md).

## Pathfinder exploration mode — installed, 2026-09-24

User felt the pathfinder led by the hand. Default is now exploration: L3 lists
only discovered things and visited rooms, by straight line and direction; R3 /
Ctrl+B routes only through visited rooms, else says so and stays silent. Fixed
landmarks survive a return to the level. Full assistance is opt-in
(`[navigation] full_assistance`, `access-beacon full`). **75/75 suites pass**;
installed DLL hash verified. **Next step:** play in the default mode and judge
whether discovery (scan range plus sight) is enough. `docs/beacon.md#exploration-mode`.

## Cairn scan visibility — installed, 2026-09-24

User confirmed the pathfinder listed the symbols and let them activate the stones,
but the short-range scan omitted them. Sight now excludes the target's own OBJECT
collision within its native footprint, preserving walls, doors, range and sectors.
Five native guards and all 587 table rows verified. **74/74 suites pass**, no build
warnings; installed DLL hash matches. **Next step:** check nearby stones with the
right-stick scan, including activated stones. `docs/cairn-stones-re.md`.

## Cairn puzzle symbols — installed, 2026-09-24

User explicitly wants freedom to choose incorrectly. All neutral stones qualify
for the cue; scan/catalogue/beacon name their fixed symbols. Opening the deciphered
scroll reads its sequence once; reopening repeats it. Automatic next-stone selection
and solution ranks were removed. **74/74 suites pass**, no compiler warnings;
deployed DLL SHA-256 matches. **Next step:** controller/NVDA check of symbols and
scroll reading. `docs/cairn-stones-re.md`, `docs/ground-items.md`.

## Quest item spatial sound — 2026-09-23

User confirmed the tree cue works. Built and installed the requested faster,
farther revision: 700 ms repetition, 45-subtile reach with gentler attenuation,
plus Cairn Stones only while unactivated. Ground quest items and the nine
item-granting interactions retain their priority and Ctrl+L toggle.
**71/71 suites pass**, no compiler warnings; installed DLL and all WAV hashes
verified. Other audio assets match their original conversion. **Next step:**
listen at greater distance and activate the Cairn Stones. Nearby loaded rooms
only; no sequence hints or automatic interaction. `docs/ground-items.md`.

## Mercenary statistics, life and level — 2026-09-23

Built and installed: advanced-stat rows for player and mercenary, corrected
native mercenary life percentage, matching player alert steps/priority, and
level-change speech. Simultaneous vitals use one utterance; lifecycle events
reset baselines. **71/71 suites pass**, no compiler warnings, installed DLL hash
verified. The percentage getter and its panel consumer were verified against
the live game before closing. **Next step:** controller/NVDA listening in the
advanced panel and during mercenary damage, recovery and a level gain.
`docs/character-panel.md`, `docs/vitals.md`, `docs/combat-status-evidence.md`.

## Multiplayer accessibility — 2026-09-23

Built and installed: native host/browser/direct-join speech, checkbox states,
input labels, status changes and one-control-per-press D-pad navigation.
**71/71 suites passed**, then 6/6 affected suites after focus-retention and
icon-label fixes; installed DLL hash verified. User confirmed navigation,
activation, retained checkbox focus and state speech on 2026-09-23. Live logs
also verified direct-join controls. Normal is enabled; Nightmare/Hell are locked
for the selected character, as reported by the native game state.
**Next step:** play with the friend; public list selection and a two-player
internet session remain unverified.
Local hosting reports Issues (automatic port setup unavailable). No network
settings changed. Setup and native evidence: `docs/multiplayer.md`.

## Complete tester ZIP — 2026-09-23

Updated copy: `../d2r-access-completo-2026-09-23-actualizado.zip`
(37 files, 56,499,484 bytes), with the confirmed multiplayer plugin, two new
configuration sections and Spanish multiplayer instructions. Loader 1.3.0-beta,
PRISM, 11 WAVs, four x64 VC runtime DLLs and licenses retained.
Verified decompression and SHA-256 for every entry; DLL matches the installed
build. **71/71 suites passed, then 6/6 affected suites** after final UI fixes.
Original ZIP retained after overwrite was blocked by automatic review.
**Next step:** send the updated ZIP; recipient installs beside `D2R.exe` and
starts `D2RLoader.exe`. Two-client audio/connectivity still needs live testing.

## Automap discovery investigation — 2026-09-23

Research only; existing navigation retained. No verified native reveal-state
reader was found. Streamed/visited rooms are not a proven automap visibility mask;
the catalogue and room graph can use undiscovered geometry. Two saved `.ma0`
containers were structurally checked, without modifying saves or the plugin.
**Next step if resumed:** trace the native automap consumer and compare discovery
against the live map before adding a strict mode. The user accepts keeping the
current guidance if that cannot be verified. `docs/automap-discovery-research.md`.

## Playability fixes and inventory potion assignments — 2026-09-23

Built and installed; **67/67 CTest suites pass**, no compiler warnings, deployed
DLL hash verified. Inventory belt focus reads the native D-pad binding, row,
potion and physical column count, distinguishing automatic inventory potions.
View/Back keeps its existing behavior; the planned settings menu is separate.
Life/mana alerts combine with life first; partial orb-text failures are reported;
empty beacon room graphs retry. Added own-corpse scan/beacon targets, poison
transitions, mercenary health and guarded quest-packet objective capture.
Live startup with NVDA verified the corpse, poison, mercenary and quest guards.
**Next step:** controller listening in inventory and live combat/quest checks;
corpse discovery covers loaded rooms only. Freeze/other effects and a complete
campaign playthrough remain unverified. `docs/belt.md`, `docs/vitals.md`,
`docs/player-corpse-re.md`, `docs/quests.md`.

## Interactive exit sound restored — 2026-09-23

Built and installed: dungeon stairs, cave entrances and other interactive exits
use `portal` again; only walking crossings use `zone transition`. Coincident
warp/boundary cues stay deduplicated. **4/4 affected suites pass**, no compiler
warnings, installed DLL hash verified. **Next step:** listen at a dungeon
entrance and a walking zone border. `docs/level-exits.md`, `docs/exploration.md`.

## Quest journal identity and controller actions — 2026-09-22

Cain's populated journal pane was announced as empty because HD icon numbers
were incorrectly used as quest string IDs. The catalog now uses classic layout
IDs, correcting three Act I and four Act III entries. Quest descriptions include
the current controller action or visible speech caption, without duplicated tab
labels. Built and installed; **67/67 CTest suites pass**, DLL hash verified.
The user confirmed the journal correction works on 2026-09-22.
**Next step:** none for this fix; unloaded-journal coverage remains separate.
Evidence: `docs/quests.md`.

## NPC and priority enemy cues — 2026-09-22

The user found the noise whoosh too similar to the radar and repetition too slow.
Installed: voiced metallic enemy swoop, unchanged wooden NPC tap, 150–450 ms
intervals and 100 ms shared spacing (isolated targets about 200–500 ms in play).
**4/4 affected suites pass**, including a comparison against the radar synthesis;
no build warnings, installed DLL hash verified. Original feature passed 67/67.
**Next step:** user listening for separation and faster pacing. `docs/entity-cues.md`.

## Supplied exploration cues — 2026-09-22

Built and installed the nearest-unvisited-room loop, finite first-entry cue,
and exit loops (interactive exits restored to portal on 2026-09-23; zone audio
only at walking crossings). Persistent
visited memory suppresses return visits. **64/64 CTest suites pass**, plus 9/9
affected suites after the final duplicate-exit fix; DLL and three WAV hashes
verified. **Next step:** listen in a dungeon and at the camp's Blood Moor gate.
`docs/exploration.md`.

## Radar sounds, outdoor exits and compact exploration storage — 2026-09-22

Lowered the floor breath's filter from 880 to 700 Hz, with level compensation.
The radar now includes outdoor area-border exits in its local passage scan.
The unexplored tail uses two short pips to distinguish it from the beacon.
Exploration uses one archive capped at 4 MiB, with disk I/O on a worker.
Built and installed; **61/61 CTest suites pass**, installed DLL hash verified.
Migrated 9 text files to one 2,846-byte archive; all 286 seen and 176 visited
room records match the originals. **Next step:** compare the three sounds and
approach the camp's Blood Moor exit. `docs/radar-readings.md`, `docs/exploration.md`.

## Equipment placement speech — 2026-09-22

Built and installed; **4/4 related CTest suites pass**. Temporary focus gaps no
longer rearm the pickup instruction. While carrying an item, occupied equipment
destinations read without waiting for an item tooltip. Confirmed releases outside
inventory clear held state silently. The user reports that placement works well
on 2026-09-22. **Next step:** listen to the corrected mercenary skill announcement.
Evidence: `docs/inventory-npc-followup.md`.

## Hiring-row tooltips and actions — 2026-09-22

Live hiring logs confirm three rendered skills per Act I candidate, with correct
fire/cold variants. Removing secondary tooltip records then caused an automatic
first-skill repeat that interrupted the complete description. Removal-only changes
are now silent; new or changed records still announce. Built and installed;
**3/3 related CTest suites pass**. **Next step:** controller listening to the full
skill description; mercenary equipment remains pending.
Evidence: `docs/character-panel.md#hiring-rows-2026-09-22`.

## Menu and item-comparison fixes — 2026-09-22

Built and installed; **61/61 CTest suites pass**. Character selection
reads the selected runtime row, even when native focus stays on the first.
Focusless inventories no longer narrate all labels
and controller glyphs. Mercenary stat rows share the character-stat reader.
Comparison capture follows the records the game actually draws; activating it
reads selected and equipped item statistics, with right-stick-up repeat.
Live startup loaded both verified tooltip hooks. Keyboard selection now announces
Joline after yarkey (14:20:32 log), matching the on-screen selection; controller
listening is still pending.
**Next step:** controller listening on these three paths, including mercenary
equipment with an available hireling. Evidence: `docs/native-tooltips.md`.

## Concise item announcements — 2026-09-22

Items use one name plus quality (`Crimson Spear. magic`) or unidentified status
(`Spear. Unidentified`). Generated names wait for the stock tooltip; prices and
properties remain in detail. User reported the stock 400 ms wait was noticeably sluggish;
it is now bypassed during active inventory reads. Built and installed; **61/61 suites pass**.
Live startup confirmed the verified delay hook loads; clean exit succeeded.
The user confirmed on 2026-09-22 that the faster reading works very well.
**Next step:** preserve this responsiveness while fixing menu and comparison coverage.
`docs/native-tooltips.md`.

## Directional wall sliding — 2026-09-22

Built and installed local directional fallback prevention: the original direction
retains its native probe; two 45-degree retries and the eight-way fallback receive
zero travel. Default on; `access-movement on|off|status`. All **61/61 suites pass**,
including 50 new movement checks; installed DLL hash matches the build.
**Next step:** user-led controller wall/doorway test, including sustained blocked
input and moving away. The earlier inert prototype is replaced. `docs/wall-sliding.md`.

## Audio and language verification — 2026-09-22

Cue classification/playback is independent of text/voice locale. Fixed missing
localized names suppressing container/object-exit cues, and beacon audio continuing
through a blocked-UI transition. Generic fallback labels cover all 13 locales.
Build/deploy succeeded; **59/59 CTest suites pass** in the current shared workspace.
Installed plugin/PRISM/eight WAVs verified. Loader remains `enUS` for text/voice.
**Next step:** live listening after a locale change and during beacon dialogue/pause.
Evidence and limits: `docs/audio-language-audit.md`.

## Unexplored mark, two memories, disk persistence — 2026-09-21

The radar's exit and floor cues carry an **unexplored tail** (now two short pips; updated 2026-09-22) when
the way leads to ground never covered: into a chamber never stood in (maze levels), to a level never
seen (any level exit), or towards the frontier of the streamed ground (outdoor, `src/frontier.*`).
A qualifier on the reading, never a new state; a qualifier-only change confirms like a state change.
`ExploredRooms::Memory` keeps **seen** and **visited** rings, originally in per-level text files
(replaced by the compact archive described above; `src/explored_store.*`), loaded when the
seed is readable, saved on lifecycle events and every 30 s. Exit review and room line tag each way
out never taken and list those first; Ctrl+O says `explorado N por ciento`; Ctrl+T says `sin
explorar`; `access-radar test` plays the marked exit and floor. 58/58 CTest suites.
`docs/exploration.md`. RE of the same day (in that doc): `WarpDist` is monster-pack placement, not
level generation; outdoor level rects are fixed at act creation; no explored-rooms counter in the game.

**Live-verified 2026-09-21 (unattended, camp, enUS locale):** store file created and filled (35 rooms,
4 visited after ten steps), `[terrain] visited room=` per new room, no grid-origin warning, Ctrl+O
`Rogue Encampment, centre, 100 percent explored…`, clean exit and reload. Not exercised: the audition
(console command, the harness cannot type `` ` `` or `-`), any `unexplored=1` cue (town floor is never
marked and Blood Moor is out of radar reach from the camp), the Spanish wording. Found on the way and
fixed: the exit review had lost the camp → Blood Moor border since the `bladestorm` merge (level-border
bearings became a separate opt-in that `RoomService::Exits` never passed); it asks for them again.

**Next step:** listening pass. Den of Evil: enter, expect the entry line to tag the openings not yet
taken; face one, expect tin + tail; walk through and back out, expect the same opening plain. Camp:
face the Blood Moor exit before ever going there, expect tin + tail; after visiting, plain. Blood Moor:
walk into fresh ground, expect breath + tail ahead and to the sides, plain behind; watch
`[terrain] … frontier_ms=` (should stay under a few ms) and the grid-origin warning (must not appear).
Quit and relaunch: `[terrain] level=… loaded … seen=N visited=M` must show the previous counts.

## The inventory names its item at once — 2026-09-21

Grid cells, equipment slots and the belt all name the item from the unit under the highlight
(`NativeUi::FocusedItem`, one getter for the three archetypes), with its quality, instead of waiting for
the controller tooltip. The wait was the layout's: `Controller/TooltipsPanelHD.json` sets
`showDelayMilliseconds` to 400, measured as 460-550 ms from highlight to name (the belt already read its
unit: ~60 ms). A slot says `Ocupado` only when its unit cannot be named, and the tooltip that follows
never interrupts — it queues only what it adds, or the name only the game knows (magic, rare, unique).
`[item]` is logged once per item instead of on every UI poll (it was 93% of the lines in a 10 s inventory
window). 57/57 CTest suites. `docs/native-tooltips.md`.

**Next step:** listening check with the controller. Walk the grid, the equipment slots and the belt: the
name must land with the highlight, nothing may be cut off mid-word, and a magic/rare item must correct
itself without saying its name twice.

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
or Ctrl+V speaks `abertura, 8 metros, a las 12, recto, 1 de 3` (a room opening) or the exit's own name; a
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

- **Engine / framework:** native C++ x64 (D2R.exe, patch 3.3) / **D2RLoader** (closed-source loader, **v1.3.1-beta 2026-09-24**, https://d2rloader.net) + its open **PluginSDK** (C++20 header-only, MIT, cloned at `../external/PluginSDK`, pinned at `6fbabfc8` / SDK 0.2.0, plugin ABI v4).
- **Screen-reader transport:** plugin DLL → `LoadLibraryW(<pluginDirectory>\prism.dll)` + `GetProcAddress` (load in `D2RLoaderLoadPlugin`, never in `DllMain`).
- **Build command:** `.\build.ps1` (CMake 4.2 + Ninja + MSVC Build Tools 2022 14.44; vcvars64 from `C:\Program Files (x86)\Microsoft Visual Studio2\BuildTools`). POST_BUILD copies `d2rl-d2r-access.dll` + `prism.dll` into the game. PRISM v0.18.2 vendored in `third_party/prism/`.
- **Mod install path:** `D:\games\steam\steamapps\common\Diablo II Resurrected\d2rloader\plugins\` (global) — D2RLoader zip extracted into the game folder, launch `D2RLoader.exe`.
  The loader is **three** files in the game root, all dated 2026-09-14: `D2RLoader.exe` (23,590,248), **`D2RCore.dll`** (18,345,304 — resolves the game pointers the widget and world readers depend on) and `d2rloader.mpq` (10,816,549). Both builds we have are kept outside the repo in `../loader-backups/` with their hashes and restore notes; **1.2.1-beta can no longer be downloaded**, that copy is the only one. Everything else in the root is the game's. Packaging the mod for someone else needs all three, plus `d2rloader/plugins/d2rl-d2r-access.dll`, `d2rloader/plugins/d2r-access/{prism.dll, sounds/item loop sounds/*.wav}` and a `d2rloader/config/d2rloader.toml` with the console enabled. Never ship `D2R.text.bin` (dumped game code, regenerated on first run).
- **Run / test:** Steam D2R 3.3.93787 (app 2536520, Spanish) installed. D2RLoader 1.3.1-beta installed into the game folder 2026-09-26 (SHA-256 verified; 1.3.0-beta from 2026-09-17 kept in `../loader-backups/`). Start NVDA, launch `D2RLoader.exe` (Steam's Play button also does it: launch options `"<game>\D2RLoader.exe" %command%`, set 2026-09-25; see README). Loader config (`d2rloader/config/d2rloader.toml`): `enable_console = true` (Ctrl+` in-game), developer logs on with `widget_panels` + `extension_details`. Logs: `d2rloader/logs/d2rloader.log`. Offline/single-player only (online ban risk undocumented).

## Section status

| Section / feature | Status | Notes |
|---|---|---|
| D2RLoader multiplayer | done — controller confirmed 2026-09-23 | Native form speech, D-pad navigation and retained checkbox focus; 71 suites pass. Internet session unverified. `docs/multiplayer.md` |
| Complete tester ZIP | updated — 2026-09-23 | Use `-actualizado.zip`: multiplayer fixes and instructions, 37 entries verified; recipient startup pending. |
| NPC / priority enemy sounds | faster resonant revision installed; listening pending | Wooden NPC tap; voiced enemy swoop. 150–450 ms intervals; 4 suites pass. `docs/entity-cues.md` |
| Exploration sound assets | built/deployed — listening pending | One nearest room loop; arrival once per new room; zone loops at real crossings. 64 suites pass. `docs/exploration.md` |
| Directional wall sliding | wip — built/deployed; controller check pending | Local type-17 fallback probes use zero travel. 50 new checks pass. `access-movement on\|off\|status`; `docs/wall-sliding.md` |
| Adapter + Speech (PRISM) | done | NVDA backend; all PRISM calls on a dedicated speech thread (`screen_reader.cpp`), `Speak` only queues. Console `access-say` |
| Mod localization (13 locales) | built and installed - live check pending | Locale headers, game-string language detection, beacon/quest messages and missing-objective text; 5763 checks pass. |
| Audio cues across locales | fixes built and installed; listening pending | Numeric cue classification, generic container/exit fallback in all 13 locales, beacon UI cancellation. 59/59 suites pass in the shared workspace; `docs/audio-language-audit.md`. |
| Quest log / automatic quest tracking | journal user-confirmed; native packet capture installed 2026-09-23 | Guarded native objective/counter capture can cover unloaded journals; ambiguous or missing captures keep the generic alert. Startup verified, live update coverage pending. 67/67 suites pass. `docs/quests.md` |
| Room exits (maze levels) | built 2026-09-20, listening check pending | Ways out of the native room the player walks into: screen directions plus named stairs/portals. Maze levels only, from `src/generated/level_flags.h`. Streamed rooms only — an unloaded neighbour is not claimed. Ctrl+M, `access-rooms`; review with Ctrl+V. `docs/rooms.md` |
| Target beacon guidance | exploration mode installed 2026-09-24; listening pending | Scanner, then L3 / Ctrl+B; one press switches to a newly scanned target. Exploration (default) routes only through visited rooms; opt-in full assistance uses the whole generated room graph. `docs/beacon.md` |
| Native automap discovery | investigated; strict mode not implemented | Current guidance retained. Native reveal state remains unverified; `.ma0` container structure inspected only. `docs/automap-discovery-research.md` |
| Panel open/close announcer | done | `ui_message_monitor.cpp` + `panel_state.cpp`; localized titles from `generated/panel_titles.h`; modals: title + native prompt text + controller hint |
| Widget vocabulary from layout JSON | done | `game-data/` (gitignored) → `tools/gen_panel_widgets.py` → `src/generated/panel_widgets.h` (105 panels, PC variants; runtime-filled states like `gameVersionNames` followed) |
| Controller focus tracking (front end) | wip — return-to-menu fix awaits live check | Native focused object + full parent path resolves catalog controls first; rectangle coverage remains fallback. Fixes silent ribbon actions after gameplay. Value-control subclasses retain their reader. `docs/controller-navigation.md` |
| Control values | done | Sliders (runtime `Indicator`), toggles/carousels (Settings.json baseline + input direction; text language locale order), dropdowns (popup rows spoken from native slot text; picked index tracked) |
| Dropdown popups / scrolling lists | done | Popup slots `Items/ListItemN/TextBox` read natively (resolutions spoken "W por H"); scroll refresh only near the indicator after a settled scroll |
| Main menu / character select | selection fix installed 2026-09-22; listening pending | Native selector bounds resolve the selected runtime row when focus stays on the first; `Name` + `Level&Class` determine speech. |
| Dynamic text (modals, popup rows) | done (front end) | `native_widgets.cpp`: D2RCore slot table → game FindPanel/FindChildWidget; text box text at +0x88. Pending: ButtonWidget labels (modal Confirm/Dismiss probed), legends, runtime tooltips |
| Performance | done — improvement confirmed by user | Scoped native reads and immediate capture on selection change installed; user reports improvement. `docs/inventory-npc-followup.md` |
| In-game text (tutorials, auto hints, NPC dialogue, area names) | wip — Charsi dialogue observed in logs | Native visible-tree/focus reader at 30 Hz, full speech text, diff-gating; lifecycle starts after LocalPlayerReady. No SetText hook needed. `docs/test-ingame-text.md` |
| New-character tutorial (mod commands, radar, scanner, through the Den of Evil and a waypoint) | built 2026-10-21; v1 played through live 2026-10-21, tester list reworked it the same day | 18 steps, sound lessons with again/next, side lessons (level-up, inventory, gold, shop), message history (BACK + right stick left/right, BACK + D-pad up; K, L, Shift+K). `docs/tutorial.md` |
| NPC menus / dialogue topics | wip — Charsi menu/topics observed in logs | Actual focused ListItem/Button text, heading + initial option, option-only navigation, return from dialogue; hidden templates excluded. Log includes introduction, gossip, trade/repair and cancel; explicit dialogue validation pending |
| Area entry banners | wip — camp entry confirmed | User confirmed hearing Campamento de las Arpías; boundary/dungeon checks remain |
| System notices / G graphics restriction | done — repeated G delivery confirmed by user | Native message identity rearms a newly delivered identical warning; stationary/older notices remain silent. `docs/test-ingame-text.md`, `GEMINI.md` |
| Internal town landmarks | wip — investigated, not implemented | Actual camp/NPC/object records identified; runtime world positions still need deriving. No kitchen/square district labels found in reviewed tables. `docs/map-landmarks.md` |
| Native world / local map | wip — camp and Blood Moor runtime confirmed | 18:19–18:29 session retained terrain, entered level 2; movement mask 1C09. Direct room reads, 19 fingerprints. Full-map coverage unverified. `docs/sonar-radar.md` |
| Level scanner (right stick) | user-confirmed 2026-09-24 | Up/down categories, left/right objects, nearest first; enemies live, loot and ordinary chests excluded. Replaces the directional scan and the L3 pathfinder. `docs/entity-scan.md` |
| Own corpse recovery | built/deployed 2026-09-23; live recovery pending | Native ownership check excludes other players and scenery. Separate scan identity supports beacon locking; loaded rooms only. Startup guards verified. `docs/player-corpse-re.md` |
| Friendly NPC announcer | line of sight on by default 2026-09-18; live check pending | Nearest friendly NPC spoken once when in range (15 subtiles, 3 hysteresis), distance + screen direction, no interrupt; silent in menus/dialogue/UI speech. Ctrl+N, `access-npc`, `[npc]` in `radar.toml`. `docs/npc-proximity.md` |
| Sonar radar | precision fixes installed 2026-09-25; live listening pending | Stick cardinals (diagonal frame selectable); obstacles kept while slide prevention is on; cue interval only between openings; no repeated reading; way-back lapse. `docs/radar-readings.md`, `docs/exploration.md`. |
| Connected navigation research | done locally - runtime integration built | Native body graph's bounded directional passage query (`ScanPassage`) is now the radar's only classifier, driving all four directions. No persistent doorway IDs/full-map guidance. `docs/D2RNavigationResearch.md`, `docs/radar-connected-passages.md` (historical), `docs/radar-redesign.md` (current use). |
| Stalled local footsteps | wip - second sliding case verified; suppression pending | 23:52:33/40 grids show up/right blocked with real travel along world X; +11.008728 then -9.091965 X in five-second intervals. No stalled decisions. No filter change; Ctrl+T now distinguishes recent movement from straight-ray blockage. Full stationary suppression remains unverified. `docs/footsteps.md` |
| Local computer use | done — initial live check | Python stdlib capture + bounded Windows inputs; menus, scroll, Ctrl+T and four movement directions verified with NVDA active. F9 up, F10 right, F11 down, F13 left assigned through game UI. Twelve mocked-input tests pass. `tools/computer_use/README.md`; no audio capture/analog controller emulation |
| Radar cue delivery log | wip — log format updated for the redesign | `[radar-audio]` still retains ID/origin through queue, submission, completion, cancellation or drop (no retries now); `[radar-direction]`/`[radar-state]` replace the removed opening-hold/wall/guidance/reference lines. Live validation of the new log pending. `docs/radar-redesign.md` (current fields), `docs/radar-event-log.md` (superseded) |
| Radar / footstep automated tests | done (local) | Six radar suites / 1439 checks against the confirm-by-travel engine (state machine, corridor stability, recorded camp/return grids, snapshot RLE, audio, readout), plus footsteps. Expectations derived from geometry, named constants and captures; timing printed, never asserted. |
| Automated text tests | done (local) | Nine CTest suites, 255 checks: text/notifications, guarded memory/tree and cell queries, gameplay lifecycle, tooltips, action help, equipment locations, delayed item speech without coordinates, shared tabs and a deliberately blocked SDK log sink |
| Highlighted NPC/object labels | wip — sight gate built 2026-09-18, live check pending | Shared primary tooltip capture plus visible target-name HUD, now dropped when every entity of that name within reach is behind a wall. `[target]` in `radar.toml`. `docs/native-tooltips.md`, `docs/target-sight.md` |
| Container loops | built 2026-09-18, live check pending | Chests/caskets/urns/barrels/corpses through the item mixer; opening one silences it. Rows from `objects.txt` `OperateFn`, tripwire checked by `chests-dump`. Ctrl+K, `[chests]` in `radar.toml`. `docs/ground-items.md` |
| Character and mercenary statistics | advanced rows installed 2026-09-23; listening pending | Shared stat rows and right-stick advanced list; title/first row announced together. `character-stats` suite; `docs/character-panel.md`. |
| In-game panels (inventory, skills, stash) | wip — basic selection and fluency confirmed | Shared item/grid archetype, native row/column; shared tab reader covers controller strip and unfocused stash changes (tab speech unconfirmed); skills/full inventory operations remain pending |
| Inventory potion assignments | built/deployed 2026-09-23; controller listening pending | Belt focus reads native D-pad direction, row, item and physical column total; automatic inventory potions identified separately. No new View/Back binding. `docs/belt.md` |
| Inventory actions / equipment slot names | wip — slot-name memory + placement destination installed 2026-09-17, live check pending | User confirms available slots read while holding equipment and prefers this navigation. Added SDK-driven pickup/release context notices. Controller item/vendor actions live in the `ControllerOverlay` `ButtonLegendScrollWidget`, whose sub-text slots are now read when its entry vector is not the one `ButtonLegendWidget` uses. Live notice/repair check pending. `docs/inventory-npc-followup.md` |
| Attribute allocation / skill-tree reading | wip — descriptions/actions confirmed by user | Stock hover refresh fixes repeated skill descriptions. Current binding, available action buttons and position/count precede the full description. All 30 live Barbarian class cells are reachable in regression tests. Live log distinguishes lower-row skills and reads Porrazo's A binding/X reassignment; user confirms it works. Zero points prevents live spend-action validation. `docs/skill-actions.md` |
| Item readout and comparison | concise reading/speed confirmed; comparison installed 2026-09-22 | Full visible selected/equipped stats on Compare; repeat while displayed; stale/glyph-only records excluded. 61/61 suites pass. `docs/native-tooltips.md` |
| Nearby ground item speech | every-pass rearm and priority confirmed by user 2026-09-24 | Localized base name/quality and gold amount within 3 subtiles; every pass, rearm beyond 4; quest > consumable > gold > equipment; Ctrl+I. Grammar tags removed after live bolts/arrows evidence. Full generated equipment names remain outside this reader. `docs/ground-items.md` |
| Level exit loops | portal restoration deployed 2026-09-23 — listening pending | Interactive entrances use `portal`; walking crossings use `zone transition`. 4 suites pass. Ctrl+P. `docs/exploration.md` |
| Life, mana and broken equipment | combined mercenary priority installed 2026-09-23; listening pending | Simultaneous alerts combine with player life first; downward critical crossings interrupt. Broken gear queues. Missing orb text is identified after stable absence. `access-vitals`; `docs/vitals.md` |
| Poison and mercenary condition | life-source fix and level speech installed 2026-09-23; live combat pending | Panel-native mercenary percentage, shared player thresholds and level-change tracking. Missing data stays unknown. Poison unchanged. `docs/combat-status-evidence.md` |
| Spatial item / quest loops | tree and faster/farther confirmed; Cairn symbols installed 2026-09-24 | 700 ms, 45-subtile reach; all neutral stones qualify. Fixed symbols in scan; sequence read only when scroll opens. Ctrl+L, two priority voices; 74 suites pass. Listening pending. `docs/ground-items.md` |

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
- **Tooltip frames**: `TooltipsPanel +0x168/+0x170` is a contiguous vector, stride `0x2E8`, body/footer `+0/+0x28`. Verified panel draw `0x880550` brackets actual `0x8DA7E0` callbacks, publishing primary + visible comparison records together. Glyph-only entries are excluded. `docs/native-tooltips.md`.
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

Check that a dungeon entrance uses the portal loop and a walking zone border
uses the zone-transition loop. `docs/level-exits.md`.

Compare the new voiced enemy swoop against the radar's floor breath and check
the much faster repetition for both NPCs and enemies during play.
`docs/entity-cues.md`.

Also validate the 2026-09-22 menu fixes: move between existing characters, read
mercenary statistics/equipment with an available hireling, and toggle/cycle item
comparison. The user has confirmed the concise item reading and reduced delay.

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

- **D2RLoader ignores the Steam language (loader bug, 1.3.0-beta).** With `text_locale`/`audio_locale`
  empty, a Steam launch through the loader comes up `enUS` (plugin probe "close" = "close"), while
  vanilla `D2R.exe` (Steam app language `spanish`, `Settings.json` `Language: 3`) shows "Pulsa
  cualquier tecla para empezar". Tested 2026-09-26. `D2R.exe` holds its own Steam-name → locale table
  (`spanish`, `latam`, `brazilian`...); the loader binaries contain none. Workaround: set both locales
  explicitly. To be reported upstream.
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
- **Sonar radar:** the 2026-09-16 confirm-by-travel redesign is installed but **not yet live-validated**, including the 18:10 forensic fix (1-cell lateral tolerance, openDepth 10/close 7, 5-cell clutter cap) that corrected the 76%-at-contact wall cues and false "exit" cues a live-session analysis found in the first cut. Direction count (four) and sound design are unchanged from before the review: an eight-direction/new-sound-design attempt was tried live and reverted at the user's request. Directional wall-sliding prevention is now installed; controller validation is pending (`docs/wall-sliding.md`). `docs/radar-redesign.md`.
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
