# Testing and contributing

## Target player

The mod is built for a **blind player**: no visual feedback can be assumed in a test script. They
use **NVDA**, an **Xbox-style controller** (A confirm, B back, D-pad, LB/RB, right stick for
beacon/exit review), and play the game in **Spanish** (`esES`). See `README.md` and `AGENTS.md`
for the current feature/control list.

## How tests are written and verified

- Tests are scripted as **controller button / key sequences** with the expected spoken output next
  to each step — never "look at the screen". See `docs/discovery-run-controller.md` for a worked
  example (front-end menu discovery run).
- Verify results from the plugin's own log:
  `d2rloader/logs/d2r-access.log` (under the game's install folder, e.g.
  `D:\games\steam\steamapps\common\Diablo II Resurrected\`).
- Automated suites live in `tests/`; run them per `AGENTS.md` ("Build, deploy, test").

## Local computer-use validation (unattended)

When the user is away, the game can be driven unattended with the repo's own harness —
keyboard input and screenshots only, **no game-memory writes**. Tool mechanics and key bindings:
`tools/computer_use/README.md`; first live measurements: `docs/computer-use-radar-check.md`.

Procedure, verified 2026-09-20:

1. `Start-Process D2RLoader.exe` from the game folder opens straight to the title screen (no
   login/update prompt while Battle.net runs in the background).
2. `space` goes to character select. Offline test characters: **YARKEY** (level 5 Barbarian,
   first, preselected) and **JOLINE** (level 2 Sorceress). Click the character, then Play.
3. Movement is bound to the game's own keys: F9 up, F10 right, F11 down, F13 left (F13 avoids
   Steam's F12 screenshot action — see `tools/computer_use/README.md`).
4. Diagnostics: **Ctrl+T** dumps the radar snapshot, **Ctrl+V** (or an R3 hold) reviews exits, **Ctrl+B** locks the
   beacon (key equivalents of the controller gestures in `docs/rooms.md` / `docs/beacon.md`).
5. **Guardrail: stay in the Rogue Encampment.** No Blood Moor, no waypoint, no NPCs, no settings —
   keep the unattended session inside camp only.
6. Exit sequence: Escape -> Save and Exit -> Exit Diablo II, then confirm `D2R.exe` has actually
   exited before ending the session.
7. Check `d2rloader.toml`'s `text_locale` before trusting spoken output in a log: it must be
   `esES` (the user's language) — a run left at `enUS` produces English speech and is not a valid
   accessibility check.

### Decoding `[radar-collision]` snapshots

The log's `[radar-collision]` lines are rate-limited (1/s), run-length-encoded raw collision
flags meant for offline replay. Format and fields: `docs/radar-redesign.md`
("Console / log format"), decode examples: `docs/radar-revisits.md`, `docs/radar-sliding.md`.
Decode the RLE snapshot and re-run the BFS/route search independently of the live log's own
verdict before concluding a cue is wrong — this caught a real false "open" that no unit test
covered (2026-09-20 unattended run).
