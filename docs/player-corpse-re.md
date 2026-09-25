# Player corpses

The native entity reader includes every loaded player corpse: a type-0 unit in
mode 17 whose inventory link ID names an owner. The local player's is "your
corpse"; another player's is named from the corpse's own PlayerData, as the
game's label does. The scanner lists them in their own **corpse** category,
after players (`docs/entity-scan.md`). It has no container audio.

`CorpseMemory` keeps each corpse seen on a level for the rest of the game, so
it is listed from anywhere on that level, across town trips. A corpse is
forgotten only after three reads that covered its place without it: the
player's room or a room touching it, with collision loaded
(`CorpseMemory::Covered`). The entity reader reads the player's room and its
near list, so touching rooms are always inside it.

The party roster adds two things (`CorpseMemory::Track`, next section):

- **A party member's corpse from any distance.** When a new corpse ID appears
  in a party member's roster entry while the roster gives their life as 0
  (dead, not yet respawned), it is placed at the member's roster level and
  position. Tracking starts with a baseline, so corpses already listed when
  the game is joined are not placed. Reading the corpse itself later corrects
  the position.
- **Pickup from any distance.** A remembered corpse whose ID leaves the
  roster lists is forgotten at once.

Players outside the party get no position from the server, so their far
corpses are only known once seen. Live death/recovery testing remains pending.

## Roster corpse evidence (2026-09-24)

Static analysis of the same image. Server facts come from the D2Game copy
embedded in the client; they cannot be proved for Blizzard's servers.

- **Creation:** the corpse is spawned when the death animation ends (mode 0
  to 17), not at respawn. The end-of-mode handler `0x42DE40` calls the creator
  `0x42C110`, then sets mode 17 (`0x42E2AF`). The creator spawns a type-0 unit
  in mode 17 at the dying player's path X/Y (`0x490410` at `0x42C656`). It
  skips creation at 16 corpses (`0x42C5D7`), which bounds the list walk.
- **Name:** the corpse's PlayerData+0 holds the owner's name. Packet 0x59
  (client `0x12CF70`) copies it through `0x34EBA0`.
- **Owner link:** packet 0x74 (client `0x12D7E0` then `0xFF910`) sets the
  corpse inventory's link ID to the owner, while the corpse is loaded.
- **Dead owner's own unit:** it lies in mode 17 on the same spot until
  respawn, so a corpse must also differ from its owner's unit ID.
- **Roster list:** packet 0x8E goes to every player in the game. Assign
  (`0x136800`) prepends a node `{+0 corpse id, +8 next}` to the owner's roster
  entry at **+0x78**; unassign (`0x138150`) removes it on pickup. No position
  is stored anywhere on the client.
- **Roster life:** packet 0x7F `[+1 party flag][+2 life word][+4 id][+8 level]`
  (`0x12DE10`) stores life at **+0x4C** as a DWORD (`0x139460` at `0x1394E6`),
  percent 0..100. New entries are zeroed. Only party members receive it. That
  it reads 0 while dead is inferred (life 0 at death), not traced.

`EntityLayout::RosterCorpseChecks` guards the roster functions; a mismatch
disables only distant corpse placement. The 0x74 handlers joined
`CorpseChecks`.

Live checks still open: the roster life reaching 0 and the +0x78 node on the
other client; the local unit's position against its corpse; the roster
position against the corpse for a member dying beyond 50 subtiles, on this
level and on another.

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

A candidate must therefore be a type-0 Unit in mode 17, other than the local
player's own unit, with a valid inventory whose link ID is non-zero. The link
ID equal to the local Unit ID marks the player's own corpse. The `objects.txt`
corpse rows are scenery containers and do not satisfy this test. Without the
verified PlayerData name reader (`EntityLayout::PlayerChecks`), another
player's corpse keeps the game's localized word (`DeadPlayerCorpseName`).

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
