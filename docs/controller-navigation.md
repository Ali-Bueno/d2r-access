# Controller menu and skill navigation follow-up

Target: D2R 3.3.93787, 2026-09-15. Native C++ / D2RLoader, existing PRISM sink.
The user reported silent main-menu actions after leaving a game, free cursor
movement with both the D-pad and left stick in skills, and only three Options tabs.

## Menu focus and value controls

The 22:58 log records Options/Videos/Exit indicator rectangles with zero matches
after returning to the front end. The old reader relied on cached geometry and
panel-open order. `native_focus.cpp` now resolves the actual native focused
object's name and complete parent path against the generated layout catalog.
It rejects hidden objects and wrong ancestors, including another panel instance.
Geometry remains a fallback; dropdown popup rows retain their existing reader.

DropdownListWidget, CarouselWidget and slider classes are value controls even
when their native base is ButtonWidget. The generic action reader must leave them
to the established value reader; otherwise their caption/help replaces the
selected value. No options or game configuration are changed.

## Discrete skill navigation

`input_direction.cpp` exposes D-pad-only press/held state independently of sticks
and keyboard arrows. `skill_navigation.cpp` discovers visible class-skill buttons
under the native SkillButtonContainer. The actual generated cell names supply
row/column, as documented in [progression.md](progression.md).

General and item-granted skills use their native focus rectangle centres because
their names do not encode cells. One cardinal press selects one skill. Movement prefers the same row/column,
skips gaps, uses the closest perpendicular alignment when the line has ended,
and stops at the outer boundary. The first press between skills enters at the
first visible cell. A stick selection becomes the starting point for the next
D-pad gesture. Hidden branches and other panels are excluded. Stock branch-tab
controls and skill activation remain available; navigation never clicks a skill.
General/item-granted skills retain their description reader; row/column speech
is only added when the game provides class-tree cell names.

The adapter places the real cursor at the native focus rectangle's centre, then
calls the existing selection method. While that D-pad gesture is held, the
native cursor-update hook delegates to the stock cursor reset, cancelling its
old interpolation. This prevents a held D-pad from dragging the cursor away
from the skill being read. Release, closing/covering the panel, or disabling the
feature restores stock updates. `access-skill-grid on|off` controls the feature.
Failed native guards/selection preserve stock input.

| Native operation | Derived evidence |
|---|---|
| Cursor update `0x234820`, 758 bytes | Caller `0x13E5E0` updates controller cursor objects; update uses free-cursor mode, velocities and snap interpolation |
| Cursor reset `0x2347B0`, 110 bytes | Copies `0x846150()` cursor coordinates into floats at `+0x18/+0x1C`, clears velocity/direction at `+0x20/+0x24/+0xA0/+0xA4` |
| Pending interpolation flag `+0x68` | `0x234820` interpolates the previous start/end positions while set; `0x235330` starts it; clear before the stock reset so release cannot resume an old snap |
| Cursor position `0x14F690`, 142 bytes | Receives packed two-int coordinates, clamps to current controller bounds, updates the native cursor and OS position; used by stock cursor commit `0x2346C0` |
| FocusableWidget selection `0x876CA0`, 209 bytes | Checks virtual `+0x90`, updates selection at `+0x544`, then sets native panel/widget focus through `0x868230`/`0x868350`; no click/purchase |
| Focus rectangle virtual `+0x80` | Stock cursor target search `0x2338B0` calls it with the widget's packed selection and compares its integer rectangle against cursor coordinates |
| Selection virtual `+0xA0`, owning panel `+0x550` | `0x876C80` dispatches selection; `0x868230` reads the owner. Adapter requires the verified base selection function and correct owner before selecting |

All four called/hooked functions are checked with complete-function FNV-1a
fingerprints from the analyzed live image. Extended traversal also requires the
existing NativeWidgets build checks. Native calls/writes are exception guarded;
the hook itself contains only delegation. No new runtime dependency is added.

## Why Options has three tabs with a controller

The layout lists Video, Audio, Gameplay and Controls. Native SettingsPanel setup
`0x147CBC0` adds its `Controls` entry only when controller mode (`0x077E10`) is
false and `0x847140` is false. The constructor builds the literal `Controls` as
`0x736C6F72746E6F43`; the branch calls the same `0x1478060` entry builder as the
other tabs. This confirms that the controller's three-tab cycle is stock behavior.

The fourth tab opens KeyBindingMenu, the keyboard binding page used during the
earlier desktop test setup. GameplayOptionsPanelHD.json instead contains the
controller cursor-speed slider, separate weapon-swap bindings toggle, and
thumbstick-swap toggle under `Row Controller Options`. These remain under **Juego**.

## Validation

Eighteen CTest suites / 972 checks pass in the deployed build, including the concurrent
radar/footstep changes. The added suites cover actual catalog ribbon actions,
hidden/wrong ancestry, sparse grid movement in four directions, boundaries,
held/released input, analog handoff, general/item-granted skills, covering/closed menus, disable and failed
native selection. Existing native UI tests cover dropdown/carousel inheritance.
Normal `build.ps1` deployed the plugin; built and installed SHA-256 matched:
`572D1BF8C90B370920B9895ADC092C94947A0FBE36DEC3BDED931DB18A324796`.

The game was closed during development. Controller movement, cursor/focus
alignment, audible output and the new hook still require live confirmation.
The user explicitly reported that they have not tried the new build yet.
In the next load, check `[skill-grid] Verified cursor adapter installed`, then
one `[skill-grid] Selected ...` per cardinal press, followed by the corresponding
`[interaction]` skill description. Check Options/Videos/Exit after leaving a game.
Do not infer real controller behavior from keyboard-only tests.
