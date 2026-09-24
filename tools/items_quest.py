"""Verify native item code, primary type and quest offsets against installed CASC tables.

Extract itemtypes.bin and misc/weapons/armor .txt and .bin into game-data/global/excel
with tools/casc-extract before running this after a game update.
"""

import csv
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parent.parent / "game-data" / "global" / "excel"
STRIDE = 0x1C0
CODE = 0x80
TYPE = 0x12E
QUEST = 0x13A
TYPE_STRIDE = 0xE8


def rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return [row for row in csv.DictReader(stream, delimiter="\t") if row["code"].strip()]


def records(path, stride):
    data = path.read_bytes()
    count = struct.unpack_from("<I", data)[0]
    if len(data) != 4 + count * stride:
        raise ValueError(f"{path.name}: unexpected row stride or count")
    return [data[4 + i * stride:4 + (i + 1) * stride] for i in range(count)]


def main():
    types = [row[:4].decode("ascii").strip() for row in records(ROOT / "itemtypes.bin", TYPE_STRIDE)]
    total = quest_items = quest_type_only = 0
    for name in ("misc", "weapons", "armor"):
        text_rows = rows(ROOT / f"{name}.txt")
        binary_rows = records(ROOT / f"{name}.bin", STRIDE)
        if len(text_rows) != len(binary_rows):
            raise ValueError(f"{name}: text and binary row counts differ")
        for index, (text, binary) in enumerate(zip(text_rows, binary_rows)):
            code = binary[CODE:CODE + 4].decode("ascii").strip()
            type_index = struct.unpack_from("<H", binary, TYPE)[0]
            item_type = types[type_index] if type_index < len(types) else ""
            quest = binary[QUEST]
            if (code, item_type, quest) != (text["code"], text["type"], int(text["quest"] or 0)):
                raise ValueError(f"{name} row {index}: code/type/quest mismatch")
            total += 1
            quest_items += bool(quest)
            quest_type_only += item_type == "ques" and not quest
    print(f"Verified {total} item rows: code +0x{CODE:X}, type +0x{TYPE:X}, quest +0x{QUEST:X}")
    print(f"Quest flag: {quest_items}; additional primary 'ques' items: {quest_type_only}")


if __name__ == "__main__":
    main()
