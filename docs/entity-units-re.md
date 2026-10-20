# Monsters, NPCs and world objects — native layout (D2R 3.3.93787)

Everything the plugin needs to enumerate **monsters/NPCs (unit type 1)** and **world objects
(unit type 2)** in the player's room and its neighbours, and to resolve each one's localized name,
hostility and liveness. Same room walk as `docs/ground-items.md`; only the per-unit decoding is new.

## Method and sources

* Code evidence: the analyzed Ghidra project `re/ghidra/D2R`, program `D2R_live.exe`. All RVAs are
  **live RVAs** (`context->exeBase + rva`), Ghidra address = RVA + `0x140000000`.
* Data evidence: the installed CASC `global/excel/*.bin` records cross-checked against the shipped
  `*.txt` of the same tables, and `local/lng/strings/*.json` for string ids. Every field marked
  *verified* below is either read by decompiled code **or** proven by making the `.bin` bytes agree
  with the `.txt` column over **all** rows (`monstats.bin` 751 rows, `monstats2.bin` 623,
  `objects.bin` 587 — each exactly `4 + rows * stride` bytes, which also proves the stride).
* Fingerprints: `{rva, size, fnv1a}` exactly like `src/ground_item_layout.h::Checks[]` — `size` is the
  Ghidra function body size, `fnv1a` is FNV-1a-64 over those bytes read at file offset == RVA in
  `re/D2R.rebuilt.exe`. Reproduced the three existing tuples (`0x9A1B0`, `0x300A90`, `0x34A0E0`)
  byte-for-byte before computing new ones.

## 1. Unit type constants at Unit+0 — verified

`0x34B9D0` is the type getter; it returns **6 for a null unit**, and `0x349250` (unit destructor)
writes `*unit = 6`, so **6 means "no/freed unit"** — reject it.

At RVA `0x34B9D0`, `FUN_14034b9d0` reads the unit type from offset zero. A null unit produces the sentinel type 6.

| value | meaning | evidence |
|---|---|---|
| 0 | player | `0x34C2C0` treats type 0 with PlrMode 0/0x11; `0x9A1B0` returns 0 for type 0 |
| **1** | **monster / NPC** | `0x9A1B0` type-1 branch reads MonStats; `0x34C2C0` uses MonMode 0/0xC |
| **2** | **object** | `0x44FAC0`: the object-type branch that sets mode 2 (door opener); `0x34AD90`/`0x34C340`/`0x34C470`/`0x34B520` all gate on unit type 2 |
| 3 | missile | `0x34DA60` excludes type 3 |
| 4 | item | already used by the plugin; `0x9A1B0` type-4 branch reads the Items row |
| 5 | tile / warp | `0x9A1B0` scans the room for a type-5 unit to name a portal (§4) |
| 6 | invalid / destroyed | `0x34B9D0` null return, `0x349250` sets it |

Counts of real comparisons against `0x34B9D0`'s result over the decompiled caller sets:
`==4` 208, `==1` 143, `==0` 98, `==5` 16, `==3` 16, `==2` 11.

## 2. Unit field getters (all verified, all pure reads)

