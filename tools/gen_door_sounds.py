#!/usr/bin/env python3
r"""gen_door_sounds.py -- Generates src/generated/door_sounds.h: the sound the
game itself plays when each door object class opens, keyed by the object class
id the plugin reads out of a unit (UnitClass -> objects.txt *ID).

objects.txt has no material column, and the class names mislead (DoorGateLeft
plays the metal sound, the desert wooden grate doors do too). The client keeps a
per-class object sound table instead: 8-byte pointers indexed by *ID, each to a
record of 12 dwords whose first eight are one sounds.txt *Index per object mode
(a door's close sound in NU, its open sound in OP or ON). Found by searching the
executable for the door sound ids; docs/ground-items.md#door-loops has the
evidence.

Usage (after every game patch; the table address is the disk image's):
    build/casc-extract/casc_extract.exe extract "<game dir>" game-data objects.txt sounds.txt
    python tools/gen_door_sounds.py [--exe "<game dir>/D2R.exe"]

A patch that moves the table fails the checks below instead of producing a
wrong header.
"""
import argparse
import csv
import io
import re
import struct
from pathlib import Path

GAME_EXE = Path(r"D:/games/steam/steamapps/common/Diablo II Resurrected/D2R.exe")
# D2R 3.3.93787 (D2R.exe SHA-1 7c742eac...): RVA of the per-class sound table in .rdata.
TABLE_RVA = 0x1606810
RECORD_DWORDS, MODE_SLOTS = 12, 8
# sounds.txt names: object_door_<sound>_open, or object_door_<sound> when one sound serves both ways.
DOOR_SOUND = re.compile(r"^object_door_(wood|metal|stone|gate|slime)(_open)?$")
ENUM = {"wood": "Wood", "metal": "Metal", "stone": "Stone", "gate": "Gate", "slime": "Slime"}


class Image:
    """Maps virtual addresses of an on-disk PE32+ image to file offsets."""

    def __init__(self, path):
        self.data = path.read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        count, optional_size = struct.unpack_from("<H12xH", self.data, pe + 6)
        self.base = struct.unpack_from("<Q", self.data, pe + 24 + 24)[0]
        self.sections = []
        for i in range(count):
            at = pe + 24 + optional_size + 40 * i
            size, rva, raw_size, raw = struct.unpack_from("<IIII", self.data, at + 8)
            self.sections.append((rva, max(size, raw_size), raw))

    def offset(self, rva):
        for start, size, raw in self.sections:
            if start <= rva < start + size:
                return raw + rva - start
        raise SystemExit("RVA %#x is outside every section -- wrong executable?" % rva)

    def dwords(self, rva, count):
        return struct.unpack_from("<%dI" % count, self.data, self.offset(rva))

    def pointer(self, rva):
        return struct.unpack_from("<Q", self.data, self.offset(rva))[0]


def read_table(path, *columns):
    rows = list(csv.reader(io.open(path, encoding="utf-8-sig"), delimiter="\t"))
    header = {name: i for i, name in enumerate(rows[0])}
    for needed in columns:
        if needed not in header:
            raise SystemExit("%s has no %s column" % (path, needed))
    return rows[1:], header


def main():
    repo = Path(__file__).resolve().parent.parent
    excel = repo / "game-data" / "global" / "excel"
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=GAME_EXE)
    parser.add_argument("--objects", type=Path, default=excel / "objects.txt")
    parser.add_argument("--sounds", type=Path, default=excel / "sounds.txt")
    parser.add_argument("--out", type=Path, default=repo / "src" / "generated" / "door_sounds.h")
    args = parser.parse_args()

    sound_rows, sound_columns = read_table(args.sounds, "Sound", "*Index")
    names = {}
    for row in sound_rows:
        if row[sound_columns["*Index"]].strip().isdigit():
            names[int(row[sound_columns["*Index"]])] = row[sound_columns["Sound"]]
    object_rows, columns = read_table(args.objects, "*ID", "Class", "IsDoor")
    doors = [(int(row[columns["*ID"]]), row[columns["Class"]]) for row in object_rows
             if row[columns["*ID"]].strip().isdigit() and row[columns["IsDoor"]] == "1"]
    if not doors:
        raise SystemExit("no IsDoor rows -- is objects.txt the right file?")

    image = Image(args.exe)
    entries = []  # (class id, class name, enum name, sound name)
    for class_id, class_name in sorted(doors):
        record = image.pointer(TABLE_RVA + 8 * class_id)
        if not record:
            raise SystemExit("%s (%d) has no sound record -- the table moved; re-derive TABLE_RVA" % (class_name, class_id))
        slots = image.dwords(record - image.base, RECORD_DWORDS)[:MODE_SLOTS]
        found = {names.get(sound, "") for sound in slots if sound}
        opens = sorted(name for name in found if DOOR_SOUND.match(name))
        if len(opens) != 1:
            raise SystemExit("%s (%d) mode sounds %s hold no single door opening sound -- the table moved"
                             % (class_name, class_id, sorted(found)))
        entries.append((class_id, class_name, ENUM[DOOR_SOUND.match(opens[0]).group(1)], opens[0]))

    out = ["// generated by tools/gen_door_sounds.py from game-data and D2R.exe -- do not edit",
           "//",
           "// The sound the game plays when each door object class opens, read from the",
           "// client's per-class object sound table (docs/ground-items.md#door-loops).",
           "//"]
    for class_id, class_name, _, sound in entries:
        out.append("// %3d %-28s %s" % (class_id, class_name, sound))
    out += ["#pragma once",
            "",
            "#include <array>",
            "#include <cstdint>",
            "",
            "namespace D2RAccess::Generated {",
            "",
            "enum class DoorSound : uint8_t { None, Wood, Metal, Stone, Gate, Slime };",
            "struct DoorEntry { uint16_t classId; DoorSound sound; };",
            "",
            "inline constexpr std::array<DoorEntry, %d> DoorSounds{{" % len(entries)]
    out += ["\t{%d, DoorSound::%s}," % (class_id, enum) for class_id, _, enum, _ in entries]
    out += ["}};",
            "",
            "// None for anything that is not a door.",
            "constexpr DoorSound DoorOpenSound(uint32_t classId) {",
            "\tfor (const auto& door : DoorSounds) if (door.classId == classId) return door.sound;",
            "\treturn DoorSound::None;",
            "}",
            "",
            "}",
            ""]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    io.open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print("%s: %d door classes" % (args.out, len(entries)))


if __name__ == "__main__":
    main()
