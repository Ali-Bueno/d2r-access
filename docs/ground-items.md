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
when applicable, and the native amount for gold. It reads once per approach;
moving more than six subtiles away rearms it. Up to four names are grouped per
announcement, with at least 900 ms between groups. More items remain eligible
while nearby. There is no speech backlog for items that have already disappeared.
The shared world-tooltip reader defers matching nearby names to avoid duplicates.
Inventory descriptions still use the game's complete rendered tooltip.

Names come from the active game's Items table and localization service. The
`[fs]`, `[ms]`, `[fp]`, `[mp]`, `[ns]`, `[pl]` and `[nl]` grammatical markers in
`item-names.json` are omitted. Equipment speech currently identifies the **base
item and quality**, not its complete generated affix/unique display name. It
does not reveal unidentified affixes.

| Supplied loop | Ground item category |
|---|---|
| `gold.ogg` | Gold |
| `potion.ogg` | All descendants of potion, elixir and scroll item types, except quest items |
| `normal equipment.ogg` | Inferior/normal/superior equipment and remaining miscellaneous items |
| `magic equipment.ogg` | Magic quality |
| `rare equipment.ogg` | Rare and crafted quality |
| `legendary equipment.ogg` | Unique and set quality |
| `portal.ogg` | Level exits: warps, portals, waypoints |
| `chest.ogg` | Containers you open for loot ([below](#container-loops)) |
| `quest item.ogg` | Ground quest items and unused objects that supply quest items ([below](#quest-item-loops)) |

Loops begin only once the native item mode is **OnGround (3)**. The falling
animation (**Dropping, 5**) is excluded; there is no guessed landing timer.
Up to eight nearest items have independent continuous XAudio2 voices. X3DAudio
supplies the stereo speaker matrix. Fixed screen axes derive from native
projection `0x334DB0`: right = `(dx-dy)/sqrt(2)`, front = `-(dx+dy)/sqrt(2)`.
Distance attenuates each source to silence at 30 subtiles. Rear sources smoothly
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

These cues share **Ctrl+L** / `access-items sound-on|sound-off` with item audio.
Disabling chest or exit sounds does not suppress quest sources. The nearest
two quest sources reserve voices before exits, containers and ordinary loot;
the eight-voice and summed-gain limits still apply. Object and item identities
remain distinct even if their numeric unit IDs coincide. Ground pickup is
refreshed at 50 ms, fixed quest objects at 500 ms. Dialogue, pause, focus and
lifecycle suspension use the existing shared gates.

The range remains 30 collision subtiles in resident nearby rooms. No distant
presets or undiscovered map positions are requested. Classification identifies
quest-related objects, not whether the player's current quest needs them: no
player quest flags are inferred. Other quest mechanisms (Cairn Stones, the
Hellforge, Horazon's journal, and the Ancients' altar) are outside this catalog;
some stay selectable after use and need separate verified state handling.

Validation covers native Inifuss selection/used modes, unavailable translation,
quest classification before quality, landing and pickup, source priority and
identity collisions, toggles, UI suspension, lifecycle reset, and the actual
converted recording's PCM level and stereo positioning. Live controller
listening at the tree and after collecting its scroll remains pending.
The Release build/deploy completed without compiler warnings and all 71 CTest
suites passed. Installed plugin and quest WAV SHA-256 hashes match the build.

## Assets and build

Edit the nine OGG files under `sounds/item loop sounds/` (the table above lists
them; `cmake/GroundItems.cmake` holds the authoritative set), then build with the game
closed. `cmake/GroundItems.cmake` uses the existing **FFmpeg command line tool**
to create mono, 48 kHz, PCM16 WAV files in the build directory. FFmpeg is a build
requirement on `PATH`; there is no decoder DLL or additional runtime library to
install. Playback uses Windows XAudio2/X3DAudio. Asset-only rebuilds also deploy.

The build installs the WAV files under
`d2rloader/plugins/d2r-access/sounds/item loop sounds/`. It keeps the user's OGG
sources unchanged. `item drop sounds/` is not used by this continuous-loop feature.
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
