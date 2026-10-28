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

# Objects a quest is advanced by using, which hand over no item: each handler
# below is the only one of its object (docs/quest-objects.md). Each maps to the
# modes (objects.txt Mode0..7 order) in which it is still a quest step; it loops
# the quest cue and lists as a quest object only then.
NEUTRAL = (0,)
QUEST_MECHANISM_FNS = {
    10: NEUTRAL,  # Gibbet, Cain's cage in Tristram
    24: NEUTRAL,  # TaintedSunShrine, the Tainted Sun altar
    25: NEUTRAL,  # SevenTombsReceptacle, the orifice the Horadric Staff goes in
    31: NEUTRAL,  # GidBInnDecoy: wakes a guardian that drops the Gidbinn
    45: NEUTRAL,  # BeneathTheCityLever, opens the Kurast sewer stairs
    # SoulStoneForge, the Hellforge: mode 2 holds the Soulstone until the hammer
    # breaks it (the client asks for the hammer in hand in mode 2, 0x140FA6DA).
    49: (0, 2),
    52: NEUTRAL,  # DiabloSeal2, DiabloSeal4
    53: NEUTRAL,  # CompellingOrb, smashed with Khalim's Will
    54: NEUTRAL,  # DiabloSeal1
    55: NEUTRAL,  # DiabloSeal3
    56: NEUTRAL,  # DiabloSeal5
    65: NEUTRAL,  # AncientsAltar, the Altar of the Heavens
    67: NEUTRAL,  # AnyaYetAgain, Frozen Anya
    74: NEUTRAL,  # ColossalAltar, the other Altar of the Heavens row
}

# The Chaos Sanctuary seals, all five of which open the way to Diablo. Their
# objects.txt rows share one label, so the plugin numbers them by this order.
SEAL_FNS = {52, 54, 55, 56}
# objects.txt SubClass bits of a portal and a gate (entity_layout.h SubClassPortal, SubClassGate).
PORTAL_SUBCLASSES = 0x04 | 0x10

# Quest books a player reads rather than loots; the automap draws both with the
# tome cell (427) it gives Lam Esen's Tome. They loop the scroll cue until read.
TOME_FNS = {
    6,   # TowerTome, the Moldy Tome that opens the Forgotten Tower
    42,  # ArcaneTome, Horazon's journal that opens the way to the Canyon of the Magi
}

