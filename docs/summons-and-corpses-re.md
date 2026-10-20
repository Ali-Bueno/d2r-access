# Summons (pets) and usable corpses — native layout (D2R 3.3.93787)

Static study, 2026-10-14. Covers the local player's summons as the HUD's pet portraits list them,
why summons are announced as NPCs, and how the game decides a monster corpse is usable.

## Method and sources

* Image: `re/D2R.rebuilt.exe` (file offset == RVA). The installed `D2R.exe` is 3.3.93787 (SHA-256
  identical to `re/D2R.exe`). That build already contains the Warlock and the Rotw table bank.
  Every fingerprint below was also computed over the plugin's own live dump
  (`<game>/d2rloader/plugins/d2r-access/D2R.text.bin`, 2026-09-26) and matches it. The shipped
  tuples `0x13ABC0`, `0x13AA50`, `0x14F2A60`, `0x34FBA0`, `0x34C2C0`, `0x97490`, `0x12DE10`
  and `0x480AD0` reproduce exactly, so the RVAs are the ones the mod already verifies.
* Code: the Ghidra program `re/ghidra` / `D2R_live.exe`, opened read-only, and capstone over the image.
* Data: CASC `global/excel/pettype.{txt,bin}`, `monstats2`, `states`, `skills`, `monstats`.
  Each offset was proven against its `.txt` column over all rows: `pettype` has 22 rows of stride
  200, `monstats2` 623 rows of 0x128, `states` 232 rows of 0x44 and `skills` 429 rows of 0x2EC.
  The Rotw-bank `pettype.txt` is saved in `game-data/global/excel/`.
* Fingerprints: `LayoutCheck::Hash` (FNV-1a 64, E8/E9 operands zeroed), whole function, size =
  contiguous extent.

**The mask table.** `0x1D996D0 + 4k` is now settled as `1 << k`. Five uses match a column that was
proven from the data:

| Mask | Byte it tests | Proven column |
|---|---|---|
| k=5 | PetType +4 | drawhp (`0x1E6560`) |
| k=2 | PetType +4 | partysend (`0x5000F0`) |
| k=4 | PetType +4 | automap (`0x13A960`) |
| k=3 / k=1 | MonStats2 +4 / +5 | isSel / isAtt (`0x1A33B0`) |
| k=7 | MonStats2 +4 | corpseSel |

This also corroborates the inferred nameplate masks in `entity-units-re.md` §5 / §7.1.

## 1. The client pet list

One singly linked list holds **every player's pets**: summons, the hireling, traps and bound
demons. The server broadcasts add/remove packets to every client in the game (`0x4FE960` and
`0x501550` go through the per-client iterator `0x490D00`, and `0x4FFF00` sends a player's whole list
to a joining client). The HUD filters on the owner field.

* **Head:** the qword global at **RVA `0x2A4DC10`**, which is null when the list is empty. It is
  0x80 past the roster head `0x2A4DB90`. `0x13B270` loads it (`48 8B 05 99 29 91 02` at
  `0x13B270`), and its fingerprint covers that displacement.
* **Next:** `node+0x30`. Nodes are pushed at the head, so the newest node comes first.
* **Node** (0x38 bytes, allocated by `0x13A5A0` / `0x13A660`):

| Off | Type | Field | Evidence |
|---|---|---|---|
| +0x00 | u16 | MonStats class id | packet +3 (`0x12DBD0`); getter `0x13AA80`; compared with PetType mclass in `0x1E6560` |
| +0x04 | i32 | PetType row | getter `0x13AB80`; 7 = hireling (`0x13ABC0(player, 7, 0)`) |
| +0x08 | u32 | pet unit id | lookup key of `0x13B270`; `0x9A5D0(id, 1)` |
| +0x0C | u32 | owner unit id | getter `0x13ABA0`, which returns -1 when the unit is not a pet |
| +0x10 | u32 | level id | packet 0x7F +8 word, `0x13B1E0` |
| +0x1C | i32 | life percent 0..100 | init 100; `0x13B1E0`; getter `0x13AA50` |
| +0x20 | i32 | dead-hireling flag | `0x13AEC0` keeps the local player's own type-7 node and sets it |
| +0x24..+0x2B | | hireling name data | type 7 only (`0x13A660`, `0x1A28A0`) |

