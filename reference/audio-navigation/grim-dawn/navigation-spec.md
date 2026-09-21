# Grim Dawn — navigation model (study of the `grimdark` mod)

Source: <https://github.com/ahicks92/grimdark> (MIT, commit `247ee0b`, 2026-09-19). ~26k lines C++.
Read from **published source**, not a decompile. Studied 2026-09-20 (code-level pass on pathfinding/probing the same day) for reuse in
[`d2r-access`](../../../d2r-access/STATUS.md).

Engine: Grim Dawn 1.3.0.8 x64, native C++, closed source. Framework: injected DLL + **Detours**,
**PRISM** for speech, **miniaudio**, **SQLite**. Same family as our D2R plugin.

> **Naming trap:** `src/core/navigator.*`, `graph_builder.*`, `key_graph.*`, `graph_announcer.*` are the
> **UI menu focus graph** (a port of Factorio Access's `key-graph.lua`), *not* world navigation. World
> navigation is `src/rooms.*` + `src/core/rooms_model.*` + `src/world.*` + `src/sonar.*`.

---

## 1. Four independent layers

| Layer | Answers | Where |
|---|---|---|
| **Wall tones** | "what box am I in *right now*" | `src/screens/in_game.cpp:29-151` |
| **Entity sonar** | "what's around me and how far" | `src/sonar.cpp`, `src/core/sonar_field.*` |
| **Rooms** | "where am I, what is this place, where are its exits" | `src/rooms.*`, `src/core/rooms_model.*` |
| **Review + ping** | "can I actually get to that thing" | scanner keys + `assets/audio/review_*.wav` |

Style 3 (wall sonification) + style 2 on demand + a named-place layer the other studies here don't have.

## 2. Walkability: the game's own navmesh, raycast live

Grim Dawn uses **Detour** (recast navmesh). The mod calls the engine's exported queries instead of
reimplementing collision:

- `world::free_distance_ray` (`src/world.cpp:1135-1169`) → `NavManager::FindStraightMovePoint`
  = `findNearestPoly(from)` + `dtNavMeshQuery::raycast`; walks polygon portals and stops at the first
  edge with no neighbour, returning the **exact** hit point.
- Superseded `IsPointOnPathMesh` sampling, which is a **bounding-box** test and read holes as walkable
  (`docs/re_wall_sliding.md:110-147`).
- Lane starts are gated by `FindClosestPointOnPathMesh` within `kLaneStartTol = 0.2f` units, so a probe
  starting inside a wall reads 0 instead of snapping to a nearby polygon.
- The **rooms exit list** (and only it) uses `GAME::Player::FindPath` plus the corridor from
  `GAME::NavManager::FindPath` — **the game's real pathfinder**, so quest gates, closed doors and
  destructibles are honoured for free (`src/rooms.cpp:335-406`). The review ping does **not** (see §6).

**Lesson:** find the engine's own nav query and call it. Everything downstream inherits the game's truth.

## 3. Wall tones (continuous bed)

- Four **ear-fixed** looping tones, `assets/audio/walltones/2/{north,east,south,west}.wav`
  (`in_game.cpp:59-60`). Pan is hard-wired per direction: E `+1.0`, W `-1.0`, N/S `0.0`.
- Volume = **`(1 - d/range)²`**, `range = 10` world units (`in_game.cpp:53,121`); silent at `d >= range`.
- Recomputed **every frame**, no smoothing and no hysteresis — deliberate, "audible as lag"
  (`in_game.cpp:109`). Muted when the game window loses focus.
- The camera is **pinned at yaw 0** every tick (`world::pin_camera`), so screen-up == world `-z` and the
  four directions never rotate. See `docs/compass.md`.

### The probe is a rectangle and the answer is the MAX over its lanes

`rect_distance` (`in_game.cpp:69-77`): 7 lanes at 0.5 u spacing (half-width 1.5 u), each a navmesh
raycast; the direction's distance is the **maximum** over lanes — "a wall is a wall only when the whole
rectangle hits it". A side lane whose start is not contained in the mesh within `kLaneStartTol = 0.2 u`
reads 0 instead of snapping onto a nearby polygon. The hazard probe does the opposite (min over lanes):
**max for "can I go", min for "is there something bad"** (`src/hazard.cpp:83-100`).

### The probe width is derived, not guessed

The probe is a **rectangle**, not a ray, and its half-width is **at least 1.25 units — the game's own
per-frame movement look-ahead**. Grim Dawn's WASD movement re-issues "walk to a point 1.25u ahead" each
frame and lets Detour snap that point onto the boundary edge, which is what produces wall-sliding
(`docs/re_wall_sliding.md:8-19,82-101`). So the character slides through any gap narrower than its own
look-ahead; a probe narrower than 1.25u reports "wall" where the player would actually round the corner
(live: 1.0u false wall, 1.5u correct). **The tolerance comes from the movement code, not from tuning.**

## 4. Entity sonar (metronome, not a sweep)

Replaced an earlier left-right sweep. Per entity within `radius = 25` units (`world::scan`), a pulse
whose **period** is interpolated logarithmically in distance: `0.14 s` at 2u → `0.80 s` at 25u
(`core/sonar_field.h:15-19`). One WAV per kind (enemy, loot, transition, destructible, shrine ruined,
interactable), each with a K-weighted dB trim so they sound equally loud (`docs/sonar-loudness.md`).
Gain `ref/(ref+dist)` with `ref=10`, floor 0.2; `-10 dB` rear shelf when behind. New ids are seeded
mid-period so arrivals don't burst; after a stall it skips whole periods instead of catching up.

`core::SonarField` is a **pure scheduler** (engine-free, unit-tested): in = `(id, distance, phase, kind)`,
out = which ids fire this frame. `sonar.cpp` does the game and audio side. Same split as our `core/`.

## 5. Rooms: an offline-baked semantic map

The part with no equivalent in the other studies, and the part that depends most on the game's shape.

**Offline** (`tools/gdmap/`, 1.4k lines Python): parse `.arc` archives → `world001.map` → per-chunk
Detour tile-cache layers (0.25 units/cell, 32-unit tiles) → walkability + floor height grids; then
segment each region: seam-stitch → island drop → hole fill → clearance EDT → **persistence watershed** →
small-region merge → **walk-cap bisection** (any room whose geodesic diameter exceeds 60 units is cut) →
classify as passage / corridor / small / hall / room (`tools/gdmap/rooms.py`). Titles and one-sentence
descriptions are generated by a **vision model per room** from a downscaled screenshot plus terrain facts,
run through a mechanical quality gate (banned phrases, no digits, no neighbour names).

**Artifact:** `assets/rooms.db`, SQLite, 74 MB — **149 regions, 18 070 rooms, 925 sub-regions**, plus a
frozen 43 MB base-game copy. Grids are RLE'd `int16` label-per-cell (`-1` = unwalkable) with a height
plane and sparse overlays for bridges/stacked floors. Room key is anchor-based
(`<region>:<round x>:<round z>`) so authored text survives re-segmentation.

**Runtime:** opened read-only; a region is decoded once into memory on first need and cached. Per frame
there is **no SQL** — just `LabelGrid::label_at()`, a nearest-labelled-cell ring search (8 cells),
height-aware. Room changes pass `core::rooms::Hysteresis` (`settle_ms = 1000`, `dwell_ms = 400`,
hand-tuned) — note the inversion: **the dwell applies only inside the first second after a change**;
once the player has been in a room for a second, a new label is announced instantly. Off-grid keeps the
current room.

**Intra-region exits are not read from the database, and only on demand (V / Shift+V).** Candidates =
every other room label with cells within `kExitRadius = 28 u`, each reduced to its nearest cell
(`src/rooms.cpp:335-406`). Four gates, all fail-open (a query that cannot run keeps the exit):
`Player::FindPath != 0` → drop (locked quest door, NavBlocker); arrival gate (the reached point must be
within `kReachTol = 1.5 u` of the target or inside the neighbour, because Detour snaps unreachable goals
and still returns 0); floor gate (1.5 u); directness gate (a corridor run > 2 u through a *third* room →
drop). The spoken point is **where the corridor enters the neighbour** (first corridor sample labelled
with it), not a centroid. Line of sight is deliberately not required. **"blocked" is never spoken for an
exit** — `exit_blocked()` has no callers; it is only said by the follow key when the ping is unreachable.
Only cross-region seams come from the DB.

Speech: `"<region>, <sub-region>, <title>"` on entering, only the parts that changed; `X` repeats title +
description; `V` / `Shift+V` cycles exits nearest-first, continuing from the currently reviewed item —
`"room 190, 5 away, 4 o'clock, 1 of 3"`. Exits are never re-pinged as you walk (their point moves with
you; "read as noise", 2026-08-25).

## 6. Review + ping: reachability *is* the sound

The strongest idea in the mod, and the cheapest to copy.

Category cycling keys — `.` enemies, `,` highest rarity only, `N` interactables, `B` bystanders/breakables,
`M` loot, `V` exits, `]` pets, `C` characters; `Alt+key` = nearest of that group. Then **`;` pings whatever
is under review**, and the ping sound itself is the answer:

| Sound | Meaning |
|---|---|
| `review_straight.wav` | the straight line is clear |
| `review_path.wav` | target stands on the mesh but the line is broken — **inferred, never verified** |
| `review_unreachable.wav` | target (pulled 0.5 u toward the player) is off the mesh |

`route_kind_to` (`src/world.cpp:2373-2389`) is a 0.5 u point walk along the straight line, terrain-hugging,
with **no pathfinder call**: "path" can be spoken for a target with no route at all (chasm, locked door).
It still uses the bounding-box `IsPointOnPathMesh`, so it can say "straight" through a rock (known,
unfixed). Recomputed every frame (`reping_tick`, ~0.25 ms at 40 u) but a sound plays **only when the
`(reviewed id, kind)` pair changes** — a change-only event, exactly our radar's model. Panned and faded by
distance. No turn-by-turn guidance exists — `'` follows a chosen map marker with a
route ping plus name, distance and **clock bearing**, and that's the whole navigation aid.

Note the division: **clock hours only in speech; the continuous bed stays at four fixed directions.**

## 7. Engineering rules worth stealing (`CLAUDE.md` "Design rules")

- **Never hold an engine pointer across frames.** Store an id and re-resolve it every frame you use it.
  Two shipped crashes came from this: a cached `GraphNode*` after a rebuild, and a locked review target
  re-resolved only every 30 frames and projected stale in between.
- **Version gate before any hook** (`src/version_gate.cpp`): exe + `Engine.dll` + `Game.dll` PE timestamps
  must match a table row, else one spoken line ("unsupported game build, exe timestamp …") and **nothing
  is installed**. A byte-signature check stays as backstop.
- `src/core` is **engine-free** (no Windows, no game types) and unit-tested with doctest.
- The launcher forces the two settings the mod cannot work without (keyboard movement, evade-to-cursor
  off) on every start, rather than documenting them.
- Read the **render** position, not the logical one; mirror the game's own visibility rules; deliberate
  deviations go in an explicit "Enhancements" category (`docs/design-notes-from-wotr.md`).

---

## 8. What transfers to Diablo II: Resurrected

| Idea | Verdict for D2R |
|---|---|
| Review + 3-state reachability ping | **Ported 2026-09-20** (beacon lock says straight / around / no route). Ours is stronger: "around" is verified by the route search, grimdark only infers it. Change-only key `(target, verdict)`. |
| Exit cycling with destination, distance, clock bearing | **Port.** Nearest-first, cycle continues from the current item, point = where the route enters the neighbour, no "blocked" word, never re-pinged while walking. Our rooms layer speaks directions once on entering only. |
| Probe = rectangle, MAX over lanes, half-width ≥ the game's own movement look-ahead | **Port, highest value.** Our `ScanPassage` corridor (1-cell lateral slack) is already a max-over-lanes; the width must come from D2's own controller move step / collision pattern, not from tuning. A lane that starts inside a wall must read 0. |
| Call the engine's pathfinder for reachability | **Investigate.** Grimdark only does it for the exit list; our BFS already verifies reachability, so the gain is honouring doors/objects the way the game does. |
| Room-change hysteresis (settle 1000 / dwell 400 ms, dwell only inside the settle window) | **Port.** Cheap, and our room announcements have no equivalent gate. |
| Id-not-pointer rule, version gate, engine-free `core/` | **Adopt.** We already do the last one. |
| **Offline baked rooms DB** | **Does not port.** Grim Dawn's world is fixed and hand-made, so 18 070 rooms can be described once. D2 generates every level per game from DRLG + DS1 tiles, so there is nothing stable to bake or to name. |
| The **segmentation algorithm** behind that DB | **Not needed — investigated 2026-09-20 and dropped.** The plan was to run clearance EDT → watershed → classify at level entry. Two facts killed it and made the feature *smaller*: D2 only keeps a collision grid for **active** rooms (`Room1+0x38`, freed on unload), so a level cannot be segmented on entry at all; and a maze level (`Levels.txt DrlgType = 1`) is assembled from `LvlPrest` cells where **one streamed room already is one chamber** (40x40 subtiles in the Crypt, 120x120 in the Den of Evil). D2 ships the segmentation. See `d2r-access/docs/rooms.md`. |
| LLM-authored room descriptions | **Does not port.** Replace with classification plus D2's own level name. |
| Continuous wall tones, and distance coded as pulse **rate** | **Rejected by the user for this mod.** Our radar fires only on change, one sound per meaning, speed not used as a channel. Keep four directions and the tin/tock design. |
