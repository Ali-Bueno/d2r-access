#!/usr/bin/env python3
r"""gen_lootables.py -- Generates src/generated/lootables.h, the set of world
objects that give the player loot, keyed by the object class id the plugin
already reads out of a unit (UnitClass -> objects.txt row).

Why this exists
---------------
The plugin used to decide "is this treasure" from the objects.txt SubClass
container bit (8). That bit is not a lootability signal:

  * every Act 1 rogue corpse -- RogueCorpse1/2, RogueRollingCorpse1,
    RogueStakedCorpse1/2, the "Dead Rogue" a player loots in the Blood Moor --
    has SubClass = 0, and so do Barrel, Urn2, Casket3, Chest4 and
    Act3DungeonCasket;
  * meanwhile Act 2/3 corpses (GuardOnAStick, Corpse1Act3, DeadPerson) do
    carry it.

So lootable things were being filed under "interactable" beside levers and
doors, and some chests were not in the treasure category at all.

OperateFn is the real signal. It is the object's behaviour function, and its
membership is clean -- see the two groups below, each listed in full in the
generated header so the choice can be audited without rerunning this.

Usage:
    python tools/gen_lootables.py [--objects game-data/global/excel/objects.txt]
                                  [--out src/generated/lootables.h]

objects.txt comes from the game's CASC storage:
    tools/casc_extract.exe extract "<game dir>" <outdir> objects.txt
and lands at <outdir>/global/excel/objects.txt. game-data/ is gitignored; the
generated header is what gets committed.
"""
import argparse
import collections
import io
from pathlib import Path

# Containers and destructibles that roll a drop. A player asking for
# "treasure" wants every one of these.
CONTAINER_FNS = {
    1,   # caskets, beds, tombs (Casket1-6, MummyCasket, Bed1Act1, TombAct2, ...)
    3,   # urns, baskets, jars, boxes, rock piles
    4,   # chests of every act, stashes, and the dead bodies that hold loot
    5,   # Barrel
    7,   # BarrelExploding
    14,  # crates, corpses, logs, skull piles, cocoons -- the Act 1 dead rogues
    19,  # ArmorStand1/2/Left/Right
    20,  # WeaponRack1/2/Left/Right
    26,  # Bookshelf1/2
    30,  # ExplodingChest100 (and ExplodingCow, which is not loot -- see below)
    68,  # IceCaveEvilUrn
}

# Objects that hand over a fixed quest item rather than rolling a drop. A
# blind player has no other way to find the Horadric Malus or the Tree of
# Inifuss, and "treasure" is the heading they would look under.
QUEST_ITEM_FNS = {
    12,  # InifussTree
    21,  # Malus
    28,  # LamEsensTome
    39,  # HoradricCubeChest
    40,  # HoradricScrollChest
    41,  # StaffOfKingsChest
    57,  # KhalimHeartChest
    58,  # KhalimEyeChest
    59,  # KhalimBrainChest
}

# Known over-inclusion, recorded rather than special-cased: OperateFn 30 holds
# ExplodingChest100, a trapped chest that does drop, and ExplodingCow, which
# does not. Filing one cow under treasure is a smaller error than leaving
# trapped chests out, and a name-based exception would be exactly the kind of
# hardcoding this generator exists to avoid.

LOOT_FNS = CONTAINER_FNS | QUEST_ITEM_FNS

# Quest books a player reads rather than loots; the automap draws both with the
# tome cell (427) it gives Lam Esen's Tome. They loop the scroll cue until read.
TOME_FNS = {
    6,   # TowerTome, the Moldy Tome that opens the Forgotten Tower
    42,  # ArcaneTome, Horazon's journal that opens the way to the Canyon of the Magi
}

# Containers that are a dead body, by the objects.txt Name key the game labels
# them with (local/lng/strings: "Corpse", "Dead Rogue", "Guard Corpse", "Body",
# "Dead Body", "Dead Guard", "Hung Skeleton", "Dead Barbarian"; "skeleton" and
# "TrappedSoul" come from the monster strings). They get the corpse loop
# instead of the chest one. DungeonGuy is left out: its key is the "Dummy"
# placeholder, so the game names nothing a player could recognise.
CORPSE_NAME_KEYS = {
    "corpse", "RogueCorpse", "CorpseOnStick", "GuardCorpse", "body", "dead body",
    "dead guard", "HungSkeleton", "deadbarbarian", "deadperson", "deadperson2",
    "skeleton", "TrappedSoul",
}


def read_rows(path):
    rows = [line.rstrip("\n").split("\t") for line in io.open(path, encoding="utf-8-sig")]
    header = rows[0]
    index = {name: i for i, name in enumerate(header)}
    for needed in ("*ID", "Class", "Name", "OperateFn"):
        if needed not in index:
            raise SystemExit("objects.txt has no %s column" % needed)
    return rows[1:], index


