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
- Switch `descriptions` (on, Navigation group): the landmark words and the area hint. Words asked
  for with BACK + right stick up are said either way.

## The player's file

`d2rloader/plugins/d2r-access/descriptions.txt`, written with notes when missing, never shipped,
so an update keeps it. One line per place: the name as the mod says it (any case), `=`, the words;
`#` starts a note. Its words win over the mod's own. Area names are the game's, in the game's
language, as the area banner says them. The tester writes area descriptions there; finished ones
can move into the strings layer to be translated.

Logs: `[descriptions] N read from descriptions.txt`, `[descriptions] hint for "<area>"`,
`[descriptions] BACK+up in "<area>" (level N)`, and the place line of the first landmark reach.
