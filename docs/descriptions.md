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

## Words that come with the mod (2026-10-24)

`data/descriptions.txt`: the tester's words for Act I and Act II (67 areas and the Hangman's Tree),
in the player's file format, English names. CMake embeds it in the DLL as RCDATA `DESCRIPTIONS`
(`descriptions.rc` in the build folder); `Descriptions::Load` parses it first and the player's file
after it, so a player's line for the same name wins, and a line taken out of the player's file gives
the mod's words back. With the game in another language the names do not match and nothing is said
(the strings layer's `hangmanTreeDescription` still is). Names are per level name, so "Sewers Level
1" also answers in Act III's sewers. Log: `[descriptions] N come with the mod, M with descriptions.txt`.

## The player's file

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
