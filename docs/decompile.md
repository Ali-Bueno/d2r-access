# The Ghidra decompile: why it has no names, and what to do about it

Session of 2026-09-19. The 2026-09-18 pass completed and saved — 46 files,
138 MB, ~90,600 functions — and the plan that followed it was to "find the
`CfgMove*` handler in the decompile and call it". That plan cannot work with
this input, and the reason is not the analysis. It is the input file.

## Symptom

- **Not one string literal** in 138 MB of decompiled C.
- `CfgMoveUp` is in the image, at `0x14178BE40`. **Nothing references it** —
  not rip-relative, not as an absolute pointer, anywhere in the file. Neither
  do `BindingNameWrapper` or `PrimaryKeyButton`, which the game's own UI
  demonstrably uses.
- No RTTI class names, so no vftable labels, so nothing to name functions
  from. The import table is a single entry (`d2r_loader.dll`), so there are no
  API anchors either.

The net effect: ~90,000 anonymous functions and no way to describe the one you
are looking for.

## The code is fine — it is the data that is missing

Worth establishing first, because it rules out a bad dump of `.text`:

- Nine of the mod's own function fingerprints were recomputed straight from
  `re/D2R_decrypted.exe`. **Eight reproduce byte-exact.** The ninth is
  `0x2F5020`, the one `ground_item_layout.h` already marks `mayBeDetoured`
  because D2RLoader 1.3.0-beta replaces it with a thunk.
- `LayoutCheck::Verify` passes against the *live* game at those same RVAs, so
  this is not circular: code at RVA X in the file is the code at base+X in the
  process.

So `.text` is authentic and correctly placed. The problem is everything it
points at.

## Measurement

Scanning `.text` for genuine `lea r64, [rip+disp32]` instructions (REX.W,
opcode `8D`, ModRM mod=00 rm=101) finds **67,254** of them. Where they land:

| target | count |
|---|---|
| no file backing at all | 48,642 |
| `.data` | 11,975 |
| `.text` | 5,457 |
| `.pdata` | 1,101 |
| **`.rdata`** | **79** |

Seventy-nine references into the read-only data section of a 22 MB game binary
is not a plausible number. And disassembling two fingerprint-verified
functions shows what the references really look like:

```
1400c17f4  mov rax, qword ptr [rip + 0x2909acd]   -> 0x1429CB2C8   (the stack cookie)
14009a4be  lea rdi, [rip + 0x298942b]             -> 0x142A238F0
```

`0x1429CB2C8` is the `__security_cookie` — it appears as `_DAT_1429cb2c8`
throughout the decompile. It is **past the file's `SizeOfImage`**
(`0x142789000`). So is `0x142A238F0`. The highest such target is
`0x143E2C27F`, which puts the live image at **62 MB or more** against the
39.5 MB the on-disk headers declare.

## Root cause

The data is not at a *wrong* address — it is at a **constant offset**. Matching
the gap-sequence of string starts in `.rdata` against the sorted `lea` targets
gives one delta, unambiguously:

> **+0x5E40A0**

At that shift, **2,456** `lea` targets land exactly on a string start. Spot
checks: `decimalFormat`, `no error`, `POSIX collating elements are not
supported`, `Carian`, `0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ`. These are real
references that simply had nothing under them.

So at runtime the game's data sits ~6.2 MB higher than the on-disk section
table places it — the protector expands the image when it maps it — and
`re/D2R_decrypted.exe` was built by splicing the live `.text` into the
*on-disk* PE, whose `.rdata` and `.data` are still at their packed RVAs.

The splice is exactly what `src/code_dump.cpp` produces: it reads the section
headers from the live process, finds `.text`, and copies
`Misc.VirtualSize` bytes from `base + VirtualAddress`. **Only `.text`.**
Nothing else about the live image is captured.

Some code is missing too, not just data: of 431,645 `E8` call targets in
`.text`, 86.4% land inside `.text` and **5,361 land in the ~6 MB gap after
it** — the region the on-disk headers do not describe.

## The fix

**Dump the whole mapped image, not one section.** Walk `VirtualQuery` from
`context->exeBase` across the entire allocation, capture every committed page
out to the real end (~0x143E2D000 on the observed build), and import that as a
flat binary based at `0x140000000`. Every data reference then resolves, and
that is what produces names:

- Ghidra's ASCII-Strings analyzer defines the strings the code points at, and
  the decompiler starts emitting them as literals — so `CfgMoveUp` becomes
  greppable and its consumer is one xref away.
- The RTTI analyzer gets real type descriptors and vftables, which names
  classes and, through them, a large share of the methods.
- Globals stop being `DAT_...` in empty space.

### Implemented, 2026-09-19

`access-dump-code image` now does the dump (`src/code_dump.cpp`). It walks
`VirtualQuery` from `context->exeBase`, writes every committed page as one flat
block based at that address, and records the region map plus the **live**
`SizeOfImage` in `D2R.image.json`. Gaps stay zero rather than closing up, so a
byte at file offset N is still the byte the code addresses at base+N.

The bare `access-dump-code` still writes the old `.text`-only dump, because
`tools/layout_fingerprints.py` reads it.

Two workspace tools carry it the rest of the way:

- `tools/rebuild_pe.py` rewrites the dump's section table so raw == virtual and
  gives any tail past the last section a `.dumped` section of its own, producing
  `re/D2R_image.exe`. Run it with no output path first: it prints the header
  facts, including whether the live `SizeOfImage` is larger than the file's and
  whether the section table has room for the extra header.
- `tools/ghidra/import_image.bat` imports that into a **new** project,
  `re/ghidra-image/D2R_image`, beside the existing one. The current `D2R`
  project is not touched: its control flow is sound and stays usable as a
  cross-check.

Importing as a PE rather than as a raw binary is deliberate — it is what lets
the RTTI analyzer recover type descriptors and vftables, and those are half of
where the names come from.

Order of operations:

1. In game, open the console and run `access-dump-code image`. Expect a brief
   hitch: it copies ~60-70 MB and writes it next to the plugin.
2. Copy `D2R.image.bin` / `D2R.image.json` out of
   `<game>/d2rloader/plugins/d2r-access/`.
3. `python tools/rebuild_pe.py D2R.image.bin D2R.image.json` to inspect, then
   again with `re/D2R_image.exe` to write.
4. `tools\ghidra\import_image.bat`. Many hours, saves once at the end, no
   timeout flags.

A cheaper half-measure exists — re-splice with the data sections shifted by
`+0x5E40A0` — but it only recovers what the on-disk `.rdata`/`.data` already
hold. It cannot recover the ~6 MB of code in the gap, or anything the
protector materialises at run time. Prefer the full dump.

Per the workspace rules the re-import is one all-or-nothing headless run:
`-import` once, no timeout flags, full analysis plus full decompile in the
same pass. **Do not `-import -overwrite` the existing project** — build the new
one beside it so the current decompile stays usable for control flow, which it
is: the logic is sound, only the names are absent.

## A second naming source, independent of all this

`D2RCore.dll` carries a **static** descriptor table at `.data:0x67AA30`,
stride `0x18`, with the game RVA at `+0x08` — the same table `core_slots.h`
reads at run time, but the RVA field is already populated on disk. It holds
**1,071 unique game addresses**, verified to reproduce the two slots
`core_slots.h` documents for 1.3.0-beta (`0x67DDC0` → `0x8B2D0`,
`0x67DDF0` → `0x9A480`), with the unit accessors clustered at
`0x67DF28`–`0x67DF88`.

That is a curated map of everything the loader resolves, and it narrows any
future hunt from ~90,000 functions to ~1,071 — useful even once names work.
