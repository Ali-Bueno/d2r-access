# Quest objects

The world objects a quest needs used, what the mod does with each, and what
still needed a live check (validated in play: testers finished Acts I to V with it, 2026-11-01). D2R 3.3.93787. Classes are `objects.txt` `*ID`,
handlers its `OperateFn`. "Classic" means the D2MOO reconstruction of 1.10,
used as a guide only and not read in D2R.

## What the mod does

While an object below is still a quest step:

- it loops the quest cue (`quest item.ogg`, 45 subtiles, the two nearest get voices);
- the scanner lists it in its own **quest** category, after the rooms and
  before the NPCs, and L3 beacons to it from there;
- D takes it before nearer ordinary things in reach, such as the Ancients'
  statues, NPCs and chests (`EntityRadar::UseBefore`). Comma and full stop
  still choose anything.

The quest-step modes come from `tools/gen_lootables.py` (`QUEST_MECHANISM_FNS`,
read by `Generated::QuestMechanismStep`). In any other mode the object is an
ordinary object if the game still lets it be selected, and gone if it doesn't.
Quest item sources, unread tomes and neutral Cairn Stones join the same
category; before this change they were listed as treasure or as objects. No
quest flags are read: the cue means the object is unused, not that the quest
needs it now.

| Act, quest | Object (class, OperateFn) | Game label | Quest step | Notes |
|---|---|---|---|---|
| I, The Search for Cain | Gibbet (26, 10) | Cain's Gibbet | neutral | |
| II, The Tainted Sun | TaintedSunShrine (149, 24) | Tainted Sun Altar | neutral | attackable |
| II, The Seven Tombs | SevenTombsReceptacle (152, 25) | orifice | neutral | takes the Horadric Staff; opened (2) when done (classic) |
| III, Blade of the Old Religion | GidBInnDecoy (252, 31) | Gidbinn | neutral | wakes a guardian that drops the Gidbinn (classic); not loot |
| III, Khalim's Will | BeneathTheCityLever (367, 45) | Sewer Lever | neutral | Kurast Sewers Level 1; opens the stairs |
| III, Khalim's Will | BeneathTheCityStairs (366, 44) | Sewer Stairs | — | an **exit** once open, see below |
| III, The Guardian | CompellingOrb (404, 53) | Compelling Orb | neutral | Khalim's Will in hand, two uses |
| IV, Hell's Forge | SoulStoneForge (376, 49) | Hellforge | neutral, opened | the Soulstone, then the hammer in hand, three hits (classic) |
| IV, Terror's End | DiabloSeal1–5 (392–396; 54, 52, 55, 52, 56) | Seal 1 … Seal 5 | neutral | numbered and counted, see below |
| V, Prison of Ice | AnyaYetAgain (558, 67) | Frozen Anya | neutral | needs Malah's potion (classic) |
| V, Rite of Passage | AncientsAltar (546, 65), ColossalAltar (573, 74) | Altar of the Heavens | neutral | D takes it before the statues |

The Hellforge stays a quest step in mode 2, where it holds the Soulstone: the
client asks for the hammer in hand in that mode (`0xFA6DA`). In classic code it
ends in mode 3, where the game does not let it be selected.

## Attackable objects

The Orb, the Hellforge and the Tainted Sun altar are attackable. When clicked,
they get the same use command (`0x41`) that D sends, so D needs no change.
Evidence: [keyboard actions](keyboard-actions.md#attackable-objects).

## Chaos Sanctuary seals

The game labels all five "Seal". The mod adds each one's place in class order
(DiabloSeal1–5, `Generated::SealOrdinal`), for example "Seal 3". It does not
say which boss a seal brings. In classic code seals 1, 3 and 5 (handlers 54,
55, 56) spawn the seal bosses, and 2 and 4 (handler 52) only mark themselves.
This is not verified in D2R.

The count is made from object modes: `QuestSeals` records every seal the scan
reads, including opened ones, which are no longer selectable. A seal seen open
in this game is counted off, and an unseen seal counts as shut. "Seals left: N"
is spoken in two cases:

- when a seal seen shut is seen open. The quest service speaks it, and waits
  while a menu has the speech.
- after a seal's name in the scanner.

At zero it says "All seals open". In multiplayer, a seal opened by someone else
counts once the player has seen it.

## Sewer stairs

Handler `0x5E9DA0` (50 bytes) does this: in mode 2 it calls `0x5A11A0`, which
warps the player through the room's warp tile. That tile is LvlWarp 60
("Act 3 Sewer Down", level 92 to 93) and is NoInteract, so the stairs are its
only click. The stairs can be selected in mode 2 only. The lever (`0x5E9C80`)
sets their quest data. The operate table (`0x23A51C0`) is filled at runtime, so
fn 44 → `0x5E9DA0` was identified by elimination, not read from the table:

- it is one of the three callers of `0x5A11A0`;
- the other two are the Ancients' door and the Summit door;
- the classic table agrees.

The opened stairs are now an exit (`RoomWarpOperateFn`). They are kept only
for the current game, with no place memory, because a new game starts them
shut. Earlier notes placed these in Act 2 with Radament. They are in Act 3.

## Live checks still needed

1. Orifice: how the staff goes in (an item slot panel?), whether that panel is
   read, and that the cue stops afterwards.
2. Anya: whether the "needs a potion" message (string 20131) is spoken, and
   that the cue stops once she thaws.
3. Seals: which seal brings which boss, that the count reaches "All seals
   open", and that Diablo comes.
4. D at the Orb (twice, with Khalim's Will), the Hellforge (Soulstone, then the
   hammer three times) and the Tainted Sun altar: whether the swing plays and
   the hits count. The game's own error when Khalim's Will is not in hand is
   not spoken by the mod.
5. Arreat Summit: which altar row (546 or 573) is used, and whether D takes it
   before the statues.
6. Sewer stairs: whether D, after the lever, takes the player to Sewers Level 2.
7. Gibbet, Gidbinn altar and Hellforge: whether the cue stops after use.
