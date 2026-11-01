# Skill targeting and attacking in place

Ported by hand from bladestorm360's 97cb7c4 (2026-11-01); not tested live.

- **Enemy required or not.** `SkillRequiresEnemy` (generated from `skills.txt`
  `TargetableOnly`, minus pet/ally/corpse/item targets and the ground-targetable
  start functions 31/38/41: Charge, Whirlwind, Leap Attack) is separate from
  attack/reach. Active self states (an armour that also strikes back) are not
  attacks: Chilling Armor, Thunder Storm, Blade Shield, Blaze, Armageddon.
  `tools/gen_skill_attacks.py` makes `src/generated/skill_attacks.h`.
- **A or W with a non-attack** (a buff, Werebear) casts once at the player's cell
  (`NativeActions::SkillHere`): no scan, no lock, no "No enemies". The game decides
  town, mana and form. A non-attack that needs an enemy (Blood Oath) casts at the
  locked one, else says "No enemies".
- **An attack with no enemy**: an optional-target attack casts once where you
  stand; a unit-only attack says "No enemies" as before. An unreadable skill is
  still fought with as the plain Attack, as on main (the branch had dropped that).
- **Shift+A, held**: left skill in place. `SkillCast::OnUnit` picks the game's
  stand-still unit command (0x07, 0x0E; Charge and Whirlwind 0x0A, 0x11); it
  beats chase and retreat, and with no enemy strikes the player's cell. Shift is
  polled each tick. In a menu Shift+A is still the equip click.
