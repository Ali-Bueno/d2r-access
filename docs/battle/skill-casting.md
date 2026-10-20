# Native skill targeting and stationary attacks

2026-10-20: complete, offline-verified and installed. skill-attacks: 98 checks;
catalogs: 26,589 checks; full CTest: 126/126, 86.31s. Gameplay remains unconfirmed.
Parent contract: [native actions](../keyboard-actions.md).

## Behavior and controls

A uses the selected left skill; W uses the selected right skill. Self buffs,
including Werebear/Werewolf, cast once at the character's current cell. They
need neither an enemy nor a terrain/navigation scan. The native command is
attempted in town; the game owns town, mana, form and other cast restrictions.
No new success announcement implies the game accepted it.

Enemy requirement is independent of damage/reach. Optional-target attacks keep
automatic aiming at an on-screen enemy; without one they cast once at the
character instead of saying No enemies. Ordinary unit-only attacks retain that
localized report. Unknown ids never inherit Attack's enemy requirement. A
targeted non-attack effect uses one native unit cast if an enemy exists, otherwise
the no-enemy report; it does not become a hold loop.

Shift+A holds the left attack in place. It suppresses both chase and ranged
retreat and uses the native Shift/unit command instead of the ordinary unit
command that can approach the enemy. Without an enemy/terrain snapshot it strikes
the current cell repeatedly. Releasing A ends the hold; Shift can be pressed or
released during A. Charge/Whirlwind's intrinsic skill movement remains native.
In menus Shift+A retains equip/unequip/potion clicks. Physical pads are unchanged.

The saved keyboard-click-shift binding id is retained. Its combined context label
is translated in all thirteen catalogs, using the current mod language at
registration like existing binding labels. No setting/default/persistence or
synthetic controller is added. Existing Auto/manual language and target reports
remain authoritative. Catalog coverage does not establish translation fluency.

## Producers and decisions

tools/gen_skill_attacks.py generates src/generated/skill_attacks.h from existing
skills/weapons data. SkillAttackFor supplies attack/reach classification. An
active aurastate without leftskill identifies self buffs even with missiles or
damage, correcting the old Ranged classification of Chilling Armor, Thunder
Storm, Blade Shield, Blaze and Armageddon. Passive, aura, summon and alternate
target rules keep their established priority.

SkillRequiresEnemy independently uses TargetableOnly, excluding pet/ally/corpse/
item targets. Charge, Leap Attack and Whirlwind start functions (srvstfunc
31/41/38, including other rows using them) allow ground targets despite that
coarse unit flag. Metadata never validates every skill-specific cast rule; native
execution does that. This uses data fields, not localized-name comparisons.

src/battle/skill_cast.h owns missing-target policy/native stationary command
selection. KeyboardService::BeginFight uses one-shot casts for non-attacks;
Fight chooses the current target or missing-target policy. Its stationary branch
returns before chase/retreat. NativeActions::SkillHere reads only the player's
path position through ReadMoveState, converts 16.16 to its integer current cell,
and calls SkillAt. Unreadable position refuses casting; it never uses OS clicks.

## Native evidence and ownership

D2R 3.3.93787, base 0x140000000; addresses below are RVAs. Existing whole-function
fingerprints guard PlayerCommand, command packets, event dispatch and selected
skill reads before actions are available.

| Producer | Verified behavior |
| --- | --- |
| Mouse down 0x83FC0 / modifier word 0x8AFD0 | Passes the modifier word to ClickDispatch 0xFE3B0 |
| ClickDispatch 0xFE610–0xFE61C | Low modifier byte sets action flag 0x20 |
| SendSkill 0x102B70, branch 0x102C88–0x102D10 | Flag 0x20 selects stationary left/right unit 0x07/0x0E; Charge/Whirlwind use extended 0x0A/0x11 |
| PlayerCommand 0xFABE0 | 6/7 share the left-unit local event; D/E use the right-unit path |
| SendCommandPacket 0xFC000 | Preserves distinct command ids for unit targets; A/11 use the extended path |

Source-bearing disassembly stays outside every Git tree in workspace
re/actions/input/dis/ and re/keyboard-combat-2026-10-20/. Documents contain
behavior/locators only. The own latest log records Hex Bane one-shot calls but
no Werebear cast: the example is not claimed reproduced there. The old terrain
dependency and self-buff misclassification are independently verified code defects.

Live reads/native calls remain on the UI owner. Input callbacks publish atomic
press/hold intent; no native pointers enter workers. Existing 200 ms fight cadence
and 250 ms strike deadline remain. Self casts need bounded position reads only.
Enemy aiming retains the existing documented UI terrain/entity scan cost and
architecture exception; this change establishes no runtime response baseline.
PRISM/slow log output keep their workers. No speech-text deduplication is added.
Existing lifecycle/menu/stop/physical-release paths retire fight/movement state.

## Extension and verification

Regenerate metadata after supported data changes and inspect changed rows/native
behavior. Keep enemy requirement independent from reach/damage and preserve the
stationary branch before movement. Reuse native verification/cancellation.

skill-attacks-tests covers self/damaging buffs, melee, targeted non-attacks, pet
targets, projectiles/novas/movement skills, unknown ids, stationary repetition and
exact command families. Full/deploy results belong in STATUS/HANDOFF/local evidence.

First gameplay: use Werebear or another self buff with no enemy, outside town
then in town. It must attempt without No enemies; native town rejection is valid.
Hold Shift+A with a normal weapon at an enemy outside melee reach: no chase or
retreat. Release Shift while retaining A to resume approach, then release A.
Also check stationary swings without enemies and menu Shift+A equip. Actual
animation/cast response, modifier transitions and audible ordering remain pending.
