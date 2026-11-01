# Area and landmark descriptions (2026-10-17)

A tester (2026-10-16), approved by the user: hand-written, atmospheric words for places, without
lengthening every announcement.

- **Landmarks**: the first time a landmark with words comes into reach in a run, its words are
  said instead of its name; afterwards the name. The first is the **Hangman's Tree** of the Burial
  Grounds: a new `PlaceFeature` from the HD layout of `act1/graveyard/gravey.ds1`, anchored on its
  hanging corpses and nooses (`act1_graveyard_hanging_corpse0N`, `act1_graveyard_props/noose_wrap01`,
  around subtile 58,86 of the piece, beside the oak `act1_tree_oak04`; `tools/gen_places.py`). Its
  words are `hangmanTreeDescription`, translated.
- **Areas**: **BACK + right stick up** says the words of the area the player is in, or "No
  description for <area>". The first time a described area's name is announced in a run, a hint
  follows it: "Press View and right stick up for an area description" (the button named as the
  connected controller names it).
- Switch `descriptions` (on, Navigation group): the landmark words and the area hint (the hint has
  its own switch, off, since 2026-10-26: below). Words asked for with BACK + right stick up are
  said either way.

## Words that come with the mod (2026-10-24; per language since 2026-11-01, below)

`data/descriptions.txt`: the tester's words for Act I and Act II (67 areas and the Hangman's Tree),
in the player's file format, English names. CMake embeds it in the DLL as RCDATA `DESCRIPTIONS`
(`descriptions.rc` in the build folder); `Descriptions::Load` parses it first and the player's file
after it, so a player's line for the same name wins, and a line taken out of the player's file gives
the mod's words back. With the game in another language the names do not match and nothing is said
(the strings layer's `hangmanTreeDescription` still is). Names are per level name, so "Sewers Level
1" also answers in Act III's sewers. Log: `[descriptions] N come with the mod, M with descriptions.txt`.

## The player's file (removed 2026-11-01, below)

`d2rloader/plugins/d2r-access/descriptions.txt`, written with notes when missing, never shipped,
so an update keeps it. One line per place: the name as the mod says it (any case), `=`, the words;
`#` starts a note. Its words win over the mod's own. Area names are the game's, in the game's
language, as the area banner says them. The tester writes area descriptions there; finished ones
can move into the strings layer to be translated.

**Edited while playing** (a tester, 2026-10-19): a request for words (a landmark reached, an area
announced, BACK + right stick up) looks at the file's write time at most once a second and reads it
again when it was saved. A name whose words changed, or that is new, is "the first time" again; the
others stay told. **The names it answers to**: `descriptions-names.txt`, beside it, is written in
the first game of each run (and when the game's language changes) with every area name from the
levels table and every landmark word, exactly as the mod says them. **The banner's form works
too**: the level is "Cave Level 1" (`LevelName`) but its banner says "Entering The Cave Level 1"
(`LevelEntry`, a string of its own per level), so BACK + right stick up (`Descriptions::FindArea`)
takes the level's name, else a written name that the last banner ends with and that ends with the
level's name: "The Cave Level 1" or the whole banner, never "Swampy Pit Level 1" for "Pit Level 1".
The side dungeons are "Pit Level 1", "Hole Level 1" (banners "Entering The Pit Level 1"...).

Logs: `[descriptions] N read from descriptions.txt`, `[descriptions] N read again from
descriptions.txt, saved while playing`, `[descriptions] N area and M landmark names written to
descriptions-names.txt`, `[descriptions] hint for "<area>"`,
`[descriptions] BACK+up in "<area>" (level N)`, and the place line of the first landmark reach.

## A level by its Id; the area hint (2026-10-25)

The tester: "Sewers Level 1" and "Sewers Level 2" name both Lut Gholein's sewers (Levels.txt Id 47,
48) and Kurast's (92, 93), so one line answered in both. A name ending in a level's Id in brackets,
`Sewers Level 1 [92] = ...`, is kept under that Id alone (`Descriptions::LevelKey`, the words before
it are the writer's own) and wins over a line with the name only, whichever file it is in; a later
line for the same Id replaces an earlier one, so the player's file still wins line by line. The mod's
own file keys its four sewer lines that way, and `descriptions-names.txt` lists a name two levels
share once per level with its Id.

The hint after a described area's banner never came: it looked the banner's text ("Entering Lut
Gholein") up as a name. It now takes the level the player is in (`Descriptions::NoteLevel`, from the
beacon service's level change) with its name and the banner, as BACK + right stick up does.

## The area hint off by default; Act III and the Fortress (2026-10-26)

The tester wants the lore met at the player's own pace: the hint after a described area's banner is
now its own switch, `description-hint` (Navigation group, "Area description reminders", **off**);
`descriptions` ("Landmark descriptions", on) keeps the landmark words. BACK + right stick up says an
area's words with both off: the manual tells of it, and the tutorial teaches it.

`data/descriptions.txt` now holds the tester's words for Acts I to III and the Pandemonium Fortress
(96 areas and the Hangman's Tree). The tester writes the sewers by name only; the mod's file adds the
Ids, `[47]` to `[49]` for Lut Gholein's and `[92]`, `[93]` for Kurast's.

## The Hellforge; Act IV and Harrogath (2026-10-27)

The tester's new file: Act IV's areas (Outer Steppes, Plains of Despair, City of the Damned, River
of Flame, The Chaos Sanctuary) and Harrogath, plus their spelling fixes ("ground", "The river
splinters", "Kurast's"). `data/descriptions.txt` takes it with the mod's own header: 102 areas, the
Hangman's Tree and the Hellforge. The Hellforge is the second landmark with words: a `PlaceFeature`
at the River of Flame's `SoulStoneForge` object ([places](places.md#the-fortresss-hall-the-hellforge-the-pentagram-2026-10-27)),
said with `hellforgeDescription` (translated) the first time it comes into reach
(`LandmarkWords` in `place_service.cpp`).

## Act V (2026-10-30)

The tester's new file adds Act V's 23 areas, Bloody Foothills to The Worldstone Chamber (Harrogath
was already in; 125 areas and two landmarks now). Names checked against the game's English level names: "The Arreat Summit" is
"Arreat Summit"; typos fixed: "it is painfully", "its primordial", "Mount Arreat", a double space.
`data/descriptions.txt` keeps its own header and the Hellforge, which the tester's file lacks.
Both "Arreat Summit" and "The Arreat Summit" answer: the second by the banner's form above.

**Coverage (2026-10-30)**, every level's English name (`LevelName` through `levels.json`) against
the file by the rule above: all answered but The Secret Cow Level (39), the three Pandemonium
areas Matron's Den, Forgotten Sands and Furnace of Pain (133-135) and Colossal Summit (137).
Uber Tristram (136) shares the name "Tristram" with the ruins (38): `Tristram [136]` would part
them. **Since build test-2026-10-30-2's notes every level is answered**: the tester's words for
the cows, and one placeholder ("Hell's influence has warped this area beyond all recognition.")
for the three Pandemonium areas, `Tristram [136]` and Colossal Summit, until the tester has been
there. Duriel's Lair is "Tal Rasha's Chamber" and Kurast Docktown "Kurast Docks" in the game's
own words, as the file has them.

## In every language, sealed, from the source only (2026-11-01)

The user: the words were English only (the names above matched only an English game), and they
should be changed only in this repository, by the tester or another contributor.
- **Files:** `data/descriptions/<locale>.txt`, one per language the mod speaks. `enUS.txt` is the
  source; every other file falls back to it line by line (`Layered`). An area's line is
  `Name [Id] = words`: only the Levels.txt Id counts, the name is for the writer. A landmark keeps
  its English name as key in every file (`Hangman's Tree = ...`); `place_service.cpp` looks it up by
  the enUS `placeFeatures` name; the strings layer's `hangmanTreeDescription` and
  `hellforgeDescription` are gone, so no description is left in plain text. The other languages were machine-translated from English with the
  game's own names (`game-data/local/lng/strings/*.json`).
- **Sealed:** `tools/seal-descriptions` (built and run by CMake) packs every file and seals the lot
  with AES-256-GCM (Windows CNG, `bcrypt`), with a new random key each time the files change; the
  plugin embeds the blob (RCDATA `DESCRIPTIONS`) and the generated `descriptions_key.h`.
  `Descriptions::OpenBundled` opens it in memory; an altered byte fails the tag and no words are
  said (`[descriptions] none: the sealed words were altered`). The key is in the DLL, so this stops a
  resource or hex editor, not a determined reverse engineer.
- **Language:** the book follows the language the mod speaks (`Strings::Selected`), read again when
  it changes (`[descriptions] N in deDE (its own words, English under them)`).
- **The player's `descriptions.txt` is gone**, with `descriptions-names.txt` and the live re-read:
  nothing outside the source changes the words. Old copies in the plugin folder are ignored.
- **Checks:** `descriptions-tests` reads every file: each line a note or `name = words`, every level 1
  to 137 in English, no language with a key English lacks (a mistyped Id).