* **Producers** run on the game thread from packet handlers:
  - Add/remove: `0x12DBD0` reads `+1` add flag, `+2` pet type byte, `+3` class word, `+5` owner,
    `+9` pet id. It calls `0x13A5A0` to add and `0x13AEC0` to remove. The layout matches classic
    0x7A; the opcode was not read from the handler table, which is empty in this image.
  - Hireling: `0x12DC00` → `0x13A660`.
  - Life: `0x12DE10` (packet 0x7F) sends a non-player update (`+1 == 0`) to `0x13B1E0`.
* **Iterating safely.** Walk from the head and keep a node when `owner == local player unit id`,
  as `0x13ABC0`, `0x13A890` and `0x13ADD0` do. Skip `+0x20 != 0` unless dead hirelings are wanted.
  The game has no count cap, so bound the walk as a cycle guard (for example 1024 nodes: 8
  players with large armies). Nodes are freed through the game allocator on removal, so read them
  in the same tick with guarded `Peek`, as the roster walk does.
* **Life.** The HUD bar is `0x13AA50(id)` = `node+0x1C` (`0x1E8750`, `0x1E7C60`).
  - Each tick, the server's `0x5000F0` sends 0x7F for every pet whose PetType has **partysend**.
    The value is `0x34A220` = life×100/max. Rows without partysend keep the initial 100: raven,
    invis, dopplezon, assassintrap, pettrap, hydra.
  - Speak life only when PetType **drawhp** is set. The HUD draws a bar only then; on counted
    icons, only when the count is 1.
  - `NativeStatus::MonsterLife` (`0xD0640` → `0x97490`) returns the same `node+0x1C` for a
    loaded pet when it is above 0, because `0x97490` asks `0x13AA50` first. The node value also
    works for pets that are not loaded.

## 2. PetType table and what the portraits show

Row accessor `0x13A4F0(bank, type)`: rows at **`bank+0x12D8`**, count at `bank+0x12E0`, stride
**200 (0xC8)**. The bank is the player's `Unit+0x1BD`, the same bank the mod already uses for
MonStats. Rows 20/21 (demon, binddemon) are filled only in the Rotw bank; in the base bank they are
empty, with name 5382 and icontype 0. Offsets proven over all 22 rows of both banks:

| Off | Column | Notes |
|---|---|---|
| +0x00 | (row index) | i32 |
| +0x04 | flags byte | bit0 warp, bit1 range, bit2 partysend, bit3 unsummon, bit4 automap, **bit5 drawhp** |
| +0x08 | pool | i32; pets of types sharing a pool are counted together (`0x13A890` with its 3rd arg set) |
| +0x0E | basemax | byte |
| +0x10 | **name** | u16 string id; 5382 (`dummy`, "an evil force") = none |
| +0x12 | **icontype** | 0 none, 1 one portrait, 2 one portrait with count, 3 one per pet |
| +0x13 + 0x20·i | baseicon, micon1..4 | char[0x20] each |
| +0xB6 + 2·i | mclass1..4 | u16 MonStats class |
| +0xC0 + 2·i | overridename1..4 | u16 string id, Rotw bank only; 5382 = none |

`group` is empty in every row, so its offset cannot be proven; no code reads it.

**HUD dispatcher `0x1E75F0`.**
1. The hireling portrait comes first (`0x1E78B0`, relation 7).
2. Then each PetType row in table order with a non-zero icontype, up to `panel+0x16C` portraits.
   - icontype 3 → `0x1E7C60`: one portrait per pet that is **loaded**. `0x13ADD0` requires
     `0x9A5D0(id, 1)`.
   - icontype 2 → `0x1E8750(panel, type, true)`: count text from `0x13A890(player, type, 0)`.
     The life bar shows only when the count is 1.
   - icontype 1 → `0x1E8750(panel, type, false)`: the first pet only.

**Portrait name.** `0x1E6560` picks i = the first k in 1..4 with `class == mclass_k` and a
non-empty `micon_k`; otherwise 0. `0x1E66C0` returns `overridename_i` (i in 1..4, value not 0 or
5382), else PetType `name` (+0x10).

