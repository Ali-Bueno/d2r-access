"""Derive (and re-verify) the container predicate the plugin uses for chest loops.

The plugin decides "this object is something the player opens for loot" from two
objects.txt columns it reads straight out of the game's memory: OperateFn (the
interaction handler) and the per-mode Selectable bytes. Neither the column
offsets nor the handler numbers may be guessed, so this script proves both
against the game's own files and prints the row set the predicate selects.

Run after every game patch:

    build/casc-extract/casc_extract.exe extract "<game dir>" game-data objects.bin objects.txt
    python tools/objects_containers.py

The row count it prints is the tripwire recorded in src/entity_layout.h.
"""
import collections
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent / "game-data" / "global" / "excel"
STRIDE = 0x168
# The offsets src/entity_layout.h reads, and the objects.txt columns they must equal.
OFFSETS = {
    "Name": 0x42,
    "Selectable0": 0x85,  # ... Selectable7 at 0x8C
    "HasCollision0": 0xE0,  # ... HasCollision7 at 0xE7
    "IsAttackable0": 0xE8,  # the client copies it to flag 0x4 of unit+0x124 (0x1401CB528)
    "IsDoor": 0xFA,
    "Lockable": 0x12F,
    "PopulateFn": 0x15D,
    "OperateFn": 0x15E,
    "OpenWarp": 0x163,
}
DWORD_OFFSETS = {"SizeX": 0x90, "SizeY": 0x94}
# OperateFn values whose rows are the game's containers. Read off objects.txt,
# not invented: 1 caskets/sarcophagi, 3 urns/jars/baskets, 4 chests, 5 and 7
# barrels, 14 crates/coffins/corpses/logs, 19 armor stands, 20 weapon racks,
# 26 bookshelves, 30 exploding chests, 39-41 and 57-59 the act-specific special
# chests, 68 the evil urn.
CONTAINER_OPERATE = {1, 3, 4, 5, 7, 14, 19, 20, 26, 30, 39, 40, 41, 57, 58, 59, 68}


def rows_from_txt(path):
    lines = path.read_text(encoding="utf-8-sig", errors="replace").splitlines()
    header = lines[0].split("\t")
    rows = [dict(zip(header, line.split("\t"))) for line in lines[1:] if line.strip()]
    # The bin drops the txt's nameless placeholder lines; keeping them would
    # shift every row after the first one.
    return [r for r in rows if r.get("Name", "").strip()]


def number(row, column):
    try:
        return int(row.get(column, "").strip())
    except ValueError:
        return 0


def cstring(data, offset, limit=64):
    end = data.find(b"\0", offset)
    return data[offset:end if 0 <= end < offset + limit else offset + limit].decode("latin-1")


def main():
    txt, binary = ROOT / "objects.txt", ROOT / "objects.bin"
    if not txt.exists() or not binary.exists():
        sys.exit(f"extract objects.txt and objects.bin into {ROOT} first (see the docstring)")
    rows = rows_from_txt(txt)
    blob = binary.read_bytes()
    count = (len(blob) - 4) // STRIDE
    print(f"objects.bin {len(blob)} bytes -> {count} rows, objects.txt -> {len(rows)} named rows")
    if count != len(rows):
        sys.exit("row counts differ: the two files are not from the same build")

    mismatches = collections.Counter()
    for index, row in enumerate(rows):
        data = blob[4 + index * STRIDE:4 + (index + 1) * STRIDE]
        if cstring(data, OFFSETS["Name"]) != row["Name"].strip():
            mismatches["Name"] += 1
        for column, offset in OFFSETS.items():
            if column == "Name":
                continue
            if data[offset] != number(row, column):
                mismatches[column] += 1
        for mode in range(8):
            if data[OFFSETS["Selectable0"] + mode] != number(row, f"Selectable{mode}"):
                mismatches["Selectable"] += 1
            if data[OFFSETS["HasCollision0"] + mode] != number(row, f"HasCollision{mode}"):
                mismatches["HasCollision"] += 1
        for column, offset in DWORD_OFFSETS.items():
            if int.from_bytes(data[offset:offset + 4], "little") != number(row, column):
                mismatches[column] += 1
    print("offset vs column:", dict(mismatches) or "every offset equals its column on every row")

    # The predicate itself: a container handler, and the game lets the player
    # click it while it is shut but not once it is open.
    hits = [r for r in rows if number(r, "OperateFn") in CONTAINER_OPERATE
            and number(r, "Selectable0") and not number(r, "Selectable2")]
    names = sorted({r["Name"].strip() for r in hits}, key=str.lower)
    print(f"\ncontainers: {len(hits)} rows, {len(names)} distinct names")
    print("  " + ", ".join(names))

    print("\nrows per handler")
    for value in sorted(CONTAINER_OPERATE):
        group = sorted({r["Name"].strip() for r in hits if number(r, "OperateFn") == value}, key=str.lower)
        print(f"  {value:3d}: {', '.join(group)}")


if __name__ == "__main__":
    main()