# Weapon racks loop their own recording instead of the chest one (user request,
# 2026-09-26). Armor stands (OperateFn 19) keep the chest loop.
WEAPON_RACK_FNS = {
    20,  # WeaponRack1/2/Left/Right
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


def wrap_comment(line, width=78):
    """Splits a long // comment at list commas, continuing lines indented."""
    lines = []
    while len(line) > width:
        cut = line.rfind(", ", 0, width)
        if cut < 0:
            break
        lines.append(line[:cut + 1])
        line = "//     " + line[cut + 2:]
    lines.append(line)
    return lines


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
    racks = []            # (class id, class name)
    tomes = []            # (class id, class name)
    radiant = []          # (class id, class name)
    mechanisms = []       # (class id, class name, OperateFn)
    seals = []            # (class id, class name)
    by_fn = collections.defaultdict(list)
    skipped_unselectable = 0
    lit = index["Lit0"]
    # Objects drawn with a portal's own art but no portal SubClass: Tyrael's portal to
    # Harrogath borrows the red portal's (a tester, 2026-10-27).
    token, sub_class = index["Token"], index["SubClass"]
    portal_tokens = {row[token].strip().lower() for row in rows
                     if len(row) > max(token, sub_class) and int(row[sub_class] or 0) & PORTAL_SUBCLASSES}
    portal_art = sorted((int(row[ident]), row[klass]) for row in rows
                        if len(row) > max(ident, token, sub_class) and row[ident].strip()
                        and not int(row[sub_class] or 0) & PORTAL_SUBCLASSES and row[token].strip().lower() in portal_tokens)
    for row in rows:
        # A chest lit from within: the purple glow of the quest and reward chests (a tester, 2026-10-17).
        if row[name].strip().lower() == "chest" and row[lit].strip() not in ("", "0"):
            radiant.append((int(row[ident]), row[klass]))
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
        if fn in QUEST_MECHANISM_FNS and any(row[slot] == "1" for slot in selectable):
            mechanisms.append((int(row[ident]), row[klass], fn))
            if fn in SEAL_FNS:
                seals.append((int(row[ident]), row[klass]))
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
        if fn in WEAPON_RACK_FNS:
            racks.append((int(row[ident]), row[klass]))

    entries.sort()
    if not entries:
        raise SystemExit("no lootable objects found -- is objects.txt the right file?")
    unmatched = set(QUEST_MECHANISM_FNS) - {fn for _, _, fn in mechanisms}
    if unmatched:
        raise SystemExit("OperateFn %s matched no selectable object -- did objects.txt change?" % sorted(unmatched))

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
    racks.sort()
    out.append("// Weapon racks, by OperateFn: %s." % ", ".join(class_name for _, class_name in racks))
    out.append("inline constexpr std::array<uint16_t, %d> WeaponRackObjects{" % len(racks))
    out.append("	" + ", ".join(str(class_id) for class_id, _ in racks))
    out.append("};")
    out.append("")
    out.append("constexpr bool WeaponRackObject(uint32_t classId) {")
    out.append("	if (classId > UINT16_MAX) return false;")
    out.append("	return std::binary_search(WeaponRackObjects.begin(), WeaponRackObjects.end(),")
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
    radiant.sort()
    out.append("// Chests that glow (Name chest, a Lit0 light): %s." % ", ".join(class_name for _, class_name in radiant))
    out.append("inline constexpr std::array<uint16_t, %d> RadiantObjects{" % len(radiant))
    out.append("\t" + ", ".join(str(class_id) for class_id, _ in radiant))
    out.append("};")
    out.append("")
    out.append("constexpr bool RadiantObject(uint32_t classId) {")
    out.append("\tif (classId > UINT16_MAX) return false;")
    out.append("\treturn std::binary_search(RadiantObjects.begin(), RadiantObjects.end(),")
    out.append("\t\tstatic_cast<uint16_t>(classId));")
    out.append("}")
    out.append("")
    out.append("// Drawn as a portal with no portal SubClass: %s." % ", ".join(class_name for _, class_name in portal_art))
    out.append("inline constexpr std::array<uint16_t, %d> PortalArtObjects{" % len(portal_art))
    out.append("	" + ", ".join(str(class_id) for class_id, _ in portal_art))
    out.append("};")
    out.append("")
    out.append("constexpr bool PortalArtObject(uint32_t classId) {")
    out.append("	if (classId > UINT16_MAX) return false;")
    out.append("	return std::binary_search(PortalArtObjects.begin(), PortalArtObjects.end(),")
    out.append("		static_cast<uint16_t>(classId));")
    out.append("}")
    out.append("")
    mechanisms.sort()
    out.extend(wrap_comment("// Quest mechanisms, by OperateFn: %s."
                            % ", ".join(class_name for _, class_name, _ in mechanisms)))
    out.append("inline constexpr std::array<uint16_t, %d> QuestMechanismObjects{" % len(mechanisms))
    out.append("	" + ", ".join(str(class_id) for class_id, _, _ in mechanisms))
    out.append("};")
    out.append("")
    out.append("// The modes in which each is still a quest step, bit n for mode n.")
    out.append("inline constexpr std::array<uint8_t, %d> QuestMechanismModes{" % len(mechanisms))
    out.append("	" + ", ".join("0x%02X" % sum(1 << mode for mode in QUEST_MECHANISM_FNS[fn]) for _, _, fn in mechanisms))
    out.append("};")
    out.append("")
    out.append("constexpr bool QuestMechanismObject(uint32_t classId) {")
    out.append("	if (classId > UINT16_MAX) return false;")
    out.append("	return std::binary_search(QuestMechanismObjects.begin(), QuestMechanismObjects.end(),")
    out.append("		static_cast<uint16_t>(classId));")
    out.append("}")
    out.append("")
    out.append("// True while a quest mechanism in this mode is still to be used.")
    out.append("constexpr bool QuestMechanismStep(uint32_t classId, uint32_t mode) {")
    out.append("	const auto found = std::lower_bound(QuestMechanismObjects.begin(), QuestMechanismObjects.end(), classId);")
    out.append("	if (found == QuestMechanismObjects.end() || *found != classId || mode >= 8) return false;")
    out.append("	return (QuestMechanismModes[found - QuestMechanismObjects.begin()] >> mode) & 1;")
    out.append("}")
    out.append("")
    seals.sort()
    out.extend(wrap_comment("// Chaos Sanctuary seals, by OperateFn: %s." % ", ".join(class_name for _, class_name in seals)))
    out.append("inline constexpr std::array<uint16_t, %d> SealObjects{" % len(seals))
    out.append("	" + ", ".join(str(class_id) for class_id, _ in seals))
    out.append("};")
    out.append("")
    out.append("// A seal's place in SealObjects counted from one, zero for any other object.")
    out.append("constexpr uint8_t SealOrdinal(uint32_t classId) {")
    out.append("	const auto found = std::find(SealObjects.begin(), SealObjects.end(), classId);")
    out.append("	return found == SealObjects.end() ? 0 : static_cast<uint8_t>(found - SealObjects.begin() + 1);")
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
    print("%s: %d lootable object classes from %d OperateFn groups, %d corpses, %d weapon racks, %d tomes, %d radiant chests, %d quest mechanisms, %d seals (%d skipped as never selectable)"
          % (args.out, len(entries), len(by_fn), len(corpses), len(racks), len(tomes), len(radiant), len(mechanisms), len(seals), skipped_unselectable))


if __name__ == "__main__":
    main()
