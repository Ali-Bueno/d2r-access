# Silver items

A white-name item (low quality, normal, superior) the game draws gray is spoken with the colour
word "silver" after its name, on the ground and in the inventory headline. Switch `silver-items`
(Ctrl+S list, default on). Nothing about sockets or ethereal is said.

## The game's rule (D2R 3.3.93787)

The name-colour function is at RVA `0x2BD360` (item unit, items-table row, quality; called from
the ground label at `0x2BE0A6`, the tooltip name at `0x231F6D`, `0x2A9644`). MEASURED from the
decrypted image in `re/D2R_live.exe`:

- Quality 4..9 (magic and better) go through a jump table to their own colour.
- Any other quality: ItemData flag `0x800` (socketed) or `0x400000` (ethereal) gives colour 5
  (gray); otherwise white.
- Then flag `0x100` (broken) overrides with colour 1 (red).
- Flags are the dword at ItemData + `0x18` (`ItemFlags` in `src/ground_item_layout.h`).

The function does not test the runeword flag (`0x4000000`), so a runeword item (always socketed)
would be gray here, but the name builder at `0x379A44`/`0x37E543` has its own runeword path that
was not decoded. Runewords are therefore left out of "silver" (`IsSilver`), as are broken items.
Not modelled: the later overrides by the items-table byte at row `+0x156` (colour `0x1C`) and
stat `0x4B` (colour `0x1A`).

Table-name paths (an item named from its class alone: held-item recall, equipment census) have no
flags and are unchanged. Live check: an ethereal or socketed Leather Armor on the ground and in
the inventory should end in "silver"; the `[item]` log line shows the inventory headline.
