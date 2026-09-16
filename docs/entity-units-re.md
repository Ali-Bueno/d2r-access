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

```c
// 0x34B9D0
ulonglong FUN_14034b9d0(uint *param_1) {
  if (param_1 != (uint *)0x0) return (ulonglong)*param_1;   // Unit+0 == type
  ... return 6; }
```

| value | meaning | evidence |
|---|---|---|
| 0 | player | `0x34C2C0` treats type 0 with PlrMode 0/0x11; `0x9A1B0` returns 0 for type 0 |
| **1** | **monster / NPC** | `0x9A1B0` type-1 branch reads MonStats; `0x34C2C0` uses MonMode 0/0xC |
| **2** | **object** | `0x44FAC0`: `if ((iVar1 == 2) && …) { SetMode(u,2); }` (door opener); `0x34AD90`/`0x34C340`/`0x34C470`/`0x34B520` all gate on `*param_1 == 2` |
| 3 | missile | `0x34DA60` excludes type 3 |
| 4 | item | already used by the plugin; `0x9A1B0` type-4 branch reads the Items row |
| 5 | tile / warp | `0x9A1B0` scans the room for a type-5 unit to name a portal (§4) |
| 6 | invalid / destroyed | `0x34B9D0` null return, `0x349250` sets it |

Counts of real comparisons against `0x34B9D0`'s result over the decompiled caller sets:
`==4` 208, `==1` 143, `==0` 98, `==5` 16, `==3` 16, `==2` 11.

## 2. Unit field getters (all verified, all pure reads)

