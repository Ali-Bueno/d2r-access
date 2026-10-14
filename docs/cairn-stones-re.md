# Cairn Stone order: native evidence

For D2R 3.3.93787, the decoded Scroll of Inifuss sends the client packet `0x50` with quest ID 4 and five `uint16` stone indices at offsets 3, 5, 7, 9, and 11. The server packet builder at RVA `0x5D9710` writes each stone's object class minus 17. The five classes are 17–21 (`StoneAlpha` through `StoneLambda` in the installed `objects.txt`), so packet value 0 is class 17, and so on. The stock server checks a clicked stone's class against the next class in its generated permutation at RVA `0x5D9D50`. The corresponding open-source classic quest code is [D2MOO A1Q4.cpp, revision 5596f5c](https://github.com/ThePhrozenKeep/D2MOO/blob/5596f5cb6c5251a0a07c6637d26458b06099d516/source/D2Game/src/QUESTS/ACT1/A1Q4.cpp), but the offsets and client behavior below were checked against the installed D2R image.

The live D2R opcode dispatch table at RVA `0x22BBC40` routes `0x50` through wrapper `0x12CC30` (length 15) to handler `0x232EA0`. Its quest-4 case expands the five packet indices to `uint32` and calls setter `0x245210`, which writes the order to `0x2A98B20`. Opening the decoded scroll sets a pending flag at `0x2364370`; the setter clears it. The scroll renderer at `0x245436` iterates the order by rank and assigns that rank to the corresponding `Number1`–`Number5` widget's frame at widget offset `0x90` through setter `0x859700`. Thus packet order contains **stone index by activation rank**, while the five number widgets contain the inverse, **rank by stone index**.

This mapping was checked while the user displayed the decoded scroll. The live global held `[2,4,0,1,3]`; the five visible number frames held `[2,3,0,4,1]`, its exact inverse. The required class order for that session was therefore 19, 21, 17, 18, 20. The full mapped image was read-only captured from the actual `D2RLoader.exe` game process (base `0x140000000`, raw file offsets equal RVAs, 65,503,232 bytes, no unreadable pages). Three complete function hashes matched prior live game code before analysis.

`NativeCairn` hooks the verified packet handler and stores a valid permutation only after `LocalPlayerReady`. The cache clears on game join/leave and refuses another session's order or a capture that races a reset. It does not infer an order from persistent client globals. If the hook's complete-function fingerprints change, it disables scroll-order reading. The sequence is spoken only while the deciphered scroll is open; closing and reopening permits a repeat. World cues depend only on each stone's neutral mode, and world names contain its fixed symbol rather than its position in the solution. This preserves the player's choice, as explicitly requested on 2026-09-24.

The installed `global/ui/Layouts/ScrollOfInifussHD.json` fixes Symbol1–5 to
`SoI_DropUp`, `SoI_I`, `SoI_DropDown`, `SoI_II`, `SoI_Zero`, corresponding to
object classes 17–21. Spoken labels are upward drop, one, downward drop, two,
and zero; they stay the same when the random activation order changes.

## Scanner visibility correction — 2026-09-24

The user activated the stones using the pathfinder, but the directional scan
did not list them. The 06:27 session log shows four empty directional scans at
player position (5113,5104), followed by catalogue entries for all five symbols;
the beacon later identifies stone 69 at (5109,5104). The log establishes the
scan/catalogue discrepancy, although it does not record each rejected ray cell.

All five installed stone rows have SizeX=3, SizeY=3 and HasCollision0/2=1. The
old sight ray excluded only the target centre, letting its own 3x3 collision
footprint hide it. Object dimensions now come from the live table row, and the
sight test discounts only OBJECT (0x0400) inside that target's exact rectangle.
Walls, doors and other blocking bits remain effective; range and sectors are
unchanged. Door objects keep their existing point-target sight policy.

`tools/objects_containers.py` verifies uint32 SizeX at row+0x90, SizeY at +0x94,
and eight HasCollision bytes at +0xE0 against all 587 installed text/binary rows.
Native getters 0x3481E0, 0x3482C0 and 0x34BBE0 corroborate those fields. Mask
getter 0x349890 returns 0x400 for the Cairn rows. Rectangle setter 0x363C80 uses
`left = x - (width >> 1)`, `right = left + width - 1`, and the same rule for Y.
Their complete-function fingerprints are recorded in `EntityLayout::Checks`.

The wrapper passes a 15-byte stack copy to dispatcher `0x232EA0`, whose decompiled signature is `void (packet*)` and whose quest-4 case calls the order setter before returning. The dispatcher provides a normal 283-byte function entry for the inline hook, while the setter is only 30 bytes long. The hook copies the packet before calling the original, then accepts it only if the same lifecycle epoch is still ready. Its `void(const uint8_t*)` callback preserves the stock x64 calling convention and return type.

## The symbol on focus — 2026-10-14

A tester: focusing a stone said only "Cairn Stone". The world target label is
the game's, the same for all five; it now carries the scan's symbol when the
game's own target or a clearly nearest stone says which one it is
([target-sight.md](target-sight.md#the-games-own-target-2026-10-14)).

## The order was read, then cut off — 2026-10-13

A tester (controller) heard no order. His log shows it read on all four openings and queued
behind "Scroll of Inifuss", then cut off within 20 ms: the game reopens the inventory beside the
scroll and moves its cursor to another item ("Viridian Small Charm"), which the interaction
reader announces as a new focus, interrupting. That cursor move is also the "jump to another
slot" he reported. The order now goes through `ScreenReader::Lead`: for 1.5 s an interrupting
phrase queues behind it and Stop waits. On the pad the scroll opens on a held press ("Hold ? to
Read", ui-controller.json `ReadScrollController`); a short one picks the scroll up.
