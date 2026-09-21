# Decisions log

Dated log of design decisions and explicitly rejected alternatives, newest first. Short entries —
the *how* lives in the linked docs, this page only records the *why* and what was turned down.

## 2026-09-21 — Terrain overview instead of level announcements

Do not announce undiscovered levels — it breaks immersion. Approved instead: an accessible
automap-equivalent "terrain overview" (Ctrl+O / BACK+B on `bladestorm`, R3 double-click upstream). See `docs/terrain-overview.md`.

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
