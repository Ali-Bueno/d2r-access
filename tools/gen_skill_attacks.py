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
  an active self state, without leftskill = 1   -> not an attack: a buff is
                                                   applied to the caster
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

`Weapon` needs the weapon: the header also lists the codes of the weapons that
shoot, the bows and crossbows of weapons.txt (type bow, xbow or abow). A
throwing weapon is not one of them: the plain Attack swings it, and Throw is a
skill of its own.

Known limits, stated rather than patched with a list of names:
  - How FAR each ranged skill carries is not in this table. The reach is one
    policy distance in the plugin, not a per-skill fact.
  - Skills cast around the character (Nova, Frost Nova, Poison Nova, War Cry)
    come out Ranged: they launch missiles and do damage. They are attacks; they
    are used from the policy distance like any other.
  - Native TargetableOnly describes unit targeting, independently of damage.
    Charge/Leap Attack/Whirlwind start functions also allow ground targeting;
    they do not require an enemy, despite that coarse flag.

Usage:
    python tools/gen_skill_attacks.py [--excel game-data/global/excel]
                                      [--out src/generated/skill_attacks.h]

skills.txt and weapons.txt come from the game's CASC storage:
    tools/casc_extract.exe extract "<game dir>" game-data "excel\skills.txt"
and land in game-data/global/excel/. game-data/ is gitignored; the generated
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
SHOOTING = ("bow", "xbow", "abow")


def packed(code):
    """The item code as the game stores it: little-endian, space-padded."""
    text = code.ljust(4)
    return sum(ord(ch) << (8 * i) for i, ch in enumerate(text))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--excel", type=Path, default=Path("game-data/global/excel"))
    parser.add_argument("--out", type=Path, default=Path("src/generated/skill_attacks.h"))
    args = parser.parse_args()

    with io.open(args.excel / "skills.txt", encoding="utf-8-sig", errors="replace", newline="") as source:
        rows = list(csv.reader(source, delimiter="\t"))
    header = rows[0]
    at = {name: header.index(name) for name in
          ("skill", "*Id", "passive", "aura", "summon", "range", "leftskill", "itypea1", "aurastate", "TargetableOnly", "srvstfunc") + MISSILES + DAMAGE + OTHER_TARGETS}

    def cell(row, name):
        return row[at[name]].strip() if at[name] < len(row) else ""

    def is_set(row, name):
        return cell(row, name) not in ("", "0")

    kinds = {}
    targets = {}
    corpses = set()
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
        elif cell(row, "aurastate") and not left:
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
        # TargetableOnly is independent of range/damage. A missile or damage
        # does not make a self cast (armours, novas) require a monster. Other
        # unit targets belong to their own native rules, not enemy navigation.
        targets[skill] = (cell(row, "TargetableOnly") == "1"
                          and cell(row, "srvstfunc") not in ("31", "38", "41")
                          and not any(is_set(row, name) for name in OTHER_TARGETS))
        if is_set(row, "TargetCorpse"):
            corpses.add(skill)

    with io.open(args.excel / "weapons.txt", encoding="utf-8-sig", errors="replace", newline="") as source:
        weapons = list(csv.reader(source, delimiter="	"))
    code_at, type_at = weapons[0].index("code"), weapons[0].index("type")
    shooting = sorted(packed(row[code_at].strip()) for row in weapons[1:]
                      if len(row) > max(code_at, type_at) and row[code_at].strip() and row[type_at].strip() in SHOOTING)
    if not shooting:
        raise SystemExit("weapons.txt: no bow or crossbow found")

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
    out.append("#include <algorithm>")
    out.append("#include <array>")
    out.append("#include <cstddef>")
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
    out.append("// Enemy targeting is independent of attack/reach classification. Native")
    out.append("// TargetableOnly, excluding pet/ally/corpse/item targets. Unknown ids never")
    out.append("// invent an enemy requirement; the native cast still decides validity.")
    out.append("inline constexpr std::array<bool, %d> EnemyTargets{{" % (last + 1))
    for start in range(0, last + 1, 20):
        out.append("\t" + ",".join("true" if targets.get(skill, False) else "false"
                   for skill in range(start, min(start + 20, last + 1))) + ", // %d" % start)
    out.append("}};")
    out.append("constexpr bool SkillRequiresEnemy(int skill) {")
    out.append("\treturn skill >= 0 && static_cast<size_t>(skill) < EnemyTargets.size()")
    out.append("\t\t&& EnemyTargets[static_cast<size_t>(skill)];")
    out.append("}")
    out.append("")
    out.append("// Native TargetCorpse: a skill that acts on a dead monster and does nothing at")
    out.append("// a bare cell, sorted. Whether a given corpse suits it is the game's to decide.")
    out.append("inline constexpr std::array<int, %d> CorpseTargets{{" % len(corpses))
    out.append("	" + ", ".join(str(skill) for skill in sorted(corpses)) + ",")
    out.append("}};")
    out.append("constexpr bool SkillTargetsCorpse(int skill) {")
    out.append("	return std::binary_search(CorpseTargets.begin(), CorpseTargets.end(), skill);")
    out.append("}")
    out.append("")
    out.append("// The weapons that shoot: the bows and crossbows of weapons.txt, by item code")
    out.append("// as the game stores it (little-endian, space-padded), sorted. With one in")
    out.append("// hand a Weapon skill is ranged.")
    out.append("inline constexpr std::array<uint32_t, %d> ShootingWeapons{{" % len(shooting))
    for start in range(0, len(shooting), 6):
        out.append("	" + " ".join("0x%08Xu," % code for code in shooting[start:start + 6]))
    out.append("}};")
    out.append("")
    out.append("// A NUL byte is taken as the space the game pads a short code with.")
    out.append("constexpr bool ShootingWeapon(uint32_t code) {")
    out.append("	for (int shift = 0; shift < 32; shift += 8)")
    out.append("		if (((code >> shift) & 0xFFu) == 0) code |= 0x20u << shift;")
    out.append("	return std::binary_search(ShootingWeapons.begin(), ShootingWeapons.end(), code);")
    out.append("}")
    out.append("")
    out.append("}")
    out.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    io.open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print("%s: %d skills -- %d melee, %d ranged, %d weapon, %d not attacks; %d shooting weapons"
          % (args.out, len(kinds), counts[MELEE], counts[RANGED], counts[WEAPON], counts[NONE], len(shooting)))
    for kind in (MELEE, RANGED, WEAPON):
        print("%s: %s" % (kind, ", ".join(names[skill] for skill in sorted(kinds) if kinds[skill] == kind and skill < 403)))


if __name__ == "__main__":
    main()
