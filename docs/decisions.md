# Decisions log

Dated log of design decisions and explicitly rejected alternatives, newest first. Short entries —
the *how* lives in the linked docs, this page only records the *why* and what was turned down.

## 2026-09-25 — Radar rays along the walls: the stick diagonals

The user found that a corridor could read as wall in every direction and asked to
fix it, after being offered rays aligned with the walls. D2's walls run along the
world axes, at 45 degrees to the four stick cardinals chosen on 2026-09-16, so in a
world-axis corridor every cardinal ray meets a wall. The game rotates the stick by
45 degrees into the world (41 logged stick/velocity pairs: median error 6 degrees,
against 14.5 for the screen projection), so the stick's diagonals are the world
axes. Chosen: the radar's four rays follow the stick diagonals by default, still
four rays with the same sounds, panned to their side, the lower two at down's pitch.
Kept selectable: `[radar] diagonals = false` or `access-radar cardinal`. Not
revisited: eight rays (rejected 2026-09-16). Every other feature keeps its screen
directions. `docs/radar-readings.md`.

## 2026-09-25 — L3 is the only stick button; R3 back to the game

The user removed the R3 gestures, then the L3+R3 chord too: the scanner already
covers the exit review ("next way out") and the terrain overview. Kept: L3 (the
beacon). The exit review stays on Ctrl+V and the overview on Ctrl+O, since only
the controller gestures were asked about. With no R3 gesture left, the pad
filter that hid R3 from the game (`PadFilter`, added 2026-09-21 so the beacon
click would not swap weapons) had no purpose and was removed with the chord
logic (`StickChord`), so R3 does what the game binds it to. L3 is now reported
on press, since no chord has to be told apart. Recoverable from git history.

## 2026-09-25 — Beacon on L3, one-press retarget; darker downward breath only

The user moved the beacon from R3 to L3 because R3 opens the emote wheel, and
asked that L3 on a newly scanned target switch the lock in one press, while L3
on the same target cancels. Chosen: a retarget checks the new target before
releasing the old one, so a refused target (no known route) keeps the current
lock. A short R3 click is now unbound; the R3 hold and the L3+R3 chord stay.
For the radar, only the breath (the "wind") got a larger downward drop
(`BreathDownPitch` 0.55); the tin, tock and knock keep `DownPitch` 0.86, the
sounds the user chose on 2026-09-16. Rejected: widening `DownPitch` for every
cue. `docs/beacon.md`, `docs/radar-readings.md`.

## 2026-09-24 — Fixed places found on a map stay known on that map

The user asked for exits already discovered on a map or dungeon to be readable
from far away, only on that same map: a map regenerated after a game shows its
exits only once found again. Then: waypoints too, and anything fixed, but not
objects, which change. Chosen: exits, waypoints and the stash
(`MapSurvey::Fixed`) the scanner counts as discovered go into the
explored-rooms archive under the same key (map seed, level, room-layout
digest), and the scanner lists them in later games. The key already tells a
regenerated map apart, online or offline. Rejected: a separate file (a second
store for the same key); recording what only full assistance listed (never
discovered); shrines, chests, quest objects and portals, whose type, state or
presence changes between games. `docs/exploration.md#on-disk`.

## 2026-09-24 — Unexplored rooms at the edge of the map, in the scanner

The user, after playing: keep the radar, but list unexplored rooms even when unloaded,
without revealing exits, so a lost player can head for new ground. Chosen: a separate
*unexplored room* category listing only rooms near explored ground (`RoomGraph::Near`). The
beacon may enter such a room and arrives on entry. Rejected: every unexplored room of the
level, because it would lay out the whole level's shape and point toward its far end.
`docs/entity-scan.md`.

## 2026-09-24 — Exploration follows the automap's drawn rooms

Live sessions confirmed the native automap reader (`docs/automap-reader.md`).
The automap is saved with the character, but its object glyphs are not. So
"explored" is room-level: a logical room with any drawn floor or wall cell, or
one the player stood in. The scanner lists landmarks in explored rooms. The
beacon routes through them. This is less exact than matching each glyph, but
it survives a reload and needs no per-object reveal rule.

## 2026-09-24 — One scanner on the right stick, following the automap

