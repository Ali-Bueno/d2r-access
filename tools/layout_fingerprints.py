"""Check, explain and re-derive the native layout fingerprints after a loader update.

The native readers guard their field offsets with a hash of complete game
functions (`src/layout_check.h`). A new D2RLoader build can break those without
the game changing at all, because the loader patches the game's code: usually it
just redirects a call into code it allocates at run time, occasionally it
rewrites an instruction or replaces a whole function with a thunk. Those three
cases need different answers, and this tells them apart.

Inputs:
  - the live dump the plugin writes on first front end, under the NEW loader:
    <game>/d2rloader/plugins/d2r-access/D2R.text.bin (+ .json)
  - re/D2R.rebuilt.exe, the reconstructed live image from a known-good loader
    (file offset == RVA). Optional: without it, checks are only re-hashed.

Usage (from the repo root):
    python tools/layout_fingerprints.py            # report
    python tools/layout_fingerprints.py --rewrite  # write the new hashes in place

Verdicts:
  redirected-call  only the operand of a relative call/jump moved. The masked
                   hash already ignores it, so nothing needs doing.
  patched-code     the loader rewrote an instruction. Read it before re-deriving:
                   the change may alter a field the reader depends on (1.3.0-beta
                   widened ObjectData+8 from a byte to a word this way).
  detoured         the function now starts with a jump. If the reader only calls
                   it, mark the entry `mayBeDetoured` instead of re-hashing; its
                   body belongs to the loader and is not ours to fingerprint.
"""

import argparse
import io
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
TABLES = ["src/ground_item_layout.h", "src/entity_layout.h", "src/native_world.cpp"]
ENTRY = re.compile(r"(\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(\d+)\s*,\s*)0x([0-9A-Fa-f]+)ULL")

FNV_BASIS, FNV_PRIME, MASK64 = 14695981039346656037, 1099511628211, (1 << 64) - 1
RELATIVE = (0xE8, 0xE9)


def masked_hash(data):
    """FNV-1a with the operand of every relative call/jump hashed as zero.

    Must stay identical to LayoutCheck::Hash in src/layout_check.h.
    """
    value, at, size = FNV_BASIS, 0, len(data)
    while at < size:
        byte = data[at]
        value = ((value ^ byte) * FNV_PRIME) & MASK64
        if byte in RELATIVE:
            operand = min(4, size - at - 1)
            for _ in range(operand):
                value = (value * FNV_PRIME) & MASK64
            at += operand
        at += 1
    return value


def read_tables():
    out = {}
    for relative in TABLES:
        text = io.open(REPO / relative, encoding="utf-8").read()
        body = text[text.index("Checks[] = {"):]
        out[relative] = [(int(rva, 16), int(size), int(stored, 16))
                         for _, rva, size, stored in ENTRY.findall(body[:body.index("};")])]
    return out


def load_images(game_dir):
    plugin = pathlib.Path(game_dir) / "d2rloader/plugins/d2r-access"
    info_path, dump_path = plugin / "D2R.text.json", plugin / "D2R.text.bin"
    if not dump_path.exists():
        sys.exit(f"no live dump at {dump_path}\nLaunch the game once under the new loader first.")
    info = json.loads(info_path.read_text(encoding="utf-8"))
    reference_path = REPO / "re/D2R.rebuilt.exe"
    reference = reference_path.read_bytes() if reference_path.exists() else None
    return info, dump_path.read_bytes(), reference


def classify(live, reference, size):
    """Why two versions of the same function differ."""
    if reference is None:
        return "changed"
    if live[:2] == b"\xff\x25" or (live[:1] == b"\xe9" and reference[:1] != b"\xe9"):
        return "detoured"
    for at in range(size):
        if live[at] == reference[at]:
            continue
        start = next((back for back in range(at - 1, max(-1, at - 5), -1)
                      if live[back] == reference[back] and live[back] in RELATIVE), None)
        if start is None:
            return "patched-code"
    return "redirected-call"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", default=r"D:\games\steam\steamapps\common\Diablo II Resurrected")
    parser.add_argument("--rewrite", action="store_true", help="write the re-derived hashes into the tables")
    args = parser.parse_args()

    info, dump, reference = load_images(args.game)
    text_rva = info["textRva"]
    print(f"live dump: game {info.get('build') or '?'}, .text RVA {text_rva:#x}, "
          f"{len(dump)} bytes, {info.get('unreadablePages')} unreadable page(s)")
    print(f"reference: {'re/D2R.rebuilt.exe' if reference is not None else 'absent - verdicts unavailable'}\n")

    stale = {}
    for relative, checks in read_tables().items():
        print(f"--- {relative} ({len(checks)} checks) ---")
        for rva, size, stored in checks:
            now = dump[rva - text_rva:rva - text_rva + size]
            if len(now) != size:
                print(f"  {rva:>7X}  outside the dump")
                continue
            fresh = masked_hash(now)
            if fresh == stored:
                continue
            was = reference[rva:rva + size] if reference is not None else None
            print(f"  {rva:>7X}  {classify(now, was, size):16s} 0x{stored:016X} -> 0x{fresh:016X}")
            stale[rva] = fresh
        print()

    if not stale:
        print("Every fingerprint matches the live image.")
        return
    if not args.rewrite:
        print(f"{len(stale)} fingerprint(s) differ. Read the verdicts above, then re-run with --rewrite.")
        return

    for relative in TABLES:
        path = REPO / relative
        text = io.open(path, encoding="utf-8").read()
        def swap(match):
            rva = int(match.group(2), 16)
            return f"{match.group(1)}0x{stale[rva]:016X}ULL" if rva in stale else match.group(0)
        io.open(path, "w", encoding="utf-8", newline="\n").write(ENTRY.sub(swap, text))
        print("rewrote", relative)


if __name__ == "__main__":
    main()
