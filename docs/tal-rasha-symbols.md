# Tal Rasha's tomb symbols

Implemented 2026-10-03 for D2R 3.3.93787. Journal selection and repeat name the
visible true-tomb symbol in plain text. Each tomb entrance is named with its
fixed marking, for example `To Tal Rasha's Tomb: circle`. The entrance label
does not identify whether that tomb is correct; the player compares it with
the quest log. Portals to a tomb retain their ordinary portal destination name.

## Latest-session evidence

The user's own `d2r-access.log`, 2026-10-03 06:38–07:29, contains Horazon's journal
reading at 07:22:31. At 07:26:44.980 and 07:26:49.645 the quest watch reports:
`short=The Seven Tombs long=The Symbol of the True Tomb of Tal Rasha. identified=1`.
Selection speech at 07:26:45.067 and 07:26:49.729 repeats that sentence without
the symbol. This proves the text-reader gap; the log contains no image-frame
value, so it does **not** establish which symbol was correct in that session.

All six classic/HD/controller original/expansion quest layouts contain an
`ImageWidget` named `TalRashaSymbol` under quest button `Quest11`. It has no text.
The original HD layout points to `PANEL/Quest_Log/TalRashaSymbol`; the expansion
layouts share the template. This is separate from the Cairn Stone symbols.

## Verified native path and artwork

Read offline from workspace `re/D2R_image.exe`, whose file offsets are live RVAs:

- Server routine `0x516B40..0x516E3F` calls the staff-tomb level getter at
  `0x516DE7`, subtracts `0x42` (66) at `0x516DEC`, and stores that value into
  offset +5 of its opcode `0x50`, quest ID 1 initialization packet.
- Client dispatcher `0x232EA0` routes quest ID 1 to `0x16D740`. That setter
  copies the packet's +5 uint16 to `0x2A8D004`.
- Getter `0x16C430` returns this global. Journal population routine
  `0x14DE610..0x14DEC34`, at `0x14DEA20`, clamps it to 0–6 and passes it directly
  to `ImageWidget::SetFrame` (`0x859700`). The setter stores an int32 at +0x90.
  Visibility is set separately at `0x14DEA42..0x14DEA69`, conditional on the
  selected quest's current journal state. A stored global alone would therefore
  reveal information the journal is not necessarily showing.
- The installed CASC `TalRashaSymbol.sprite` and `.lowend.sprite` each have seven
  frames. Both are SpA1 version 31 RGBA atlases. The high-resolution atlas is
  1442×186, with 204-pixel frames at a 206-pixel stride. Frames were decoded and
  visually inspected, using the original
  [sprite exporter's format description](https://github.com/dzik87/d2r-sprites/blob/main/d2r-sprites.py).

| Frame | Destination level | Plain symbol name |
| --- | --- | --- |
| 0 | 66 | four-pointed star |
| 1 | 67 | square |
| 2 | 68 | crescent |
| 3 | 69 | circle |
| 4 | 70 | double chevron with a diamond |
| 5 | 71 | triangle |
| 6 | 72 | circle above a crescent |

`Levels.txt` identifies 66–72 as the seven Tal Rasha tombs, all sharing the same
localized name. The server's frame = destination − 66 relation establishes
their fixed symbol identity. As a cross-check, native `0x59E9D0` excludes the
staff tomb's index from the six Arcane glyph objects using table `0x23A7FD8`
(`313,312,308,310,311,309,307`). Classic
[A2Q6 quest code](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Game/src/QUESTS/ACT2/A2Q6.cpp)
has the same exclusion rule; its packet quest enum is not assumed to match D2R.

Workspace inspection files: `re/logs/tal-rasha-re.txt` and
`re/tal-rasha-assets/symbol-frames.png`. Extracted game artwork is not part of
the plugin or tracked source.

## Implementation and boundaries

`tomb_symbols.h` shares the level/frame index and entrance naming policy.
`speech_strings.h` and all 13 locale tables provide the shape vocabulary; these
are descriptive mod phrases, not existing game string-table entries. Translations
other than English remain unreviewed by native speakers.

`NativeWidgets::Initialize` verifies complete fingerprints of the frame setter
and journal population routine before enabling `SupportsTombSymbols`. Startup
logs `[tomb] Verified visible journal symbol reader.` Both fingerprints also
match the existing live `D2R.text.bin` dump. A mismatch keeps ordinary
journal text and disables symbol reading.

`NativeUi::Snapshot` processes only nodes already copied from the visible tree.
It requires an ImageWidget named `TalRashaSymbol`, a parent Quest11 quest button,
a matching live `ShortDescription`, a nonempty objective and frame 0–6. It appends
the symbol to the copied `LongDescription`; it never writes the native widget,
invokes the quest builder or reads the true-tomb global. The existing UI reader
then speaks/settles/repeats the whole objective through the PRISM queue. The
background quest watch still reads the game's original text.

`NativeEntities` decorates loaded warp entrances and existing distant presets.
`TerrainService::KnownPlaces` applies the same policy after restoring and
relocalizing stored entrances. The native label remains in `worldName` and its
localization key remains in `nameKey`, avoiding duplicate suffixes and preserving
map identity. Navigation and interaction targets are unchanged.

## Validation

The normal four-job build/deploy succeeded with the game closed. Regression
checks cover all seven visible journal frames and loaded tomb entrances, repeat,
hidden image/ancestor, invalid frame, unsupported layout, stale/missing details,
locale changes, repeated entrance composition and portals without markings.
All 96 CTest suites passed (`--timeout 20`); installed DLL SHA-256
`BB80B7CD56357267BDE98A2422EC3DB50C4D5921628874F9A962F9A477700196`
matches the build. Live speech is not yet tested.

User-led check: open the quest log and select **The Seven Tombs**. The objective
should end with a plain shape name. Home/repeat should include it again. In the
Canyon of the Magi, cycle the exits: each tomb should include its own shape name.
Match that name with the journal, then choose the entrance normally. In the new
log, `[speech]`/`[text]` should include the journal symbol and `[survey]` should
include the entrance suffixes. A hidden journal graphic must supply no symbol.