| RVA | reads | note |
|---|---|---|
| `0x34B9D0` | `*(uint*)(Unit+0x00)` | type |
| `0x349860` | `*(uint*)(Unit+0x04)` | class id (row index into the type's table) |
| `0x34A330` | `*(uint*)(Unit+0x08)` | unit id |
| `0x34AB60` | `*(uint*)(Unit+0x0C)` | mode |
| `0x34A500` | `*(void**)(Unit+0x10)` | type-specific data (MonsterData / ObjectData / ItemData) |
| `0x34A0E0` | `*(byte*)(Unit+0x1BD)` | table bank (already in `Checks[]`) |
| `0x34B4A0` | `*(void**)(Unit+0x160)` | next unit in room list (already in `Checks[]`) |
| `0x34FBA0` | `(*(uint*)(Unit+0x124) >> 9) & 1` | see §5, player-owned minion |

**Unit flag words: `Unit+0x124` and `Unit+0x128`, both uint32.** `0x349Cxx`–`0x34FBxx` is a dense
bank of one-bit getters/setters over them (e.g. `0x34E190`/`0x34E140` are the generic
set/clear-by-mask entry points). Only the bits named in this document are established.

## 3. Liveness — verified

```c
// 0x34C2C0  (param_1 = int* unit; [0x49]=Unit+0x124, [0x4a]=Unit+0x128, [3]=Unit+0xC)
if ((param_1[0x49] & 0x10000U) != 0) return 1;                       // forced-dead flag
if (*param_1 == 0) { if (param_1[3] == 0) return 1; ok = param_1[3] == 0x11; }   // player
else if (*param_1 == 1) { if (param_1[3] == 0) return 1; ok = param_1[3] == 0xc; } // monster
else return (param_1[0x4a] & 0x4000000U) != 0;                       // everything else
```

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

```c
char* FUN_14009a1b0(int *unit) {           // 0x9A1B0, __fastcall, returns char* (UTF-8)
  if (unit[0] == 0) return 0;                                   // player -> nothing
  if (unit[0] == 1) { s = FUN_1400985a0(unit);                  // composed unique/champion name
                      if (strlen(s) == 0) { row = FUN_1400976e0(bank, (short)unit[1]);
                                            return FUN_1405f4a50(*(u16*)(row + 0x36)); } }
  else if (unit[0] == 4) { row = FUN_140314110(bank, GetClass(unit));
                           return FUN_1405f4a50(*(u16*)(row + 0xfc)); }   // items (known path)
  else if (unit[0] == 2) { /* objects, see 4.3 */ } }
```

Calling convention: `__fastcall`, one argument (the unit pointer), returns `char*` — its consumer
`0x14F3400` does `pcVar16 = (char *)FUN_14009a1b0(param_2); if (*pcVar16 == '\0')`, i.e. **UTF-8/ANSI
`char*`, not `wchar_t*`**, and non-null for types 1/2/4. It is safe thread-wise only on the game/UI
thread, **but it is not side-effect free**: for monsters it reaches `0x985A0`, which frees and
re-allocates `MonsterData+0x40` through the game allocator. Recommendation: **do not call it**; use
the data paths below, which is also the style the plugin already uses for items.

Localization primitives (the plugin's own `Localization::Text` goes through D2RCore's
`LocalizationService`, so these are only needed as evidence):
`0x5F4A50 = Text(uint16 id)`, `0x5F4AF0 = Text(uint16 id, char* fallback)`,
`0x5F4B90 = Text({const char* key, size_t len})` — all three are hash lookups returning
`*(char**)(node+0x18)`.

### 4.2 Monster name — data path

**Base name (verified end to end).** `MonStats` row = `0x976E0(bank, classId)`, i.e.
`bank+0xF58` (ptr) / `bank+0xF60` (count), stride **0x1FC**; the name is the **uint16 string id at
row+0x36**. Verified in code (`0x9A1B0`, `0xC0420`, `0xCEA50`, `0x16A390` all do
`Localize(*(u16*)(row + 0x36))`) *and* against the data: over all 751 rows, `+0x36 == 0` exactly when
`monstats.txt NameStr` is empty, the key→id map is injective, and the ids resolve in
`local/lng/strings`: row 0 `skeleton1` → 3011 `"Skeleton"`, row 1 `skeleton2` → 3013 = `0xBC5`
`"Returned"`, row 148 `akara` → 2892 `"Akara"`.

**Unique / superunique / champion name (verified from code).** `0x985A0` composes it from
**MonsterData = `*(void**)(Unit+0x10)`**:

```c
// 0x985A0, lVar3 = *(longlong *)(unit + 0x10)   (MonsterData)
sVar1 = *(short *)(lVar3 + 0x34);  sVar2 = *(short *)(lVar3 + 0x32);
if (sVar1 == -1) { if (sVar2 != -1) FUN_1405f4a50(sVar2); return; }       // plain name
pcVar13 = (sVar2 == -1) ? "" : FUN_1405f4af0(sVar2, "");                  // pattern
... FUN_1403adca0(local_428, pcVar13, pcVar8 /*+0x34*/, pcVar11 /*+0x36*/ /*, +0x38*/);
*(longlong *)(lVar3 + 0x40) = <allocated copy of local_428>;              // cached char*
```

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

`MonStats2` row = `0x975B0(bank, classId)` — it reads `monstats[0x4C]` (int16 `MonStatsEx` index)
and indexes `bank+0xF98` / count `bank+0xFA0`, stride **0x128**. Needed only for the animation-mode
mask (§3) and display metrics.

### 4.3 Object name — verified

**ObjectsTxt record = `*(void**)(*(void**)(Unit+0x10))`**, i.e. the first qword of ObjectData
(getter `0x34AD90`). Independently, `0xC0420` gets the same record from the class id with
**`0x38FD00(bank, classId)` = `bank+0x1528` (ptr) / `bank+0x1530` (count), stride 0x168**.

`0x9A1B0`'s object branch, with every flag byte verified against `objects.txt` over all 587 rows:

```c
if (rec[0x163] != 0 && unit[3] == 2) { /* OpenWarp && mode Opened: find the type-5 warp unit in
                                          this room or a neighbour -> FUN_14009f2a0 */ }
if (rec[0xfa] == 0) {                                   // not a door
   if (FUN_14034c470(unit)) { link = *(void**)(ObjectData + 0x10);  id = *(u16*)(link + 0x12); }
   else if (rec[0x12f] == 0 || (char)*(byte*)(ObjectData + 8) >= 0) {
        key = (char*)(rec + 0x42); return FUN_1405f4b90({key, strlen(key)}); }   // normal path
   else id = 0xcbe; }
else { id = (unit[3]-4u < 2) ? 0xc9c : unit[3] == 0 ? 0xc99 : unit[3] == 6 ? 0xc9d : 0xc9b; }
return FUN_1405f4a50(id);
```

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
`levelId = 0x2EFE50(bank, room)` → `0x32C4A0(bank, levelId)` (`bank+0x1460` / count `bank+0x1468`,
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

**The game's own rule for "show the monster health plate".** `0xC0420` (hover-name renderer) picks
between `0xCEA50` (the plate *with* a health bar, which reads `0x2F5020` stats) and the plain
`0x9A1B0` hover name:

```c
// 0x140C0C42 .. 0x140C0C5D, lVar14 = MonStats row of this monster
movzx eax, byte [r13+0x3d]
test  dword [0x1D996D4], eax   ; jne -> plain name        (must be CLEAR)
test  dword [0x1D996EC], eax   ; je  -> plain name        (must be SET)
cmp   word  [r13+0x36], 0      ; je  -> plain name        (must have a name)
```

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

Two hard-coded monster classes are special-cased in `0xC0420`: `0x1A1`/`0x1A2` = `shadowwarrior` /
`shadowmaster` (named from MonStats), and `0x164` = `dopplezon`, named after the owning player.

## 6. Table bank map

Resolver `0x300A90(bank)` returns `*(void**)(0x2A9A580 + bank*0x10)`; every table accessor is
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
   `*(uint32*)(exeBase + 0x1D996D0 + 4*k)` for k = 0..7 once at plugin init; expect
   `{1,2,4,8,0x10,0x20,0x40,0x80}`.
2. **A dedicated "not rendered / hidden" unit flag.** `Unit+0x124 bit16` forces dead (§3) and
   `MonStats.enabled` (`+0x3F` bit1) disables a row entirely, but no per-instance visibility bit was
   identified. Live check: for each monster in the room, log `Unit+0x124` and `Unit+0x128` next to
   its name and mode, in a zone with known off-screen/ambush monsters, and diff the words.
3. **`MonsterData` beyond `+0x32…+0x40`** — the champion/unique bit flags and `wUniqueNo` were not
   located; nothing in the dumps writes those fields. Live check: log the first 0x60 bytes of
   `*(void**)(Unit+0x10)` for a normal, a champion, a unique and a superunique monster.
4. **Which of `MonsterData+0x32/0x34/0x36/0x38` carries which affix** for a champion (the pattern
   semantics of `0x3ADCA0`). Live check: log the four int16 ids plus
   `Localization::Text(id)` for each, and `MonsterData+0x40`, for one champion and one unique.
5. **`ObjectData+0x08` and `+0x10`** — `+0x08` is the byte whose sign selects `"Locked Chest"`, and
   `+0x10` points at the `shrines.bin` row for shrine objects; neither was traced to a writer. Live
   check: log `ObjectData+0x08` for a locked and an unlocked chest, and
   `*(u16*)(*(void**)(ObjectData+0x10) + 0x12)` for a known shrine.
6. **The in-memory Levels record** (stride 0x18C vs the 0x194 bin record): `+0x125` is read as an
   ASCII key by `0x9F2A0`, but the field could not be confirmed against the bin. Live check: open a
   town portal and log the key string at `row+0x125`.

## 8. Fingerprints

`{rva, size, fnv1a}`, same production method as `ground_item_layout.h::Checks[]` (the last three
rows are the already-shipped tuples, reproduced here as a self-check).

```c
{0x34B9D0,   45, 0xA892FC2D16626027ULL}, // UNIT_GetType        (Unit+0)
{0x349860,   46, 0x27DC60F1541D2802ULL}, // UNIT_GetClassId     (Unit+4)
{0x34A330,   46, 0x7626C6CCA52BFF23ULL}, // UNIT_GetId          (Unit+8)
{0x34AB60,   43, 0xBCDE6EC19007BD7AULL}, // UNIT_GetMode        (Unit+0xC)
{0x34A500,   53, 0x8D3BD69276CB0C27ULL}, // UNIT_GetData        (Unit+0x10)
{0x34C2C0,  104, 0xBF5FE9280CD1598FULL}, // UNIT_IsDeadOrDying
{0x34FBA0,   52, 0x7BAC2EF8F8795A3DULL}, // Unit+0x124 bit9 (player-owned minion)
{0x34AD90,   81, 0x2BADCEB575F7FFEFULL}, // OBJECT_GetRecord
{0x34B520,   82, 0x92E9C1E7FFFA1F03ULL}, // OBJECT_GetLinkedRecord (shrines row)
{0x34AD40,   72, 0xFA46CE6C937348C4ULL}, // ObjectData+8 byte
{0x34C340,   66, 0x63746874D9F78DA0ULL}, // ObjectsTxt IsDoor byte
{0x34C470,   30, 0x2E439D67061BDE9DULL}, // OBJECT_IsShrine (rec+0x127 bit0)
{0x985A0,   521, 0x69116D401817738CULL}, // MONSTER composed-name builder
{0x976E0,   169, 0x262A2516AE7AEA21ULL}, // MonStats row      (bank+0xF58, 0x1FC)
{0x975B0,   300, 0x7E685F6F31B35BD2ULL}, // MonStats2 row     (bank+0xF98, 0x128)
{0x38FD00,  132, 0x8E99DD2AE136F14FULL}, // Objects row       (bank+0x1528, 0x168)
{0x32C4A0,  246, 0x6974DAC5D6D6B9B3ULL}, // Levels row        (bank+0x1460, 0x18C)
{0x9F2A0,   203, 0xBF0DFAAA7FBA1AE5ULL}, // warp -> level name
{0x5F4A50,  150, 0x8F484EA21C6A17E1ULL}, // Localize(id)
{0x5F4AF0,  147, 0xDC0D9F06773A7F63ULL}, // Localize(id, default)
{0x5F4B90, 1035, 0x89EEBB5DDB1B0BC5ULL}, // Localize(key)
{0x3ADCA0,  990, 0xB52F49BC9A292002ULL}, // name pattern formatter
{0xCEA50,  5725, 0x27A13CD2C7F4B8CAULL}, // monster plate (evidence for the hostile rule)
{0xC0420,  3244, 0xAFA4D77E1B5D8644ULL}, // hover-name renderer (evidence)
{0x9A1B0,   651, 0x52805E57958E13F6ULL}, // UNITS_GetName (already shipped)
{0x300A90,   67, 0xB8B977E3BE437A28ULL}, // table bank resolver (already shipped)
{0x34A0E0,   47, 0x17D168C4E18BD823ULL}, // Unit+0x1BD bank (already shipped)
```

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