| RVA | reads | note |
|---|---|---|
| `0x34B9D0` | the unsigned 32-bit value read at Unit offset 0x00 | type |
| `0x349860` | the unsigned 32-bit value read at Unit offset 0x04 | class id (row index into the type's table) |
| `0x34A330` | the unsigned 32-bit value read at Unit offset 0x08 | unit id |
| `0x34AB60` | the unsigned 32-bit value read at Unit offset 0x0C | mode |
| `0x34A500` | the pointer read at Unit offset 0x10 | type-specific data (MonsterData / ObjectData / ItemData) |
| `0x34A0E0` | the unsigned 8-bit value read at Unit offset 0x1BD | table bank (already in `Checks[]`) |
| `0x34B4A0` | the pointer read at Unit offset 0x160 | next unit in room list (already in `Checks[]`) |
| `0x34FBA0` | the bitwise intersection of the right shift of the unsigned 32-bit value read at Unit offset 0x124 and 9 and 1 | see §5, player-owned minion |

**Unit flag words: `Unit+0x124` and `Unit+0x128`, both uint32.** `0x349Cxx`–`0x34FBxx` is a dense
bank of one-bit getters/setters over them (e.g. `0x34E190`/`0x34E140` are the generic
set/clear-by-mask entry points). Only the bits named in this document are established.

## 3. Liveness — verified

The liveness routine at RVA `0x34C2C0` checks the forced-dead flag at Unit offset `0x124` (mask `0x10000`). Player mode zero or `0x11` and monster mode zero or `0x0C` count as dead/dying. Other unit types use the `0x4000000` flag at offset `0x128`.

Mode tables are authoritative from CASC `global/excel/MonMode.txt` / `PlrMode.txt` / `ObjMode.txt`:

* **MonMode**: 0 death, 1 neutral, 2 walk, 3 gethit, 4 attack1, 5 attack2, 6 block, 7 cast,
  8–11 skill1–4, **12 dead**, 13 knockback, 14 sequence, 15 run.
  Cross-checked independently: `monstats2` record `+0xF0/+0xF1` is a 16-bit "mode available" mask
  whose bits correlate 1:1 with the `mDT mNU mWL mGH mA1 mA2 mBL mSC | mS1 mS2 mS3 mS4 mDD mKB mSQ mRN`
  columns, in exactly that order.
* **PlrMode**: 0 Death, 17 Dead (matches the constants already in `ground_item_layout.h`).
* **ObjMode**: 0 Neutral, 1 Operating, 2 Opened, 3–7 Special1–5.

So: **monster alive ⇔ mode ∉ {0, 12} and `!(Unit+0x124 & 0x10000)`**; for objects use
`!(Unit+0x128 & 0x4000000)`.

## 4. Names

### 4.1 The native resolver `0x9A1B0` is a general UNITS_GetName — verified

It is already in `Checks[]` (the plugin cites it as the *item* name resolver); it actually handles
players, monsters, objects and items:

`FUN_14009a1b0` (RVA `0x9A1B0`, Windows x64 calling convention) returns UTF-8 name text. Players have no result on this path. Monsters first use the composed-name builder at `0x985A0`, then fall back to the MonStats row at `0x976E0` and its name index at `0x36`. Items use the record resolver at `0x314110` and name index at `0xFC`; object handling is described in section 4.3. Numeric localization uses `0x5F4A50`.

Calling convention: `__fastcall`, one argument (the unit pointer), returns `char*` — its consumer
`0x14F3400` does the call to FUN_14009A1B0 followed by an empty-name test, i.e. **UTF-8/ANSI
`char*`, not `wchar_t*`**, and non-null for types 1/2/4. It is safe thread-wise only on the game/UI
thread, **but it is not side-effect free**: for monsters it reaches `0x985A0`, which frees and
re-allocates `MonsterData+0x40` through the game allocator. Recommendation: **do not call it**; use
the data paths below, which is also the style the plugin already uses for items.

Localization primitives (the plugin's own `Localization::Text` goes through D2RCore's
`LocalizationService`, so these are only needed as evidence):
RVA 0x5F4A50, numeric text lookup by 16-bit ID, RVA 0x5F4AF0, numeric text lookup with fallback,
RVA 0x5F4B90, text lookup by key pointer and byte length — all three are hash lookups returning
the character pointer read at node offset 0x18.

### 4.2 Monster name — data path

**Base name (verified end to end).** `MonStats` row = `0x976E0(bank, classId)`, i.e.
`bank+0xF58` (ptr) / `bank+0xF60` (count), stride **0x1FC**; the name is the **uint16 string id at
row+0x36**. Verified in code (`0x9A1B0`, `0xC0420`, `0xCEA50`, `0x16A390` all do
the result of Localize with arguments the unsigned 16-bit value read at row offset 0x36) *and* against the data: over all 751 rows, `+0x36 == 0` exactly when
`monstats.txt NameStr` is empty, the key→id map is injective, and the ids resolve in
`local/lng/strings`: row 0 `skeleton1` → 3011 `"Skeleton"`, row 1 `skeleton2` → 3013 = `0xBC5`
`"Returned"`, row 148 `akara` → 2892 `"Akara"`.

**Unique / superunique / champion name (verified from code).** `0x985A0` composes it from
**MonsterData = the pointer read at Unit offset 0x10**:

The monster name builder at RVA `0x985A0` uses MonsterData at Unit offset `0x10`. Its indices at `0x32` and `0x34` select plain-name versus pattern formatting, with `-1` indicating an absent index. The localized pattern and name components at `0x34`, `0x36` and `0x38` feed formatter `0x3ADCA0`; the allocated composed UTF-8 result is cached at MonsterData offset `0x40`. See the adjacent localization and layout evidence.

| MonsterData field | type | meaning | confidence |
|---|---|---|---|
| `+0x32` | int16 | string id of the **name pattern** (or the whole name when `+0x34 == -1`); `-1` = none | verified |
| `+0x34` | int16 | string id, pattern argument 1; `-1` = none (⇒ `+0x32` used alone) | verified |
| `+0x36` | int16 | string id, pattern argument 2; `-1` = none | verified |
| `+0x38` | int16 | string id, pattern argument 3 (5th stack argument of `0x3ADCA0`) | inferred |
| `+0x40` | char* | **cached composed name**, allocated/replaced by `0x985A0` | verified |

`0x3ADCA0` is a pattern expander (it copies each non-null argument into a 0x400-byte slot and scans
the pattern for `':'` markers), *not* a plain concatenation. Since only the game populates `+0x40`
(lazily, the first time it resolves that monster's name), the safe plugin recipe is: read `+0x40`
and use it when it is a readable non-empty C string; otherwise fall back to
`Text(+0x32)`+`Text(+0x34)`+`Text(+0x36)`+`Text(+0x38)` joined with spaces; otherwise the MonStats
base name. Superunique names live in `SuperUniques.bin` (66 rows, stride 0x34) as a **uint16 string
id at record+0x02** (ids 0x0B35… consecutive) — that is what ends up in `MonsterData+0x32`
(*inferred*).

**Placeholder name (fixed 2026-09-16, verified against monstats.txt).** 42 rows carry no name of
their own and share `NameStr` id 5382, which `local/lng/strings/ui.json` resolves to "an evil force" /
"una fuerza maligna" — a label the game itself never puts on a nameplate: the camp rogue scouts
`rogue1`/`rogue3`, the Den's `evilhole1-5` spawners, cow/horse/pig/camel, bat/rat/bird,
`hellmeteor`, `invisospawner`, `seventombs`, and Act 2/3 town extras. Flags cannot separate them from
real units — `act2guard1`/`rogue1`/`rogue3` are `npc`+`killable` yet unnamed, while real NPCs (Akara,
Charsi…) are `npc`+`interact`. `ReadMonster` now resolves the key once via `Localization::Text`
(`EntityLayout::PlaceholderNameKey` = `"dummy"`) and drops any unit whose resolved display name
matches it, so one guard covers every announce path.

`MonStats2` row = `0x975B0(bank, classId)` — it reads `monstats[0x4C]` (int16 `MonStatsEx` index)
and indexes `bank+0xF98` / count `bank+0xFA0`, stride **0x128**. Needed only for the animation-mode
mask (§3) and display metrics.

### 4.3 Object name — verified

**ObjectsTxt record = the pointer read at the pointer read at Unit offset 0x10**, i.e. the first qword of ObjectData
(getter `0x34AD90`). Independently, `0xC0420` gets the same record from the class id with
**`0x38FD00(bank, classId)` = `bank+0x1528` (ptr) / `bank+0x1530` (count), stride 0x168**.

`0x9A1B0`'s object branch, with every flag byte verified against `objects.txt` over all 587 rows:

The object branch of `FUN_14009a1b0` uses the object record and current mode. An opened OpenWarp follows a type-5 warp in the current or neighboring room to resolver `0x9F2A0`. Non-door shrines use their linked-record name index at `0x12`; ordinary objects localize the key at record offset `0x42`. A negative ObjectData state in the special record case selects label `0xCBE`. Doors select the native state labels `0xC99`, `0xC9B`, `0xC9C` or `0xC9D` according to mode. The controlling record fields are `0x163`, `0xFA`, `0x12F` and shrine classifier `0x34C470`.

| ObjectsTxt field | meaning | how verified |
|---|---|---|
| `+0x42` | NUL-terminated ASCII **localization key** = `objects.txt Name` | read from the bin: `"Casket"`, `"Shrine"`, `"Barrel"`, `"TowerTome"`, `"Portal"`, `"Duriel's Lair"` |
| `+0x85 … +0x8C` | `Selectable0 … Selectable7` (one byte per ObjMode) | byte-exact over all rows |
| `+0xFA` bit0 | `IsDoor` (25 rows) | byte-exact |
| `+0xFB` bit0 | `BlocksVis` (21 rows) | byte-exact |
| `+0x110` bit0 | `Draw` | byte-exact |
| `+0x127` bit0 | "is shrine" (92 rows — every row whose name key is `Shrine`/well) | bit pattern + name-key inspection (*inferred* name, verified bit) |
| `+0x12F` bit0 | `Lockable` (59 rows) | byte-exact |
| `+0x131` / `+0x133` | `Restore` / `Sync` | byte-exact |
| `+0x15D` / `+0x15E` | `PopulateFn` / `OperateFn` | byte-exact |
| `+0x15E` values | `OperateFn` is the **index of the interaction handler**. Containers: 1 caskets/sarcophagi, 3 urns/jars/baskets, 4 chests, 5 and 7 barrels, 14 crates/coffins/corpses/logs, 30 exploding chest, 39-41 and 57-59 act-specific chests, 68 evil urn. With "selectable while shut, not once open" that is **165 rows, 78 distinct keys**. `PopulateFn` is *not* the container signal: value 3 alone covers 261 rows including beds, candles and banners. | grouped the rows of the extracted `objects.txt` by value and read the `Name` keys per group; `tools/objects_containers.py` reproduces it and re-proves every offset in this table against its named column on all 587 rows |
| `+0x163` bit0 | `OpenWarp` (4 rows) | byte-exact |
| `+0x164` | uint16 `AutoMap` | exact |

String ids resolved from `local/lng/strings/objects.json`: `0xC99 "Closed Door"`,
`0xC9B "Open Door"`, `0xC9C "Blocked Door"`, `0xC9D "Locked Door"`, `0xCBE "Locked Chest"`.
The shrine link record is `shrines.bin` (23 rows, stride 0x1C) and `+0x12` is its name string id —
row 0 holds 10809 = `0x2A39`, which `shrines.json` resolves to `"Null Shrine"` (**verified**).

**Interactable / visible.** Use `Selectable[mode]` (`ObjectsTxt + 0x85 + mode`, getter-style read;
`0x34BBE0` uses the same `rec + 0xE0 + mode` idiom for a different per-mode array). That is the
game's own "can this object be targeted in its current animation mode" bit: a chest reads selectable
in Neutral(0) and not in Opened(2), a door in its closed modes, etc. Doors additionally give their
state through the mode → name mapping above. Portals/waypoints: `OpenWarp` (`+0x163`) objects in mode
2 (Opened) name themselves after their **destination level** via `0x9F2A0`:
levelId obtained from native resolver 0x2EFE50 using bank and room → `0x32C4A0(bank, levelId)` (`bank+0x1460` / count `bank+0x1468`,
stride 0x18C) → ASCII key at `row+0x125` → `0x5F4B90`. (Note `Levels.bin`'s on-disk stride is 0x194,
so the in-memory Levels record is *not* the bin record — trust `+0x125` from the code, not the bin.)

## 5. NPC vs hostile, and player-owned minions

**Primary signal (verified from the game's data): `MonStats + 0x87` = `Align`.**
Distribution over 751 rows: `0` → 630 rows (skeletons, zombies, …), `1` → 117 rows (cain1, gheed,
akara, kashya, rat, rogue1, …), `2` → 4 rows (cow, camel, horse, pig). Byte-exact match to the
`monstats.txt Align` column. So **0 = hostile, 1 = friendly/NPC, 2 = neutral/critter**.

**MonStats flag bytes (every bit byte-exact against its `monstats.txt` column, all 751 rows):**

| byte | bit0 | bit1 | bit2 | bit3 | bit4 | bit5 | bit6 | bit7 |
|---|---|---|---|---|---|---|---|---|
| `+0x3C` | isSpawn | isMelee | noRatio | opendoors | SetBoss | BossXfer | **boss** | **primeevil** |
| `+0x3D` | **npc** | **interact** | inTown | lUndead | hUndead | demon | flying | **killable** |
| `+0x3E` | switchai | nomultishot | neverCount | petIgnore | deathDmg | genericSpawn | zoo | placespawn |
| `+0x3F` | inventory | enabled | – | noAura | rangedtype | canNotUseTwoHandedItems | CannotDesecrate | CannotHerald |
| `+0x40` | nopreventmonsterheal | | | | | | | |

Also verified: `+0x36` NameStr id, `+0x4C` int16 MonStatsEx index, `+0xFC` Level.

### Enemy cue traits (2026-09-22)

`ReadMonster` attaches traits only after its hostile, alive, killable, non-minion filter.
The class-level `boss` and `primeevil` bits (`MonStats+0x3C`, bits 6 and 7) mark
important enemies, while `rangedtype` (`MonStats+0x3F`, bit 4) marks ranged enemies.
These offsets/bit positions match every row of the extracted
`game-data/global/excel/monstats.txt`; for example, `cr_archer1` and
`sk_archer1` carry `rangedtype=1`. Neither trait uses translated names.

The same game's `AI` column identifies the resurrecting `FallenShaman` and
`FetishShaman` families. Their `*hcIdx` class IDs are 58–62, 645–647 and
278–282, 662–664 respectively. These are class IDs, not source row numbers:
rows after the inserted entry at source row 645 have a one-row offset.

Per-instance elite status follows the game's own special-monster predicate.
The live-image Ghidra function `0x38E0E0` reads the `uint16` at
`MonsterData+0x1A` and returns true for its low nibble (`flags & 0x000F`).
The native monster health-plate builder `0xCEA50` calls that predicate with
argument `0` to select special-monster decoration. An optional `0x0010` bit
is included only when the predicate's second argument is nonzero; it is
therefore excluded here, as in the health plate. Other native consumers
`0x1A5C10` and `0x1A5CD0` test bits `0x0004` and `0x0008` individually for
plate styling. This is a verified native classification, independent of names
and locale. The exact game label for each low-nibble bit is not yet mapped;
a live comparison with champion, unique and superunique units remains useful
for refining their individual cue timbres, but is not needed for the shared
elite cue.

**The game's own rule for "show the monster health plate".** `0xC0420` (hover-name renderer) picks
between `0xCEA50` (the plate *with* a health bar, which reads `0x2F5020` stats) and the plain
`0x9A1B0` hover name:

The hover-name renderer between VAs `0x140C0C42` and `0x140C0C5D` reads the MonStats byte at `0x3D`. The relevant bit must be clear in global `0x1D996D4`, set in `0x1D996EC`, and accompanied by a nonzero name word at MonStats offset `0x36`; otherwise the renderer uses the plain name. These are native trait gates, not a new mod classification.

The two masks are **global variables**, not immediates. In the disk image `.rdata` at those addresses
holds an unrelated monotonic id table, so their live values could not be read offline. They sit in a
contiguous 8-dword array `0x1D996D0 + 4k` whose reference counts fall monotonically
(128, 97, 89, 47, 35, 34, 33, 32), which is the signature of `mask[k] = 1 << k`. Under that reading
`0x1D996D4 = 0x02 (interact)` and `0x1D996EC = 0x80 (killable)`, giving
`!interact && killable && NameStr != 0` → health plate — semantically exactly right for
"hostile monster vs town NPC". **Confidence: inferred** (see §7 for the live check).

Earlier in the same function, before that test: `0x34FBA0` → `Unit+0x124 bit9 (0x200)`. When set on a
monster, `0xC0420` shows a plain name and no health bar, and `0x3473D0` treats it specially
(`*param_1 == 1 && (param_1[0x49] & 0x200) != 0`). The bit is set by `0x34E2F0(unit, 1)` from a
creation path guarded by `GetType(owner) == 0` (the owner is a **player**), so
**`Unit+0x124 & 0x200` = player-owned minion / hireling / summon** — *inferred (strong)*. Note that
`Align` cannot express this: a raised skeleton still has `Align == 0`.

**Corrected 2026-10-14** ([summons and corpses](summons-and-corpses-re.md) §3): on the client the
bit is set for the **hireling only** (`0x1A28A0`, reached for pet type 7), and every summon has
`Align = 1`. A player's pets are told by the game's pet list and its second ownership rule.

Two hard-coded monster classes are special-cased in `0xC0420`: `0x1A1`/`0x1A2` = `shadowwarrior` /
`shadowmaster` (named from MonStats), and `0x164` = `dopplezon`, named after the owning player.

## 6. Table bank map

Resolver `0x300A90(bank)` returns the bank pointer in table 0x2A9A580 at 0x10-byte stride; every table accessor is
`row = base[off] + index * stride` with `count` at `off+8`. Strides were matched against the
extracted `.bin` files (`4 + rows*stride == filesize`), which names them:

| bank offset | count | stride | table | accessor |
|---|---|---|---|---|
| `+0xF58` | `+0xF60` | `0x1FC` | **monstats** | `0x976E0` |
| `+0xF98` | `+0xFA0` | `0x128` | **monstats2 / MonStatsEx** | `0x975B0` |
| `+0x1348` | `+0x1350` | `0xE8` | ItemTypes (already used) | `0x36A4B0`, `0x293CC0` |
| `+0x1460` | `+0x1468` | `0x18C` | Levels (in-memory form) | `0x32C4A0` |
| `+0x14B8` | | `0x1AC` | LvlPrest | `0x32C360` |
| `+0x1528` | `+0x1530` | `0x168` | **objects** | `0x38FD00` |
| `+0x15A0` | `+0x15A8` | `0x1C0` | Items (already used) | `0x314110` |
| `+0x11B0` | | `0x2EC` | skills | `0x97790` |
| `+0x1258` | | `0x144` | ItemStatCost | `0x2F4190` |
| `+0x1890` | | `0x60` | LvlWarp | `0x32C980` |
| `+0x1878` | | `0x781` | LvlTypes | `0x32C300` |

## 7. What could not be established offline

1. **The two nameplate masks** at `0x1D996D4` / `0x1D996EC` (§5). Live check: log
   the unsigned 32-bit value read at the sum of the sum of exeBase and 0x1D996D0 and the product of 4 and k for k = 0..7 once at plugin init; expect
   `{1,2,4,8,0x10,0x20,0x40,0x80}`.
2. **A dedicated "not rendered / hidden" unit flag.** `Unit+0x124 bit16` forces dead (§3) and
   `MonStats.enabled` (`+0x3F` bit1) disables a row entirely, but no per-instance visibility bit was
   identified. Live check: for each monster in the room, log `Unit+0x124` and `Unit+0x128` next to
   its name and mode, in a zone with known off-screen/ambush monsters, and diff the words.
3. **Individual labels of `MonsterData+0x1A` low-nibble rarity bits** are not
   yet mapped; `0x38E0E0(unit, 0)` establishes their aggregate special-monster
   predicate. Live check: compare this field for a normal, champion, unique and
   superunique monster before giving each a distinct cue.
4. **Which of `MonsterData+0x32/0x34/0x36/0x38` carries which affix** for a champion (the pattern
   semantics of `0x3ADCA0`). Live check: log the four int16 ids plus
   `Localization::Text(id)` for each, and `MonsterData+0x40`, for one champion and one unique.
5. **`ObjectData+0x08` and `+0x10`** — `+0x08` is the byte whose sign selects `"Locked Chest"`, and
   `+0x10` points at the `shrines.bin` row for shrine objects; neither was traced to a writer. Live
   check: log `ObjectData+0x08` for a locked and an unlocked chest, and
   the 16-bit name index at offset 0x12 in the linked record reached through ObjectData offset 0x10 for a known shrine.
6. **The in-memory Levels record** (stride 0x18C vs the 0x194 bin record): `+0x125` is read as an
   ASCII key by `0x9F2A0`, but the field could not be confirmed against the bin. Live check: open a
   town portal and log the key string at `row+0x125`.

## 8. Fingerprints

`{rva, size, fnv1a}`, same production method as `ground_item_layout.h::Checks[]` (the last three
rows are the already-shipped tuples, reproduced here as a self-check).

Fingerprint evidence for `LayoutCheck::Hash`; each row records RVA, byte span, hash and purpose.

| RVA | Bytes | Hash | Purpose / source locator |
|---|---:|---|---|
| 0x34B9D0 | 45 | 0xA892FC2D16626027 | UNIT_GetType        (Unit+0) |
| 0x349860 | 46 | 0x27DC60F1541D2802 | UNIT_GetClassId     (Unit+4) |
| 0x34A330 | 46 | 0x7626C6CCA52BFF23 | UNIT_GetId          (Unit+8) |
| 0x34AB60 | 43 | 0xBCDE6EC19007BD7A | UNIT_GetMode        (Unit+0xC) |
| 0x34A500 | 53 | 0x8D3BD69276CB0C27 | UNIT_GetData        (Unit+0x10) |
| 0x34C2C0 | 104 | 0xBF5FE9280CD1598F | UNIT_IsDeadOrDying |
| 0x34FBA0 | 52 | 0x7BAC2EF8F8795A3D | Unit+0x124 bit9 (player-owned minion) |
| 0x34AD90 | 81 | 0x2BADCEB575F7FFEF | OBJECT_GetRecord |
| 0x34B520 | 82 | 0x92E9C1E7FFFA1F03 | OBJECT_GetLinkedRecord (shrines row) |
| 0x34AD40 | 72 | 0xFA46CE6C937348C4 | ObjectData+8 byte |
| 0x34C340 | 66 | 0x63746874D9F78DA0 | ObjectsTxt IsDoor byte |
| 0x34C470 | 30 | 0x2E439D67061BDE9D | OBJECT_IsShrine (rec+0x127 bit0) |
| 0x985A0 | 521 | 0x69116D401817738C | MONSTER composed-name builder |
| 0x976E0 | 169 | 0x262A2516AE7AEA21 | MonStats row      (bank+0xF58, 0x1FC) |
| 0x975B0 | 300 | 0x7E685F6F31B35BD2 | MonStats2 row     (bank+0xF98, 0x128) |
| 0x38FD00 | 132 | 0x8E99DD2AE136F14F | Objects row       (bank+0x1528, 0x168) |
| 0x32C4A0 | 246 | 0x6974DAC5D6D6B9B3 | Levels row        (bank+0x1460, 0x18C) |
| 0x9F2A0 | 203 | 0xBF0DFAAA7FBA1AE5 | warp  to  level name |
| 0x5F4A50 | 150 | 0x8F484EA21C6A17E1 | Localize(id) |
| 0x5F4AF0 | 147 | 0xDC0D9F06773A7F63 | Localize(id, default) |
| 0x5F4B90 | 1035 | 0x89EEBB5DDB1B0BC5 | Localize(key) |
| 0x3ADCA0 | 990 | 0xB52F49BC9A292002 | name pattern formatter |
| 0xCEA50 | 5725 | 0x27A13CD2C7F4B8CA | monster plate (evidence for the hostile rule) |
| 0xC0420 | 3244 | 0xAFA4D77E1B5D8644 | hover-name renderer (evidence) |
| 0x9A1B0 | 651 | 0x52805E57958E13F6 | UNITS_GetName (already shipped) |
| 0x300A90 | 67 | 0xB8B977E3BE437A28 | table bank resolver (already shipped) |
| 0x34A0E0 | 47 | 0x17D168C4E18BD823 | Unit+0x1BD bank (already shipped) |

Only `0x300A90` / `0x34A0E0` (and the existing `0x8B2D0` / `0x9A480` / `0x2F5020`) need to be
*called*; everything else in this document is a guarded memory read, so the plugin keeps its
read-only posture. The entries for functions the plugin never calls still belong in `Checks[]`,
because they are the proof that the offsets they read are the ones documented here.

## 9. Reproducing the analysis

```sh
# xrefs / decompilation (batch several RVAs per run; the JVM start dominates)
analyzeHeadless.bat re/ghidra D2R -process D2R_live.exe -noanalysis \
  -scriptPath re/scripts -postScript D2RDecomp.java <rva-list> <out.c>
# table record layouts
build/casc-extract/casc_extract.exe extract "<game dir>" <out> monstats.bin objects.bin ObjMode.txt
```
The column↔offset proof is just: `rows = (filesize-4)/stride`, then for every candidate byte offset
and bit, compare the bin value against the `.txt` column over all rows and keep the unique match.

## 10. Corrections from the level-exit study (2026-09-16)

Established while adding the exit loops (`docs/level-exits.md`), with the same method:

- **`ObjectsTxt+0x127` is the whole `SubClass` byte**, not a shrine bit: bit0 shrine (92 rows),
  bit2 town/permanent portal, bit4 Arcane Sanctuary portal / hell gate, bit6 waypoint (16 rows),
  bit7 suppresses the label (9 rows). The label builder `0xC17E0` branches on bits 2, 6 and 7.
- **`OpenWarp` (+0x163) is exactly four rows** — Trapdoor, UnfoldingStairLeft/Right, MephistoDoor —
  so §4.3's "portals/waypoints name themselves via OpenWarp" was wrong: an *opened* OpenWarp object
  is named by finding the adjacent **type-5 warp unit** and calling `0x9F2A0` on it; portals are
  named from **`ObjectData+8` = destination level id** (closes §7 item 5), waypoints from the
  room's own level name.
- **In-memory Levels row (stride 0x18C) is the bin row shifted by −8**: `LevelName` key at
  **+0xFD**, `LevelWarp` ("To The ...") key at **+0x125** (closes §7 item 6).
- **Type-5 warp units** live in the room unit list, use the static path like objects, and carry the
  LvlWarp `Id` in `Unit+4` (compared as a byte). The destination is on the room, not the unit:
  `Room+0x18` → RoomEx, `RoomEx+0x78` warp nodes (`+0` destination RoomEx, `+8` next, `+0x10` gate,
  `+0x20` LvlWarp row), `RoomEx+0x90` → Level, `Level+0x1F8` level id.

## 11. Other players' names (2026-09-24)

The level scanner's player category reads other type-0 units in the loaded
rooms. The evidence comes from the 2026-09-23 mapped image, the one the corpse
study cites. Every fingerprint below matches that image, the Cairn
`scroll-open` image and the 2026-09-17 `D2R.text.bin`. The controls
0x34B9D0, 0x34A500, 0xCE8F0 and 0x34C2C0 reproduce their stored hashes.

- **PlayerData** is the value read at Unit offset 0x10. The getter `0x34B240` asserts the type is
  0 at `0x34B261` (the comparison of the unit type word with zero), then reads the load from Unit offset 0x10 into RAX at
  `0x34B27A`.
- **The name is at PlayerData+0.** The name getter `0x134FC0`
  (`D2Client\src\Game\Roster.cpp`) returns the PlayerData pointer itself
  (the return-value copy from RBX to RAX at `0x1350A9`) when the unit has no roster entry. With a
  roster entry it returns the roster display name at entry+0xAA, which can
  carry a platform tag. The corpse label `0xCE8F0` uses this getter.
- **The buffer is `char[0x3D]`.** `0x1C9DE0` copies the name with a 0x3D bound
  (the value 0x3D passed in R9D at `0x1C9DFE`, then `bcStrCopyN`, `0x1227A80`). The ear name
  `0x36DFA0` copies 0x3D bytes from PlayerData. The quest array begins at
  PlayerData+0x40 (`0x5A6650`). 61 = 15 × 4 + 1, which fits 15 UTF-8
  characters, so the name is read as UTF-8. That is an inference: no decoder
  was found.
- **A living player is any mode other than 0 or 17.** Those are the
  PlrMode.txt DT and DD rows, extracted from the 3.3 CASC. The liveness test
  `0x34C2C0` treats both modes as dead for type 0, and treats the ForcedDead
  flag as dead for every type.

`EntityLayout::PlayerChecks` guards `0x34B240`, `0x134FC0`, `0x1C9DE0` and
`0x5A6650` on their own. A mismatch disables only the player category.

## 12. Party roster: distant teammates (2026-09-24)

Other players' units exist on this client only near the player. For party
members elsewhere on the level, the scanner reads the client party roster, the
list the automap uses to draw its party markers. Evidence comes from the
2026-09-23 mapped image. `EntityLayout::RosterChecks` matches both Cairn
captures, `scroll-open` and `scroll-visible`.

- **List:** `RosterNext` `0x136470` returns `entry+0x148`. For a null entry it
  returns the head global, loaded at `0x13647D` (`48 8B 05`, target
  `0x2A4DB90`). The fingerprint covers that displacement. The automap walks
  the list from the head and treats the head as the local player. The reader
  instead finds the local entry by unit ID.
- **Entry:** the fields and the functions that prove them:

  | Field | Offset | Evidence |
  |---|---|---|
  | Name, `char[0x3D]` | +0 | init `0x13A220`: the value 0x3D passed in EDX, then the copy |
  | Unit ID | +0x48 | `0x1366E0` |
  | Party ID, word | +0x5A | `0x136570`, see below |
  | Level ID | +0x5C | getter `0x136440`; set by `0x139460`, packet 0x7F, table entry size 10 |
  | X / Y | +0x60 / +0x64 | getter `0x136450`; set by `0x139420` from packet 0x90 (handler `0x12DE30`, size 13: id, X, Y) |
  | Life percent | +0x4C | set by `0x139460` from packet 0x7F; getter `0x1364A0` (`docs/player-corpse-re.md`) |
  | Corpse ids | +0x78 | node list `{+0 id, +8 next}` from packet 0x8E (`docs/player-corpse-re.md`) |

  `0x136570` returns 0xFFFF unless at least two entries share the party ID.
  The two handlers sit 17 entries apart in the handler table at `0x22BC828`
  and `0x22BC9C0`, 24-byte stride. That matches opcodes 0x7F and 0x90.
- **Units:** subtiles, the same frame as unit paths. The server builder
  `0x480AD0` fills X/Y from the unit path getters (`0x341A20`/`0x341A30`). It
  sends 0x90 only when unit distance `0x324FC0` exceeds 50 (the comparison of EAX with 0x32 at
  `0x480BC8`). Its callers include the party-member loop `0x507440`, so
  positions exist only for party members. The automap draw `0xD286C` passes
  X/Y through the subtile projection `0x334E00`.
- **The automap marker rule (`0xD286C`):**
  - The local party ID is not 0xFFFF, and the entry's party ID equals it.
  - The unit is not loaded (`0x9A5D0(id, 0)` returns null).
  - The two levels share an automap layer (LevelDefs +0x08 via `0x32C200`).

  The reader applies the first two rules. It narrows the third to the same
  level ID, as the user asked. It also skips entries whose position is zero
  (never received). For "not loaded" it looks for the player unit across the
  loaded rooms of the level (a walk over each room's near-room list, same level,
  at most 256 rooms), not only the scan's own room and its neighbours. Within 50
  subtiles the roster X/Y is stale, so a unit two rooms away used to be placed
  where the friend last stood far away, even on the previous level (2026-09-26).
  The walk runs only when the roster lists someone besides the local player.