Rotw rows that matter here, with the enUS name the HUD shows and the MonStats name of the class:

| Type | Row | icontype | HUD name | MonStats name |
|---|---|---|---|---|
| golem | 3 | 1 | Golem | Clay / Blood / Iron / Fire Golem |
| skeleton | 4 | 2 | Skeleton | Skeleton |
| skeletonmage | 5 | 2 | Mage | Skeletal Mage |
| revive | 6 | 2 | Revived | the revived monster's own name |
| raven | 10 | 2 | Raven | Raven (no drawhp, no partysend) |
| spiritwolf | 11 | 2 | Wolf | Spirit Wolf |
| fenris | 12 | 2 | Dire Wolf | Dire Wolf |
| totem | 13 | 1 | Spirit | Oak Sage / Heart of Wolverine / Spirit of Barbs |
| vine | 14 | 1 | Vine | Plague Poppy / Cycle of Life / Vine Creature |
| grizzly | 15 | 1 | Bear | Grizzly |
| demon | 20 | 3 | Tainted / Defiler / Goatman (overridenames) | same |
| binddemon | 21 | 1 | Demon | the bound monster's own name |

**Name source.** The MonStats name of the node class (`row+0x36`, `0x976E0`) gives the tester's
labels: "Oak Sage", "Spirit Wolf", "Skeletal Mage", "Clay Golem". Revives are the exception: their
class varies, so use the PetType name ("Revived"). The game has no plural strings; plural labels
are the mod's own text.

## 3. Summons announced as NPCs

* **`Unit+0x124` bit 9 marks only the hireling on the client.**
  - The setter `0x34E2F0` has two callers. The server's is `0x544B49`. The client's is `0x1A28A0`
    (hireling name and skills).
  - The client reaches `0x1A28A0` from `0x13A660` (the hireling packet) and from `0x98FA0`, where
    it is gated by `0x13AB80(id) == 7` at `0x99471`.
  - So `entity-units-re.md` §5's "player-owned minion / summon" is wrong for client units.
* **Summons have `Align = 1`.** Every summon row in `monstats.txt` has `Align = 1`: necroskeleton,
  necromage, golems, valkyrie, druid summons, shadow warrior/master, hydras, sentries,
  wargoatman/warbighead/warputriddefiler. §5's "a raised skeleton still has Align == 0" is wrong.
  A **bound demon** keeps its own hostile class (`Align = 0`).
* **The game's ownership rule** (`0x971E0`, hostility): a type-1 unit belongs to a player when
  1. `0x13ABA0(id) != -1`, that is, the unit has a node in the pet list (owner = `node+0x0C`); or
  2. `Unit+0x128 & 0x400` (`0x34FB00`) and `Unit+0xE8 == 0`: the owner type is player, and the
     owner id is `Unit+0xEC` (`0x97322`..`0x97375`).

## 4. Usable corpses

**The game's selection.** `0x9E950(player, unit, skillId)` reads skills `+0x35` = **SelectProc**
(byte-exact over 429 rows; TargetCorpse is `+0x27` bit0). It calls `table[SelectProc](player, unit)`.
The table is at `0x2294F30` and its count is at `0x2294F74`; both are written at run time.

**The client select procs** (each moves the target into rcx):

| Proc | Calls | Predicate |
|---|---|---|
| `0x965E0` | `0x33A4F0` → `0x33FBE0(t, 0)` | core |
| `0x965F0` | `0x33A9A0(t, 0)` | core && MonStats `+0x68` Velocity != 0 |
| `0x96600` | `0x33A9A0` | core && Velocity && `0x34C730` && the class checks of Revive |
| `0x967B0` | `0x33A510` | core && MonStats2 **soft** (`+5` bit4, via `0x34FA60`) |
| `0x967C0` | `0x33A5C0` | core |

**Core predicate** (`0x33FBE0` / `0x33A5C0`):
* type 1 and mode **12** (MonMode DD);
* `!0x336300(unit)`, meaning no **udead** state;
* MonStats2 (`0x975B0`, bank+0xF98, stride 0x128) `+4` bit7 = **corpseSel**.

