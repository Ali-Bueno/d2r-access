# Recoverable player corpse

The native entity reader now includes a loaded player corpse when the game's
inventory link ID identifies the local player. The ordinary scan and beacon can
name and follow it; pickup or unloading removes it from the next native snapshot.
It has no container audio. Live death/recovery testing remains pending.

## D2R 3.3.93787 evidence

The source is the fresh full mapped image captured on 2026-09-23
(`D2R.image.bin`, base `0x140000000`, file offset = RVA). All offsets below are
RVAs or structure offsets in that image, not borrowed from classic Diablo II.

- The client UI's `D2Client\\src\\UI\\ui.cpp` function at `0xCE8F0` compares
  player Unit mode with **17** at `0xCE927`. That branch uses the game's
  `DeadPlayerCorpseName` localization key. The installed `ui.json` gives it
  ID **26699** and `esES` **Cadáver**.
- The server corpse-pickup handler in `D2Game\\src\\Player\\PlrMsg.cpp`
  (fragment beginning `0x4B2C8A`) walks room units and checks Unit type **0**
  before treating one as a corpse (`0x4B2D1A` calls `0x34B9D0`). At `0x4B30C7`
  it gets that unit's inventory (`0x34A360`) and at `0x4B30E1` reads its
  link ID (`0x388BA0`). At `0x4B3275` it obtains the looting player's Unit ID;
  `0x4B3296` compares that ID to the corpse inventory's link ID. This is the
  ownership check used by the game, not a name or a guessed visual tag.
- `0x34A360` reads Unit `+0x90` for the inventory. While the Unit `+0xA0`
  reference has a positive count at `+0x08`, it uses Unit `+0x98` when present.
  `0x388BA0` validates inventory magic `0x01020304` and returns its link ID
  at `+0x48`.
- Four complete-function fingerprints guard these D2R findings separately
  from the existing monster/object/exit reader. If corpse evidence changes,
  only this target is disabled; the ordinary entity scan continues.
- Player and monster position uses the already verified dynamic path: Unit
  `+0x38` to Path, packed 16.16 X/Y at Path `+0/+4`, room at Path `+0x20`.
  The reader requires the path room to match the room list entry.

The local candidate must therefore be a different type-0 Unit in mode 17, with
a valid inventory whose link ID equals the verified local Unit ID. It is named
with the game's localized corpse string. The `objects.txt` corpse rows are
scenery containers and do not satisfy this test. A corpse belonging to another
player has a different link ID and is excluded.

## Validation

The 2026-09-23 build passed all 67 CTest suites. Live startup on 3.3.93787
verified the separate corpse ownership guards; a death/retrieval test remains
pending.

`native-entities` tests use constructed native memory to cover own versus
foreign corpses, living players, invalid inventory magic, the inventory owner
wrapper, ordinary scenery containers, and pickup/unload disappearance. Confirm
the same behavior in a live softcore game: die with equipment, respawn, scan
and lock the corpse, retrieve it, then verify it disappears. Also verify two
players' corpses and a body in an unloaded room when practical. The reader
only searches the current room and its loaded neighbors; it cannot discover an
unloaded corpse elsewhere in the level.
