# Controller glyphs in the game's text (2026-10-19)

The game writes buttons into its strings as private-use code points of its controller font
("You have died\nPress  to continue.", "Hold  to Open"). `ControllerText` (`src/controller_text.*`)
names them; `UiText::Clean` drops any it is not asked to name. A tester (build 2026-10-18-2) heard
"Press to continue" on the death screen and "Hold to Open" on the Horadric Cube: the mod knew only
U+E000..E012. Established statically from `re/D2R_live.exe` (game 3.3.93787); RVAs below.

## Two ranges

- **U+E000..E012: physical controls**, in the font's own order (`0x45E80` registers glyph to
  sprite): D-pad, D-pad up, down, left, right, Menu, View, left stick, L3, right stick, R3, LB, RB,
  LT, RT, then the four face buttons by position (bottom, right, left, top = A, B, X, Y on Xbox).
  `0x872E50` maps the button legend's preset names to these codes; `0x1441F0` formats skill-hotkey
  glyphs from XInput masks, consistent with the order.
- **U+E013..E02A: UI actions.** `0x45BC0` fills a code-to-action map (`0x253FA98`) from 24 pairs:
  E013 -> 0, E014 -> 0x17, E015..E02A -> 1..0x16. The per-glyph draw `0x90AF50` asks
  `0x13CBB0(inputManager, player, action)` for the bound button's physical glyph (0 draws nothing).
  The bindings are set only in `0x13D0C0` (sole caller of the insert `0x13C8C0`), so the player
  cannot remap them on PC; it takes the confirm button from `0x120FF40`, which on PC returns 0x1000
  (A): select is A, back is B.

| Code | Action | Button (PC) |
|---|---|---|
| E013 | select | A |
| E014 | back | B |
| E015, E016 | option 1, 2 | X, Y ("Hold Y to compare") |
| E017, E018 | option 3, 4 | L3, R3 |
| E019, E01A | unresolved | none bound |
| E01B, E01C | tab left, right | LT, RT |
| E01D | pause | Menu ("Press Menu to continue", level up, quest updated) |
| E01E | menu | View |
| E01F..E022 | belt 1-4 | D-pad left, up, down, right (as `ControllerHelpBelt1..4`) |
| E023..E026 | navigate | the stick; nothing drawn |
| E027, E028 | (likely cycle left/right) | LB, RB |
| E029, E02A | unresolved | none bound |

E031..E037 are the immunity and desecrated icons (`0x46150`), not buttons. Action names come from
the legend's name-to-action map at `0x872A00` and were matched by string length: the buttons are
certain, some names are not.

## What the mod does

`ControllerText::Button` maps an action code to its button's physical code through a fixed table,
then names it as before (Xbox labels, the mod's translated `controllerButtons`). Readers that decode:
the death screen, notifications (`UiNotifications`: level up, quest updated), item tooltips
(`TooltipText::Read`, for any line that has words: a lone symbol, like the stash label's U+E013,
stays out), the cube's own hint and every reader that already called `ControllerText`.

## Not done

- **Controller families.** `0x90A820` builds `Controller/ControllerButtons/<family>/<sprite>` from
  `0x13CFA0` (global `0x22BD530`; 9 detects the device; jump table `0x90A938`: xbox, ps4, ps5, switch,
  steam). The mod always says Xbox labels. Calling `0x13CFA0` could name Cross or Circle.
- **Thumbstick swap** (`0x13CC20` swaps L3/R3 under flag `0xD2E490`, probably the game's swap
  option) would change E017/E018, which no string uses.
- Runtime resolution (`0x13CE90` input manager, `0x8B2D0` player index, `0x13CBB0`) is not needed
  while PC bindings are fixed; none of these has a fingerprint check in the mod.