**The udead state test.**
* `0x336300(unit)` passes the mask the value read at bank offset 0x3D8 to `0x336440`.
* `0x336440` ANDs that mask with the unit's active-state bitset `statlist+0xAF0` (from `0x2F6370`,
  the same bitset the poison reader uses). The length in words is `(bank+0x298 + 31) >> 5`.
* The masks are an array at `bank+0x2C8 + 8·k` (indexed form at `0x3362D0`), with k the states
  flag bit. `states.bin` flags at `+0x10` put **udead** at flag bit 34, and
  `0x2C8 + 34·8 = 0x3D8`.
* The udead states in states.txt are freeze, dopplezon, revive, redeemed, corpse_nodraw, shatter,
  corpse_noselect, shadowwarrior, attached, recycled, restinpeace, corpseexp, uberminion,
  bind_demon and bind_demon_underling. This is how a consumed corpse stops counting.

**Cursor hover.** The hover filter `0x9B840` additionally requires that a dead monster keeps
`Unit+0x124` bit 2 (`0x34F5A0`). Monster init `0x1A33B0` sets that bit from MonStats2 **isAtt** and
sets bit 1 from **isSel**.

**Which proc each SelectProc uses is not established statically**, because the table is filled at
run time. The SelectProc values are 1 corpse explosion, 2 raise skeleton/mage, 3 revive, 4 find
potion, 5 find item and 6 grim ward. The thunk order suggests 1→`0x965E0`, 2→`0x965F0`,
3→`0x96600`; this is inferred. Live check: log the 8 qwords at `exeBase+0x2294F30` and the dword at
`+0x2294F74` once.

## 5. Mod integration and existing enumeration

* `native_entities.cpp:692`. Every type-1 unit in the room ring passes through `NoteDead`
  (`:177`, mode 0/12 ids), which `map_survey_service.cpp:375` drains to forget enemies.
  `ReadMonster` drops modes 0/12 at `:198`. This walk is where a usable-corpse count belongs.
* `corpse_memory.cpp` handles player corpses only (type 0, mode 17).

## 6. Fingerprints (`LayoutCheck::Hash`)

Fingerprint evidence for `LayoutCheck::Hash`; each row records RVA, byte span, hash and purpose.

