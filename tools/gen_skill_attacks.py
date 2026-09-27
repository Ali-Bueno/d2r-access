#!/usr/bin/env python3
r"""gen_skill_attacks.py -- Generates src/generated/skill_attacks.h: for each
skill id, whether the skill is an attack, and whether it is struck from beside
the monster or from a distance.

Why this exists
---------------
The keyboard's fighting keys use whatever skill the player has selected (the
user, 2026-09-27: "attack should be whatever skill is active as long as it's an
attack skill"), and a ranged one is used from its range, not from beside the
monster ("so they aren't running into melee range with ranged attacks"). Both
need to know what kind of skill a skill id is, and skills.txt says so.

The rules, in order. Each is a column of skills.txt, not a list of names:

  passive = 1, aura = 1, or a `summon`          -> not an attack
  TargetPet / TargetAlly / TargetCorpse /
  TargetItem = 1, without leftskill = 1         -> not an attack: its target is
                                                   not a monster. With
                                                   leftskill the skill strikes
                                                   monsters too (Holy Bolt)
                                                   and goes on
  range = both                                  -> Weapon: melee or ranged by
                                                   the weapon in hand (Attack)
  range = h2h, leftskill = 1                    -> Melee
  range = rng, leftskill = 1 or does damage     -> Ranged
  range = none, a weapon skill (itypea1 set)
      with leftskill = 1                        -> Melee: Charge and Whirlwind
                                                   carry the character to the
                                                   monster themselves
  range = none, launches a missile and does
      damage                                    -> Ranged
  anything else                                 -> not an attack

`leftskill` is the game's own rule for what may sit on the attack button, which
is why it gates the weapon skills: Find Potion and Find Item are `h2h` and are
not attacks.

Known limits, stated rather than patched with a list of names:
  - How FAR each ranged skill carries is not in this table. The reach is one
    policy distance in the plugin, not a per-skill fact.
  - Skills cast around the character (Nova, Frost Nova, Poison Nova, War Cry)
    come out Ranged: they launch missiles and do damage. They are attacks; they
    are used from the policy distance like any other.
  - Armours that strike back (Chilling Armor, Blade Shield, Thunder Storm) come
    out Ranged for the same reason.

Usage:
    python tools/gen_skill_attacks.py [--excel game-data/global/excel]
                                      [--out src/generated/skill_attacks.h]

skills.txt comes from the game's CASC storage:
    tools/casc_extract.exe extract "<game dir>" game-data "excel\skills.txt"
and lands in game-data/global/excel/. game-data/ is gitignored; the generated
header is what gets committed.
"""
import argparse
import csv
import io
from pathlib import Path

NONE, MELEE, RANGED, WEAPON = "None", "Melee", "Ranged", "Weapon"
MISSILES = ("srvmissile", "srvmissilea", "srvmissileb", "srvmissilec")
DAMAGE = ("MinDam", "EMin", "SrcDam")
OTHER_TARGETS = ("TargetPet", "TargetAlly", "TargetCorpse", "TargetItem")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--excel", type=Path, default=Path("game-data/global/excel"))
    parser.add_argument("--out", type=Path, default=Path("src/generated/skill_attacks.h"))
    args = parser.parse_args()

    with io.open(args.excel / "skills.txt", encoding="utf-8-sig", errors="replace", newline="") as source:
        rows = list(csv.reader(source, delimiter="\t"))
    header = rows[0]
    at = {name: header.index(name) for name in
          ("skill", "*Id", "passive", "aura", "summon", "range", "leftskill", "itypea1") + MISSILES + DAMAGE + OTHER_TARGETS}

    def cell(row, name):
        return row[at[name]].strip() if at[name] < len(row) else ""

    def is_set(row, name):
        return cell(row, name) not in ("", "0")

    kinds = {}
    names = {}
    for row in rows[1:]:
        number = cell(row, "*Id")
        if not number.isdigit():
            continue
        skill = int(number)
        if skill in kinds:
            raise SystemExit("skills.txt: skill id %d defined twice" % skill)
        names[skill] = cell(row, "skill")
        reach = cell(row, "range")
        left = cell(row, "leftskill") == "1"
        damage = any(is_set(row, name) for name in DAMAGE)
        missile = any(cell(row, name) for name in MISSILES)
        if is_set(row, "passive") or is_set(row, "aura") or cell(row, "summon"):
            kind = NONE
        elif any(is_set(row, name) for name in OTHER_TARGETS) and not left:
            kind = NONE
        elif reach == "both":
            kind = WEAPON
        elif reach == "h2h":
            kind = MELEE if left else NONE
        elif reach == "rng":
            kind = RANGED if left or damage else NONE
        elif cell(row, "itypea1") and left:
            kind = MELEE
        elif missile and damage:
            kind = RANGED
        else:
            kind = NONE
        kinds[skill] = kind

    last = max(kinds)
    counts = {kind: sum(1 for value in kinds.values() if value == kind) for kind in (NONE, MELEE, RANGED, WEAPON)}

    out = []
    out.append("// generated by tools/gen_skill_attacks.py from game-data -- do not edit")
    out.append("//")
    out.append("// Whether each skill is an attack, and whether it is struck from beside the")
    out.append("// monster or from a distance, by skill id, from skills.txt. The rules and")
    out.append("// their known limits are in the generator. %d skills: %d melee, %d ranged,"
               % (len(kinds), counts[MELEE], counts[RANGED]))
    out.append("// %d by the weapon in hand, %d not attacks." % (counts[WEAPON], counts[NONE]))
    out.append("#pragma once")
    out.append("")
    out.append("#include <array>")
    out.append("#include <cstdint>")
    out.append("")
    out.append("namespace D2RAccess::Generated {")
    out.append("")
    out.append("// Weapon: melee or ranged by the weapon in hand, as the plain Attack is.")
    out.append("enum class SkillAttack : uint8_t { None, Melee, Ranged, Weapon };")
    out.append("")
    out.append("// By skill id; an id skills.txt does not define is not an attack.")
    out.append("inline constexpr std::array<SkillAttack, %d> SkillAttacks{{" % (last + 1))
    letters = {NONE: "N", MELEE: "M", RANGED: "R", WEAPON: "W"}
    out.append("#define N SkillAttack::None")
    out.append("#define M SkillAttack::Melee")
    out.append("#define R SkillAttack::Ranged")
    out.append("#define W SkillAttack::Weapon")
    for start in range(0, last + 1, 20):
        piece = ",".join(letters[kinds.get(skill, NONE)] for skill in range(start, min(start + 20, last + 1)))
        out.append("\t%s, // %d" % (piece, start))
    out.append("#undef N")
    out.append("#undef M")
    out.append("#undef R")
    out.append("#undef W")
    out.append("}};")
    out.append("")
    out.append("constexpr SkillAttack SkillAttackFor(int skill) {")
    out.append("\treturn skill >= 0 && static_cast<size_t>(skill) < SkillAttacks.size()")
    out.append("\t\t? SkillAttacks[static_cast<size_t>(skill)] : SkillAttack::None;")
    out.append("}")
    out.append("")
    out.append("}")
    out.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    io.open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print("%s: %d skills -- %d melee, %d ranged, %d weapon, %d not attacks"
          % (args.out, len(kinds), counts[MELEE], counts[RANGED], counts[WEAPON], counts[NONE]))
    for kind in (MELEE, RANGED, WEAPON):
        print("%s: %s" % (kind, ", ".join(names[skill] for skill in sorted(kinds) if kinds[skill] == kind and skill < 403)))


if __name__ == "__main__":
    main()