def main():
    here = Path(__file__).resolve().parent
    repo = here.parent
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", type=Path, default=repo / "game-data" / "global" / "excel" / "objects.txt")
    parser.add_argument("--out", type=Path, default=repo / "src" / "generated" / "lootables.h")
    args = parser.parse_args()

    rows, index = read_rows(args.objects)
    ident, klass, name, operate = index["*ID"], index["Class"], index["Name"], index["OperateFn"]
    selectable = [index["Selectable%d" % mode] for mode in range(8)]

    entries = []          # (class id, class name, OperateFn)
    corpses = []          # (class id, class name)
    tomes = []            # (class id, class name)
    by_fn = collections.defaultdict(list)
    skipped_unselectable = 0
    for row in rows:
        if len(row) <= max(ident, klass, name, operate, *selectable):
            continue
        # The "Expansion" separator row carries no id and is not a real object.
        if not row[ident].strip():
            continue
        try:
            fn = int(row[operate] or 0)
        except ValueError:
            continue
        if fn in TOME_FNS:
            tomes.append((int(row[ident]), row[klass]))
        if fn not in LOOT_FNS:
            continue
        # Only what the game itself lets the player target in some mode; the
        # plugin applies the same rule per current mode when it reads a unit.
        if not any(row[slot] == "1" for slot in selectable):
            skipped_unselectable += 1
            continue
        entries.append((int(row[ident]), row[klass], fn))
        by_fn[fn].append(row[klass])
        if fn in CONTAINER_FNS and row[name] in CORPSE_NAME_KEYS:
            corpses.append((int(row[ident]), row[klass]))

    entries.sort()
    if not entries:
        raise SystemExit("no lootable objects found -- is objects.txt the right file?")

    out = []
    out.append("// generated by tools/gen_lootables.py from game-data -- do not edit")
    out.append("//")
    out.append("// World objects that give the player loot, by the object class id the")
    out.append("// plugin reads from a unit (UnitClass). This replaces the objects.txt")
    out.append("// SubClass container bit, which is NOT a lootability signal: every Act 1")
    out.append("// rogue corpse has SubClass 0, and so do Barrel, Urn2, Casket3, Chest4 and")
    out.append("// Act3DungeonCasket, while Act 2/3 corpses carry it. OperateFn is the real")
    out.append("// signal; the groups below are its full membership, so the choice can be")
    out.append("// audited here without rerunning the generator.")
    out.append("//")
    for fn in sorted(by_fn):
        group = "quest item" if fn in QUEST_ITEM_FNS else "container"
        members = ", ".join(by_fn[fn])
        line = "// OperateFn %d (%s): %s" % (fn, group, members)
        # Wrap long member lists to keep the header readable.
        while len(line) > 78:
            cut = line.rfind(", ", 0, 78)
            if cut < 0:
                break
            out.append(line[:cut + 1])
            line = "//     " + line[cut + 2:]
        out.append(line)
    out.append("#pragma once")
    out.append("")
    out.append("#include <algorithm>")
    out.append("#include <array>")
    out.append("#include <cstdint>")
    out.append("")
    out.append("namespace D2RAccess::Generated {")
    out.append("")
    out.append("// Sorted, so a lookup is a binary search over %d ids." % len(entries))
    out.append("inline constexpr std::array<uint16_t, %d> LootableObjects{" % len(entries))
    row_text = "\t"
    for class_id, class_name, fn in entries:
        piece = "%d," % class_id
        if len(row_text) + len(piece) > 78:
            out.append(row_text)
            row_text = "\t"
        row_text += piece + " "
    if row_text.strip():
        out.append(row_text.rstrip())
    out.append("};")
    out.append("")
    quest_ids = [class_id for class_id, _, fn in entries if fn in QUEST_ITEM_FNS]
    out.append("// Quest interactions that grant a fixed quest item.")
    out.append("inline constexpr std::array<uint16_t, %d> QuestItemObjects{" % len(quest_ids))
    out.append("\t" + ", ".join(str(class_id) for class_id in quest_ids))
    out.append("};")
    out.append("")
    out.append("constexpr bool QuestItemObject(uint32_t classId) {")
    out.append("\tif (classId > UINT16_MAX) return false;")
    out.append("\treturn std::binary_search(QuestItemObjects.begin(), QuestItemObjects.end(),")
    out.append("\t\tstatic_cast<uint16_t>(classId));")
    out.append("}")
    out.append("")
    corpses.sort()
    out.append("// Containers that are a dead body, by their objects.txt Name key:")
    line = "//     "
    for _, class_name in corpses:
        piece = class_name + ", "
        if len(line) + len(piece) > 78:
            out.append(line.rstrip())
            line = "//     "
        line += piece
    out.append(line.rstrip().rstrip(","))
    out.append("inline constexpr std::array<uint16_t, %d> CorpseObjects{" % len(corpses))
    out.append("	" + ", ".join(str(class_id) for class_id, _ in corpses))
    out.append("};")
    out.append("")
    out.append("constexpr bool CorpseObject(uint32_t classId) {")
    out.append("	if (classId > UINT16_MAX) return false;")
    out.append("	return std::binary_search(CorpseObjects.begin(), CorpseObjects.end(),")
    out.append("		static_cast<uint16_t>(classId));")
    out.append("}")
    out.append("")
    tomes.sort()
    out.append("// Quest books read in place: %s." % ", ".join(class_name for _, class_name in tomes))
    out.append("inline constexpr std::array<uint16_t, %d> TomeObjects{" % len(tomes))
    out.append("\t" + ", ".join(str(class_id) for class_id, _ in tomes))
    out.append("};")
    out.append("")
    out.append("constexpr bool TomeObject(uint32_t classId) {")
    out.append("\tif (classId > UINT16_MAX) return false;")
    out.append("\treturn std::binary_search(TomeObjects.begin(), TomeObjects.end(),")
    out.append("\t\tstatic_cast<uint16_t>(classId));")
    out.append("}")
    out.append("")
    out.append("// True when this object class gives the player loot.")
    out.append("constexpr bool LootableObject(uint32_t classId) {")
    out.append("\tif (classId > UINT16_MAX) return false;")
    out.append("\treturn std::binary_search(LootableObjects.begin(), LootableObjects.end(),")
    out.append("\t\tstatic_cast<uint16_t>(classId));")
    out.append("}")
    out.append("")
    out.append("}")
    out.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    io.open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print("%s: %d lootable object classes from %d OperateFn groups, %d corpses, %d tomes (%d skipped as never selectable)"
          % (args.out, len(entries), len(by_fn), len(corpses), len(tomes), skipped_unselectable))


if __name__ == "__main__":
    main()
