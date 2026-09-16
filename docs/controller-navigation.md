# Controller menu and skill navigation follow-up

Target: D2R 3.3.93787, updated 2026-09-16. Native C++ / D2RLoader, existing PRISM sink.
Latest follow-up: [skill descriptions, controller actions and coverage](skill-actions.md).
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
native cursor-update hook delegates to `native_cursor_gesture.cpp`, cancelling
both interpolation and the latched directional snap before the stock reset.
Changing direction or rolling through a diagonal without fully releasing the
D-pad stays in the same gesture. Each native cursor consumes one final cleanup
update after release before stock stick movement resumes. Closing/covering the
panel or disabling the feature also releases ownership with that cleanup.
`access-skill-grid on|off` controls the feature.
Failed native guards/selection preserve stock input.

| Native operation | Derived evidence |
|---|---|
| Cursor update `0x234820`, 758 bytes | Caller `0x13E5E0` updates controller cursor objects; update uses free-cursor mode, velocities and snap interpolation |
| Cursor reset `0x2347B0`, 110 bytes | Copies `0x846150()` cursor coordinates into floats at `+0x18/+0x1C`, clears velocity/direction at `+0x20/+0x24/+0xA0/+0xA4` |
| Pending interpolation flag `+0x68` | `0x234820` interpolates the previous start/end positions while set; `0x235330` starts it; clear before the stock reset so release cannot resume an old snap |
| Held-direction flag `+0xB0` | `0x234820` compares current direction activity from `0x234480` with this byte. A held-to-released transition calls `0x235330` using the saved direction at `+0xC0`. Stock reset does not clear it; clear it with interpolation to prevent a second release-triggered move |
| Native cursor capacity | `0x13E5E0` walks eight cursor objects (`0x1C8 / 0x39` qwords); each needs independent release cleanup |
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

The user reported on 2026-09-16 that navigation appeared to work but a D-pad
press seemed to count twice. The preceding 23:47 controller log records a
mod selection of `SkillButton3 0,0` at 32.923, Aullido at 33.004, and Burla at
33.600 without another `[skill-grid] Selected` event. Other moves show the same
uncommanded second focus change. This supports a pending native cursor move,
rather than duplicate grid-selection calls. Reinspection of `0x234820` identified
the separate release-triggered snap described above.

Thirteen UI CTest suites / 383 checks pass, including 39 native gesture checks
and 35 skill-navigation checks. The regression fixture preserves the stock
reset's omission of both snap flags and exercises held input, dispatch after
release sampling, independent native cursors, analog handoff, diagonal/direction
changes, immediate deliberate subsequent presses, and failed memory access.
The existing suites cover actual catalog ribbon actions,
hidden/wrong ancestry, sparse grid movement in four directions, boundaries,
held/released input, analog handoff, general/item-granted skills, covering/closed menus, disable and failed
native selection. Existing native UI tests cover dropdown/carousel inheritance.
The full shared-workspace run at 00:04 passed 19 of 21 suites; radar-timing and
radar-guidance failed while the separate radar agent was editing those sources.
Those changes are outside this fix. Normal `build.ps1` compiled and deployed the
cursor correction; the subsequent shared build retained it. Built and installed
SHA-256 matched at verification:
`D26B2CA80FF1D58EEA26EC06C1E55005B56C2640AC3CA898A39A06BC19DA9E2B`.

The game was closed during development. The adapter installed successfully in
the preceding live log; the release correction still needs controller validation.
In the next load, check `[skill-grid] Verified cursor adapter installed`, then
one `[skill-grid] Selected ...` per cardinal press, followed by the corresponding
`[interaction]` skill description with no second move on release. Check
Options/Videos/Exit after leaving a game.
Do not infer real controller behavior from keyboard-only tests.