The user asked the scanner to follow the automap, with no L3 mode. Chosen: the
right stick always drives the catalogue (up/down categories, left/right
objects). Enemies stay as a live nearest-first category. Ground loot and
ordinary chests leave the scanner: they are not on the automap, and their loops
already cover them. The directional "what is to my left" scan was removed, not
moved to another gesture (user's choice). Until the native automap reveal state
is readable, discovery keeps the range-and-sight approximation. `docs/entity-scan.md`.

## 2026-09-24 — Pathfinder: exploration first, full assistance opt-in

The user felt the pathfinder led the player by the hand and wanted to encourage
exploration. Chosen: limit what it can solve, not how often. The catalogue only
orients (straight-line distance and direction to what was discovered), and the
beacon only routes through visited rooms, so it helps you return rather than
discover. Full assistance stays as an option, off by default (the user's choice).
Rejected: removing the pathfinder; a cooldown (one use still skips the
exploration, and when lost it only makes you wait). `docs/beacon.md#exploration-mode`.

## 2026-09-23 — Potion assignments in inventory; View reserved for settings

The user chose belt consultation through inventory focus, including the assigned
D-pad direction. Do not add a View/Back hold for potion review. The user plans a
future mod settings menu on View/Back and wants to move keyboard shortcuts there.
This task does not implement that menu or remove existing bindings. `docs/belt.md`.

## 2026-09-21 — Unexplored as a qualifier on the radar, not a new sound or a guide

The user asked for a sound-first way to find unexplored ground without spam. Chosen: a short tail
on the existing exit/floor cues when the way leads somewhere never covered, so no new event is ever
added; the meaning is per level type (chamber never stood in, level never seen, frontier of the
streamed ground on open levels). Plus tags in the exit review and room line, a percent in the
terrain overview, and the two memories (seen/visited) persisted per map seed like the game's
`.ma0`. Rejected: a continuous tone or bed (the radar already sounds every step), an automatic
beacon to the nearest frontier (too close to the routed navigation turned down on 09-20), a
sonified four-direction sweep (repeats what Ctrl+O says). `docs/exploration.md`.

## 2026-09-21 — Terrain overview instead of level announcements

Do not announce undiscovered levels — it breaks immersion. Approved instead: an accessible
automap-equivalent "terrain overview" (Ctrl+O / L3+R3). See `docs/terrain-overview.md`.

## 2026-09-20 — Navigation must be organic, not routed

Navigation follows the Grim Dawn mod (`grimdark`) model: the player learns where they are and its
exits (room name, exit label, distance, clock bearing, reachability), never a dictated step
sequence ("up 6, right 4"). Rejected: a "route to nearest unexplored space" feature (PR #3);
removed from the merge, kept alive on branch `feature/sight-explore-containers` if ever wanted.
Design and ported behavior: `docs/rooms.md`, `docs/beacon.md`,
`reference/audio-navigation/grim-dawn/navigation-spec.md` §8.

## 2026-09-20 — Radar precision is the priority

The radar (wall/opening cues) is what the user lives with every step; it comes first. Navigation-layer
features (exit review, beacon) are separate, explicitly requested work — do not fold them into a
"polish the radar" session unasked.

## 2026-09-16 — Radar cue redesign: kept 4 cardinals and the original sound, fixed thresholds

User's rule: *"si hay pared suena pared, si hay salida suena salida, y ya; solo avísame de los
cambios; da igual si camino rápido o lento."* Rejected after listening: eight directions ("mucha
información, con 4 ya bastaba"), a new sound palette with pitch glides ("los de antes me parecían
mucho mejor"), and speed-scaled warning distance. Kept: four screen cardinals, the original 120 ms
tin/tock cues (1100/260 Hz, Down 14% lower, pan ±0.9, level 0.15), fixed distance thresholds
(open 10 / close 7 cells after the 18:10 forensic fix). Full rationale and parameters:
`docs/radar-redesign.md`.

## 2026-09-16 — Quest tracking: automatic, no hotkey

Rejected an on-demand "read my current objective" hotkey. Required instead: (1) the quest panel
navigable/readable when opened, and (2) the new objective spoken automatically whenever the game
shows the change visually (alert, kill counter, step completed) — parity with what a sighted
player perceives passively. Implemented behavior: `docs/quests.md` ("Automatic announcements").

## 2026-09-14/15 — Core architecture

D2RLoader plugin + PRISM in-process, speech on its own thread (PRISM/NVDA `speak` blocks
300-800 ms), native widget text read via D2RCore's slot table, `d2rloader.toml` must set
`text_locale = "esES"`, and `D2R.exe`'s `.text` is Arxan-encrypted on disk so the plugin dumps live
code instead of relying on static RE. Already recorded in detail in `STATUS.md` (Hard-won facts)
and `AGENTS.md` (Rules / Hard-won facts) — this entry only marks that those were 2026-09-14/15
decisions, not new information.