| RVA | Bytes | Hash | Purpose / source locator |
|---|---:|---|---|
| 0x13B270 | 33 | 0x17DFFFA847B8AD90 | node by id: head 0x2A4DC10, id +8, next +0x30 |
| 0x13ABC0 | 123 | 0x6B3D47A73702E114 | (shipped) nth pet: owner +0xC, type +4, dead flag +0x20 |
| 0x13AA50 | 37 | 0xDA9992529F17CB7B | (shipped) life +0x1C |
| 0x13ABA0 | 32 | 0xEF297C9F25E10636 | owner +0xC or -1 |
| 0x13AB80 | 27 | 0x3F52C1F982EDB58A | pet type +4 |
| 0x13AA80 | 27 | 0xC210D535C7CBDBE5 | class u16 +0 |
| 0x13A5A0 | 179 | 0x4AC276CB45359F2E | node create: 0x38 bytes, life 100, head push |
| 0x13AEC0 | 173 | 0x640B7385EF906B52 | remove; own hireling flagged +0x20 |
| 0x13B1E0 | 129 | 0xDEE448CAC950BB5F | life +0x1C, level +0x10 |
| 0x12DBD0 | 36 | 0x9AE4DA92E6178A75 | add/remove packet layout |
| 0x12DE10 | 29 | 0x9C1AF17D2B65342B | (shipped) 0x7F: non-player  to  0x13B1E0 |
| 0x13A890 | 195 | 0xD27DFEF5C8476258 | count by owner/type (pool +8) |
| 0x13ADD0 | 228 | 0x75F1780A77344CD7 | per-pet walk, loaded units only |
| 0x13A4F0 | 169 | 0x41641BC29FB5E290 | PetType row: bank+0x12D8/+0x12E0, stride 200 |
| 0x1E75F0 | 697 | 0x719F3C566D8354C4 | HUD dispatcher: icontype +0x12 |
| 0x1E8750 | 1017 | 0xB43B0394955637C4 | single/counted portrait |
| 0x1E7C60 | 1150 | 0xF70A32865564F91F | per-pet portraits |
| 0x1E78B0 | 932 | 0x41B7EFE401C8822F | hireling portrait |
| 0x1E6560 | 341 | 0x65A33E3907E2FF09 | icon index: mclass +0xB6, micon +0x33, drawhp |
| 0x1E66C0 | 141 | 0x76D2ECDB0E4C5E6B | name: overridename +0xC0, name +0x10, 5382 skipped |
| 0x97490 | 228 | 0x76BB3F5F993669AA | (shipped) unit life prefers node life |
| 0x5000F0 | 581 | 0x8253CF977504AC72 | server: 0x7F for partysend pets |
| 0x480AD0 | 461 | 0x4E34945C8B6B454E | (shipped) server 0x7F builder |
| 0x480CA0 | 334 | 0x66027795B9D2B371 | server add/remove builder |
| 0x4FFF00 | 491 | 0x535EE4EC9E22C6CA | server: all pets of a player to a client |
| 0x1A28A0 | 611 | 0xE2E2757FB95F3E9F | client: bit 9 set for the hireling |
| 0x98FA0 | 1385 | 0x0C89F66E218B2A96 | ...only when pet type == 7 |
| 0x34E2F0 | 73 | 0xA458B77F6147A6E7 | bit 9 setter |
| 0x971E0 | 675 | 0x702AD84E5A3174AC | hostility: pet owner, or +0x128&0x400 / +0xE8 / +0xEC |
| 0x34FB00 | 54 | 0x94781FF2A3108387 | Unit+0x128 mask test |
| 0x9E950 | 129 | 0x264896FE6CD54DFC | SelectProc +0x35 dispatch, table 0x2294F30 |
| 0x97790 | 169 | 0x44E02BF073C32627 | skills row |
| 0x96600 | 421 | 0x2C28006559D579EF | revive proc |
| 0x33FBE0 | 177 | 0xAAB1ED1B6CE1192C | core: mode 12, !udead, corpseSel |
| 0x33A4F0 | 26 | 0x189A5B87CFF67661 | See surrounding native analysis |
| 0x33A5C0 | 155 | 0x8D9FF5C29B085D65 | core |
| 0x33A510 | 167 | 0xB4FCC3BC8968BF90 | core + soft |
| 0x33A9A0 | 96 | 0x0C67C5B215C0CBF1 | core + Velocity |
| 0x34FA60 | 156 | 0x249991F0A3B8BAB4 | MonStats2 +5 soft |
| 0x336300 | 42 | 0x5033F280CDEB4295 | udead mask bank+0x3D8 |
| 0x336440 | 159 | 0x7458FBC20E6C1529 | mask AND statlist+0xAF0 |
| 0x2F6370 | 96 | 0x5854EA1B418EE4BA | statlist+0xAF0 |
| 0x975B0 | 300 | 0x58E2457EEE069F89 | MonStats2 row |
| 0x9B840 | 1438 | 0x68A7C846BCCB18EF | hover: dead needs bit 2 |
| 0x1A33B0 | 1175 | 0xF4E8D8AF227FED8A | init: bit1 = isSel, bit2 = isAtt |
| 0x34DB10 | 71 | 0xD1F8F2A8B0759702 | bit 2 setter |

**Not useful as checks.** The thunks `0x965E0`/`0x965F0` and `0x967B0`/`0x967C0` hash identically
in pairs: their only operand is the zeroed jump.

**Hash caveat.** `LayoutCheck::Hash` treats every `E8` byte as a call. In `shr eax, imm`
(`C1 E8 ib`) and `bts eax, imm` (`0F BA E8 ib`) the ModRM byte is `E8`, so the bit number is
masked out. As a result `{0x34FBA0, 52}` (shipped, bit 9) and `0x34F5A0` (bit 2) have the
**same** hash. The check proves the function is a single-bit getter on `Unit+0x124`, but not which
bit it reads.

## 7. Live checks still open

1. The SelectProc table mapping (§4).
2. The pet list on a necromancer with skeletons, mages and a golem: the head, owner filter and
   life values against the portraits.
3. Rotw `bank+0x3D8`: log which state ids are set; expect the 15 udead ids above.

