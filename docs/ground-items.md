# Ground item speech and spatial loops

Implemented 2026-09-16 for the installed D2R 3.3.93787 executable. The user chose
fixed screen orientation: screen up is ahead and screen down is behind. This
feature has its own native snapshot, tracker and audio worker; it does not change
radar geometry or radar playback.

## Behavior and controls

Both features start enabled. **Ctrl+I** toggles proximity speech; **Ctrl+L**
toggles item loops; **Ctrl+P** level exit loops; **Ctrl+K** container loops
([below](#container-loops)). Console:
`access-items on|off|sound-on|sound-off|exits-on|exits-off|chests-on|chests-off|chests-dump|status`.
Preferences last for the current process. Ordinary item speech queues through
PRISM without interrupting the current announcement.

An item within three collision subtiles reads its localized base name, quality
when applicable, and the native amount for gold. The user asked (2026-09-24) for
a reading on every pass: moving beyond four subtiles (one subtile of hysteresis)
rearms it, and an item that has just come within reach speaks at once, without
waiting for the previous group. Names read together follow the user's priority:
quest items, then potions/elixirs/scrolls, gold, and equipment or other items
last; distance orders each tier. Up to four names are grouped per announcement;
the rest of a pile follows in groups at least 900 ms apart while nearby. The user
confirmed this behavior in play the same day.
There is no speech backlog for items that have already disappeared.
The shared world-tooltip reader defers matching nearby names to avoid duplicates.
Inventory descriptions still use the game's complete rendered tooltip.

Names come from the active game's Items table and localization service. The
`[fs]`, `[ms]`, `[fp]`, `[mp]`, `[ns]`, `[pl]` and `[nl]` grammatical markers in
`item-names.json` are omitted. Equipment speech currently identifies the **base
item and quality**, not its complete generated affix/unique display name. It
does not reveal unidentified affixes.

| Supplied loop (`sounds/item loop sounds/`) | Source |
|---|---|
| `loot/gold.ogg` | Gold |
| `loot/potion.ogg` | All descendants of the potion and elixir item types, except quest items |
| `loot/scroll.ogg` | Descendants of the scroll type (`scro`: identify, town portal) |
| `loot/rune.ogg` | Descendants of the rune type (`rune`) |
| `loot/gem.ogg` | Descendants of the gem type (`gem `: every gem grade and the skulls) |
| `loot/normal equipment.ogg` | Inferior/normal/superior equipment and remaining miscellaneous items |
| `loot/magic equipment.ogg` | Magic quality |
| `loot/rare equipment.ogg` | Rare and crafted quality |
| `loot/legendary equipment.ogg` | Unique and set quality |
| `loot/chest.ogg` | Containers you open for loot ([below](#container-loops)) |
| `loot/corpse.ogg` | Containers that are a dead body ([below](#corpse-loops)) |
| `loot/weapon rack.ogg` | Containers that are a weapon rack ([below](#weapon-rack-and-stash-loops)) |
| `loot/stash.ogg` | The stash, looped with the exits ([below](#weapon-rack-and-stash-loops)) |
| `loot/quest item.ogg` | Ground quest items and unused objects that supply quest items ([below](#quest-item-loops)) |
| `exploration/portal.ogg` | Level exits: warps, portals, waypoints |
| `exploration/wooden door.ogg`, `metal door.ogg`, `stone door.ogg`, `slime door.ogg` | Shut doors ([below](#door-loops)) |
| `exploration/unexplored room.ogg`, `zone transition.ogg`, `inside unexplored room.ogg` | Exploration cues ([exploration.md](exploration.md)) |

Loops begin only once the native item mode is **OnGround (3)**. The falling
animation (**Dropping, 5**) is excluded; there is no guessed landing timer.
Up to eight nearest items have independent continuous XAudio2 voices. X3DAudio
supplies the stereo speaker matrix. Fixed screen axes derive from native
projection `0x334DB0`: right = `(dx-dy)/sqrt(2)`, front = `-(dx+dy)/sqrt(2)`.
Ordinary sources fade to silence at 30 subtiles; quest sources reach 45. Rear sources smoothly
lower pitch to 94% across the first subtile behind the listener. Voices keep
their loop position across updates. The summed gain is limited to 0.5 for
headroom when a pile of objects peaks together. These ranges, gains and timings
are accessibility tuning, not claims about native pickup range or acoustic units.

Recordings are peak-normalized to 0.9 when loaded, before applying the near-source
gain of 0.4 and distance attenuation. This preserves their waveform and dynamics
without modifying the OGG files. A silent recording is rejected. The original
gold and normal recordings measured -44.6 and -41.0 dBFS RMS; applying the old
0.22 gain and stereo panning made them effectively inaudible against gameplay.
The user confirmed that normalization restored audibility, but requested a lower
level. Near gain was reduced from 0.6 to 0.4 and the pile limit from 0.75 to 0.5:
about 3.5 dB quieter for both individual items and piles.

Pickup, range exit, lifecycle changes, death, dialogue, pause and losing foreground
stop the applicable voices. Nonpausing inventory keeps the cues running and
defers proximity speech. The worker expires a snapshot after 300 ms even if UI
polling stops. Audio updates occur on its own thread; file loading and playback
never block a UI callback. Missing/bad audio files leave item speech usable.

## Container loops

Requested 2026-09-18: a cue for the things you open for loot — chests, caskets,
urns, jars, barrels, crates, baskets, corpses, tombs, hidden stashes, hollow
logs, rock piles. They loop exactly like the items on the floor (distance
squared into gain, X3DAudio stereo, 30 subtiles of reach) using `chest.ogg`,
taken from the author's DevilutionX accessibility fork, where the same sound
marks chests.

**Opening one silences it, with nothing extra in the code.** `NativeEntities::ReadObject`
already refuses any object the game itself will not let the player click in its
current animation mode, and a container is selectable while shut and not once
open, so an opened one leaves the unit list and its voice is stopped on the next
refresh (up to 500 ms).

**What counts as a container** cannot be the name — that is localized — so it is
the game's own interaction handler, `objects.txt OperateFn` (`row + 0x15E`):
handlers 1 (caskets, sarcophagi), 3 (urns, jars, baskets), 4 (chests), 5 and 7
(barrels), 14 (crates, coffins, corpses, logs), 30 (the exploding chest), 39-41
and 57-59 (the act-specific special chests) and 68 (the evil urn), combined with
the shut/open selectable shape above. That selects **165 of the 587 rows, 78
distinct keys**. Neither the offsets nor the handler numbers are guessed:
`tools/objects_containers.py` re-derives them from the extracted `objects.bin`
and `objects.txt` and proves every offset equals its named column on every row.
`PopulateFn` is the obvious candidate and is the wrong one: a single value covers
261 rows, beds, candles and banners among them.

`access-items chests-dump` logs the verdict for every object the player can
currently see, to check the predicate against a room whose contents are known.
It also counts the container rows in the live `objects.txt` and compares both
counts with `EntityLayout::ContainerRowCount` / `ObjectRowCount` — the patch
tripwire. If they disagree it says so; rerun `tools/objects_containers.py`
before trusting the cue.

After the reserved quest share, exits claim voices first, then containers up to `MaxContainerVoices` (3), then
items: a crypt full of caskets must not silence the loot on the floor or the way
out. Containers are never spoken — they carry no name into the mixer, exactly
like level exits. `radar.toml` `[chests] enabled` sets the state the mod starts
in; **Ctrl+K** toggles it for the current run.

## Corpse loops

Requested 2026-09-25: the dead bodies a player can loot (dead rogues, guard
corpses, villagers, harem guards, hung skeletons, trapped souls, dead
barbarians, the "Corpse" rows) loop `loot/corpse.ogg` instead of the chest cue.
They stay containers in every other respect: same predicate, same share of
voices, same `[chests]` switch and Ctrl+K toggle. Which containers are bodies
comes from the objects.txt `Name` key the game labels them with, listed in
`tools/gen_lootables.py` (`CORPSE_NAME_KEYS`) and emitted as
`Generated::CorpseObject` (25 classes on D2R 3.3). DungeonGuy is left out: its
key is the `Dummy` placeholder, so the game names nothing a player could recognise.

Loudness: the corpse, gem, scroll and rune recordings are short clicks, so the
build boosts them and limits the peak to 0.9 before the runtime peak
normalization (`D2R_CLICK_BOOST_DB`, 24 dB). On 2026-09-26 the user found the
corpse still too quiet. Measured on the built WAVs (RMS of the loudest 100 ms,
re peak), it sat at -17.7 dB against -9 to -13 dB for the other loot loops; its
own `D2R_CORPSE_BOOST_DB` of 42 dB brings it to -11.8 dB, the level of the quest
(-11.7) and weapon rack (-11.6) cues, with the tail still fading to -83 dB.

## Weapon rack and stash loops

Requested 2026-09-26 with two new recordings. Weapon racks (objects.txt
OperateFn 20: WeaponRack1/2/Left/Right, classes 106, 107, 548, 549) loop
`loot/weapon rack.ogg` instead of the chest cue; they stay containers otherwise
(same share, `[chests]` switch, Ctrl+K). `tools/gen_lootables.py`
(`WEAPON_RACK_FNS`) emits `Generated::WeaponRackObject`. Armor stands
(OperateFn 19) keep the chest loop. The stash (`Category::Stash`) loops
`loot/stash.ogg` as a landmark beside the exits, under their switch (Ctrl+P,
`access-items exits-on|off`). Neither recording needed a build-time boost
(-11.6 and -13.1 dB).

Follow-up the same day, measured as heard: each built loop repeated for 12 s,
EBU R128 integrated loudness after the runtime peak normalization. The loot
loops sit between -11 and -16 LUFS (median about -14.5); the chest was at
-18.6, the quietest. Raising it through the boost+limiter would take more than
12 dB of limiting, so a runtime trim does it instead: `SoundFile::levelDb`
scales the voice gain in `GroundItems::Spatialize`, chest +4 dB
(`ChestLevelDb`), stash -4 dB (`StashLevelDb`). The 0.3 s stash recording is
padded with silence to repeat once a second (`D2R_STASH_LOOP_SECONDS`), and the
stash loop pauses while the game's stash layout is open (`BankOriginalLayout`,
`BankExpansionLayout` in `PanelState::OpenPanels`); the log says
`[items] stash open: its loop paused` and `closed … resumed`.

2026-10-12: racks never looped. `IsContainer` took only `ContainerOperateFns`, which lacked 19
(armor stands), 20 (weapon racks) and 26 (bookshelves), though `gen_lootables.py` lists them.
They are added (`ContainerRowCount` 175 of 587, `tools/objects_containers.py`); armor stands and
bookshelves loop the chest cue.

## Shrine and well loops (2026-10-12)

A tester noticed shrines and wells only through the scanner. Every one the player can still use
(objects.txt SubClass shrine or well; a used one is no longer selectable) loops while in reach:
`exploration/shrine.ogg` and `exploration/well.ogg`, the two nearest at most (`MaxShrineVoices`),
behind `shrine-sounds` (Ctrl+S, on). Both are Diablo IV recordings chosen without listening: the
shrines' ambience cut to 12 s with 0.3 s fades, and the healing well's activation padded to 1.8 s.
The user may swap either file; a water ambience was too sparse to pass the loudness test.

## Gold and potions from the Last Epoch mod (2026-10-12)

The tester preferred the Last Epoch mod's coins and glass. `last-epoch-sounds` (Ctrl+S, off)
swaps them in: its gold loop and the game's four potion pickups, one per kind, from the
ItemTypes ancestry (`rpot` before `hpot`/`mpot`, its parents; other potions and elixirs share the
fourth). Sources: `LeAccess/Sounds/Loops/gold.wav` and `LastEpoch_sonidos/candidatos/04 pociones`;
the gold and the other potion are short clinks boosted 18 dB through the limiter.

## Low-quality names (2026-10-12)

A tester: "low quality" hid the game's four grades. A quality-1 item on the ground is now named
as the game names it: its LowQualityItems row is the int32 at ItemData+0x34 (ITEMS_GetFileIndex
0x36CE70, the field unique and set rows share), the row's prefix string id at +0x20 of a 0x22
row at bank +0x1688 (count +0x1690, getter 0x3141C0), composed with the game's own
`LowqualityFormat` through `MonsterPlate::Compose` ("Crude Short Sword"; "Espada corta basta"
where the format puts the noun first). Rows in vanilla: Crude, Cracked, Damaged, Low Quality.
Found offline in the name builder 0x1E1D80 (the quality switch, then 0x36CE70 and 0x3141C0);
both functions are fingerprinted in `LowQualityChecks`, matched against the live dump, and a
mismatch only keeps the old "low quality" word. Inventory headlines already read the game's own
tooltip name; the fallback names of arrivals and held items still use the word. Unheard live.

## Gold amounts (2026-10-12)

A pile is named without its amount on the way past; the amount is said once it is picked up
("Picked up 37 gold", `GroundItems::Purse`), from a rise of the gold stat during play only, so a
sale, a withdrawal or a reward in a menu says nothing.

## Door loops

Requested 2026-09-25: a door that is still shut loops a cue by what it is made
of, at its position, through the same mixer (up to `MaxDoorVoices` = 2 nearest,
claimed after exits and before containers). An opened door is silent: the loop
is set only while the object is in mode NU (`ObjectNeutral`), so the next unit
refresh (up to 500 ms) drops it. The radar's passage probe reads the game's own
DOOR collision bit, which opening clears, so the way through opens there too.
Switch: `[doors] enabled` in `radar.toml`, `access-items doors-on|doors-off`.

**Which cue** is the sound the game itself plays when that door opens, not its
class name. objects.txt has no material column; the client keeps a per-class
object sound table in `.rdata`: 8-byte pointers indexed by objects.txt `*ID`
(disk RVA `0x1606810` in D2R 3.3.93787, D2R.exe SHA-1 `7c742eac…`), each to a
12-dword record whose first eight dwords are one sounds.txt `*Index` per object
mode (close in NU, open in OP or ON). The index was confirmed on non-door rows
(Casket5, LargeChest, Barrel, TownPortal, Fountain, torches all line up).
`tools/gen_door_sounds.py` reads it from the installed exe and writes
`src/generated/door_sounds.h`; it fails instead of guessing if a patch moves the
table. The reader of the table in live code was not located (the rebuilt
`.rdata` layout differs there), which is why the mapping is generated offline
rather than read at runtime.

| Game sound | Door classes | Loop |
|---|---|---|
| `object_door_wood_open` | DoorWooden*, DoorMonasteryDoubleRight, DoorSmallCathedral*, DockDoor, DesertWoodenDoor* | `wooden door` |
| `object_door_gate_open` | DoorCourtyardDouble*, DoorCathedralDouble, AndarielDoor | `wooden door` (great wooden double doors) |
| `object_door_metal_open` | DoorGate* (the Act 1 barred gates), DesertIronGrateDoor*, DesertWoodenGrateDoor* | `metal door` |
| `object_door_stone` | TombDoor*, TombDoor*2 | `stone door` |
| `object_door_slime` | SlimeDoor1/2 | `slime door` |

Other objects with door-type sounds but `IsDoor` = 0 (Trapdoor, HiddenDoor2,
Duriel's door, the Arcane and Hell gates, the Act 5 pen door, the Ancients'
gateway, the Summit door) are not doors to the plugin and get no door loop.

## Quest item loops

Added 2026-09-23 using the supplied `quest item.ogg`. Ground quest items take
this sound before quality or consumable classification, including the Scroll
of Inifuss. Classification reads the live item row's nonzero `quest` byte or
its primary type's `ques` ancestry. The flag covers quest weapons with ordinary
weapon types, including the Malus, Horadric Staff, Gidbinn and Khalim's weapons.
`python tools/items_quest.py` verifies code, primary type and quest flag against
the installed text/binary tables: all 692 rows match on D2R 3.3.93787, with 26
nonzero quest flags and 28 additional primary `ques` rows. Extract the inputs
listed in that script before rechecking after a game update.
Interactable sources use `Generated::QuestItemObject`, generated
alongside the existing lootable catalog from the installed `objects.txt`:
InifussTree (30), Malus (108), LamEsensTome (193), the Horadric Cube, Scroll and
Staff chests (354–356), and Khalim's Heart, Eye and Brain chests (405–407).
Their interaction handlers are 12, 21, 28, 39–41 and 57–59. All nine rows are
selectable only in neutral mode, so the native current-mode gate silences
them after use. A quest chest uses only the quest sound, without a chest loop.

The user confirmed the tree cue works but found its 2.812-second repetition and
30-subtile reach too hard to locate while exploring. The build now trims/pads
only the quest WAV to 700 ms, preserving playback speed, with a 5 ms attack and
20 ms release to avoid clicks. The original OGG is unchanged. Existing rear
pitch makes repetition about 745 ms behind the player. Quest sources reach
45 subtiles and use a linear distance fade, retaining one-third of near gain
at the old 30-subtile boundary. Ordinary loot retains its existing range and
attenuation; the shared near gain and pile cap remain in force.

Cairn Stones (classes 17–21, installed `objects.txt` OperateFn 9) all qualify
for the cue while neutral, whether or not the scroll has been opened. Activated
stones stop sounding. As with other quest sources, the two nearest eligible
sources receive voices. The scan, catalogue and beacon identify the fixed symbol:
upward drop, one, downward drop, two, or zero. Original HUD labels remain
available for visibility matching.

Opening the deciphered Scroll of Inifuss reads its full sequence of symbols
once, waiting for the native packet if necessary. Closing and reopening repeats
it. The verified client quest-4 packet supplies five class indices in activation
order; session changes clear the cache and all five values must form a permutation.
That sequence is used only for reading the open scroll. The player compares it
with the stones and chooses which one to touch, including making mistakes.
There is no next-stone cue or solution rank in world labels. The live scroll
numbers, packet consumer and server interaction were cross-checked in D2R
3.3.93787; see [native evidence](cairn-stones-re.md).

These cues share **Ctrl+L** / `access-items sound-on|sound-off` with item audio.
Disabling chest or exit sounds does not suppress quest sources. The nearest
two quest sources reserve voices before exits, containers and ordinary loot;
the eight-voice and summed-gain limits still apply. Object and item identities
remain distinct even if their numeric unit IDs coincide. Ground pickup is
refreshed at 50 ms, fixed quest objects at 500 ms. Dialogue, pause, focus and
lifecycle suspension use the existing shared gates.

The quest range is 45 collision subtiles in resident nearby rooms. No distant
presets or undiscovered map positions are requested. Classification identifies
quest-related objects, not whether the player's current quest needs them: no
player quest flags are inferred. Quest mechanisms that hand over no item (the Orb, the
seals, the Hellforge, the Ancients' altar and others) use the same cue in their
quest-step modes: see [quest objects](quest-objects.md).

**Quest tomes** (requested 2026-09-25): the Moldy Tome (TowerTome, OperateFn 6)
and Horazon's journal (ArcaneTome, OperateFn 42), the books the automap draws
with the same tome cell as Lam Esen's Tome, loop `loot/scroll.ogg` in the quest
share while the object is in its unread mode NU; both stay selectable once read,
so the mode, not the selection, ends the loop. `Generated::TomeObject`, from
`tools/gen_lootables.py` (`TOME_FNS`). Lam Esen's Tome hands over an item and
keeps the quest cue.

Validation covers native Inifuss selection/used modes, unavailable translation,
quest classification before quality, landing and pickup, source priority and
identity collisions, toggles, UI suspension, lifecycle reset, and the actual
converted recording's PCM level and stereo positioning. Cairn tests cover all
120 valid orders, invalid permutations, session reset races, cues independent
of scroll order, fixed symbol names and reading only the open scroll. The user
confirmed the tree cue and its faster, farther revision. Symbol and scroll
reading still need controller/NVDA listening after installation.

## Assets and build

Edit the OGG files under `sounds/item loop sounds/loot/` and
`sounds/item loop sounds/exploration/` (the table above lists them;
`cmake/GroundItems.cmake` holds the authoritative set, and
`GroundItems::SoundFiles` the folder each is loaded from), then build with the game
closed. `cmake/GroundItems.cmake` uses the existing **FFmpeg command line tool**
to create mono, 48 kHz, PCM16 WAV files in the build directory. FFmpeg is a build
requirement on `PATH`; there is no decoder DLL or additional runtime library to
install. Playback uses Windows XAudio2/X3DAudio. Asset-only rebuilds also deploy.

The build embeds each WAV in the plugin DLL as an RCDATA resource named
`FOLDER_STEM` (uppercase, spaces as underscores, e.g. `LOOT_NORMAL_EQUIPMENT`) and
the plugin reads them from its own module, so players cannot swap them by
replacing files (a resource editor still could). Nothing is installed under
`d2rloader/plugins/d2r-access/sounds/`; an old folder there is unused and can be
deleted. The item-audio suite opens the built DLL and checks every sound in it.
It keeps the user's OGG sources unchanged.
Do not commit the generated WAV files or build binaries.

## Native evidence

Source: the existing analyzed `re/ghidra/D2R`, program `D2R_live.exe`, backed by
the decrypted capture of the installed executable. No old-version offsets were
copied from an external map tool. The reader verifies complete native function
spans in `ground_item_layout.h` before using the layouts and copies bounded data
on the UI thread. It retains no native item/room pointers between polls.

| Fact | Source |
|---|---|
| Local player index / lookup | `0x8B2D0`, `0x9A480`; lookup accepts eight local indices |
| Unit type/class/id/mode and static item path | Item type 4; `0x266690` reads static X/Y at `Unit+0x38 -> +0x10/+0x14` |
| Player room versus item room | `0x34B440`: dynamic path room `+0x20`, static path room `+0` |
| Nearby rooms | `0x2EFDE0`: array/count at `Room+0/+0x40` |
| Room's actual world-unit chain | `0x2EFD90`: head `Room+0xA8`; `0x34B4A0`: next `Unit+0x160`; `0x3B24B0` traverses these for nearby item lookup |
| Different unrelated chain | `0x2EFB10` / `0x3497F0` expose `+0x30/+0x150`; these are **not** the nearby world-unit traversal |
| Data bank | `0x34A0E0`: `Unit+0x1BD`; `0x300A90` resolves its active table bank |
| Items rows | `0x314110`: pointer/count at bank `+0x15A0/+0x15A8`, stride `0x1C0` |
| Item code / localized name / primary type | `0x1807D0`, `0x9A1B0`, `0x293CC0`: row `+0x80/+0xFC/+0x12E` |
| Item quality | `0x36CF60`: DWORD at `Unit+0x10 -> +0`; enum from PluginSDK `Items::Quality` |
| Item type table | `0x293CC0`: bank `+0x1348/+0x1350`, stride `0xE8` |
| Type ancestry layout | Installed CASC `ItemTypes.bin`: four-byte Code then two uint16 Equiv indices; cross-checked against `itemtypes.txt` |
| Gold amount | `0x370C90` reads stat 14, matching `ItemStatCost.txt` `gold`; guarded read-only getter `0x2F5020(unit, stat, layer=0)` |
| Ground quest classification | Installed `misc.txt`, `weapons.txt`, `armor.txt` and their `.bin`: row `quest` byte at `+0x13A`, uniquely matched across all 692 rows (169/306/217); `+0x80` code and `0x1C0` stride cross-checked. Primary `ques` ancestry also qualifies. |
| Player death modes | Installed `plrmode.txt`: Death row 0, Dead row 17 |

The scan covers the player's resident room and its native neighbors, with finite
bounds on room/unit counts, table rows and type ancestry. It only accepts ground
item mode (3) whose static path still refers to the enumerated
room. These snapshots do not create items, collect items, change targeting, move
the player or load other rooms. Unreadable/inconsistent chains discard the
snapshot so stale positions do not persist in audio.

## Validation

Four new test suites cover classification, quest consumables, proximity/reentry,
duplicate identities, bounded piles, pause/session handling, separate toggles,
guarded native layouts, corrupt/cyclic memory, real OGG-to-WAV output and actual
X3DAudio left/right/front/rear matrices. The existing interaction-reader suite
also checks tooltip suppression and restoration. A separate local check verifies
all native fingerprints against `re/D2R.rebuilt.exe` and rejects a changed byte.
The preceding full build passed **36 suites / 2377 checks**. After lowering the
level and requiring landing, **5 affected suites / 341 checks pass**, including
the native Dropping-to-OnGround transition through the actual item tracker.
The audio suite checks
normalization, negative full-scale samples, silent input and actual per-ear RMS
after distance/panning for every supplied recording, rather than merely checking
that a file contains a nonzero sample.

Live log, 2026-09-16: **12:45:21** verified the reader and loaded all six loops;
**12:45:34** resumed on a ready player; **12:48:41** started the normal-item loop
for item 24 behind the player; **12:48:55** announced its nearby localized name
(`Saetas`). That first run exposed the grammatical `[fp]` prefix, now covered by
the name-cleaning regression. **The user reported no audible item sounds.**
The measured level defect above was then corrected. A standalone XAudio2 check
on the actual default endpoint, with its diagnostic master muted, confirmed
advancing samples and a nonzero signal using the Windows volume-meter effect;
the endpoint was unmuted at full volume. It issued no gameplay inputs or audio
capture. With the initial 0.6 gain, normal-item output at 5.4 subtiles was about -29.4 dBFS per ear
at the measured maximum RMS, compared with about -62 dBFS before the fix.

The **13:03:06** live load confirms normalization and all six loops initialized.
Each source now logs one progress record after 250 ms (samples played, queue,
gain, stereo matrix and pitch); failures include the HRESULT. These are software
checks, not confirmation of speaker output. The user subsequently confirmed
audibility and requested lower volume and silence during the falling animation.
Listening validation of that follow-up, direction, distance, rear pitch and
remaining categories is pending.

User-led check: approach a potion, scroll, gold and equipment; stop beside each;
walk away and return; collect one; open dialogue and inventory; leave/rejoin.
Use `[items]` and `[item-audio]` in the latest plugin log. No per-frame audio
logging or computer-control inputs are needed for this listening check.