## 8. Reading corpseSel and udead with Peek alone (follow-up, 2026-10-14)

**Bank.**
* The bank byte is `Unit+0x1BD` for every unit type. `0x34A0E0` reads it with no type test, and
  `0x3501E0` passes a monster's own `+0x1BD` straight to `0x975B0`.
* `0x300A90(bank)` asserts that `bank < 4` and returns the value read at the sum of the sum of base and 0x2A9A580 and the product of bank and 0x10. That
  displacement is inside its fingerprint.
* `NativeGroundItems::RuntimeRoots` resolves `tables` through the same `0x300A90(player+0x1BD)`, so
  `tables` is this bank base. `+0xF58`, `+0xF98`, `+0x12D8`, `+0x298` and `+0x3D8` are all relative
  to it.
* The game resolves the bank from the unit it tests, so the strict replica peeks the monster's own
  `+0x1BD`.

**MonStats → MonStats2.**
* `0x975B0(uint8 bank, int classId)` returns the row pointer, or null when out of range:
  1. classId is nonnegative and below the count at bank offset 0xF60;
  2. ex is the signed 16-bit index at offset 0x4C of the classId row in the table reached through bank offset 0xF58, with 0x1FC row stride;
  3. ex is nonnegative and below the count at bank offset 0xFA0, a qword count;
  4. return the sum of the value read at bank offset 0xF98 and the product of ex and 0x128.
* A failed bound runs an assert hook and then returns 0.
* `+0x4C` holds the **MonStatsEx** column as a sign-extended int16: the row index into monstats2.
  Proven over all 751 rows of both the Rotw and base banks, and no other offset matches.
* corpseSel is `+4` bit 7 of that row, proven over all 623 rows of both banks.

**udead test.**
* `0x336300(unit)` loads the **pointer** the value read at bank offset 0x3D8, a dword bit array, and tail-calls
  `0x336440(unit, mask)`.
* `0x336440` returns int 1 when any word of `mask[i] & bits[i]` is non-zero, else 0, for
  i is below the word count obtained by adding 31 to the unsigned 64-bit bit-count at bank offset 0x298 and shifting right five bits.
* `bank+0x298` is the states row count: the table builder writes `bank+0x290` (rows) and `+0x298`
  (count) next to the 0x44 stride at `0x300646`..`0x300676`.
* `bits` comes from `0x2F6370`:
  1. statlist = the value read at Unit offset 0x88 (`0x34B870`);
  2. if the statlist is null, or the signed 32-bit statlist field at offset 0x1C is nonnegative, there are no bits and the test
     returns 0;
  3. otherwise `bits = statlist + 0xAF0`, an **inline** array.
* The path only reads. Its failure branches call the game's assert hooks, which end in `int3` when
  they return non-zero. A Peek replica that pre-checks these values is the safe form off the game
  thread.

Fingerprint evidence for `LayoutCheck::Hash`; each row records RVA, byte span, hash and purpose.

| RVA | Bytes | Hash | Purpose / source locator |
|---|---:|---|---|
| 0x975B0 | 300 | 0x58E2457EEE069F89 | MonStats2 row: MonStats+0x4C, bank+0xF98/+0xFA0, 0x128 |
| 0x976E0 | 169 | 0xFF5A28122B22038A | (shipped) MonStats row |
| 0x34A0E0 | 47 | 0x05F93D9EFDC375A1 | (shipped) Unit+0x1BD |
| 0x300A90 | 67 | 0xBC5C7D366A355A3B | (shipped) bank table 0x2A9A580, stride 0x10 |
| 0x3501E0 | 119 | 0x83CCAAB89BFD9402 | monster's own +0x1BD into 0x975B0 |
| 0x336300 | 42 | 0x5033F280CDEB4295 | mask pointer bank+0x3D8 |
| 0x336440 | 159 | 0x7458FBC20E6C1529 | word AND over (bank+0x298 + 31) >> 5 |
| 0x2F6370 | 96 | 0x5854EA1B418EE4BA | statlist+0x1C sign, +0xAF0 inline |
| 0x34B870 | 59 | 0xF5A91004595DA706 | statlist = Unit+0x88 |
