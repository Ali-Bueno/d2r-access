# Diablo II: Resurrected — Sonar Navigation Radar Implementation Spec v2

**Audience:** Claude / Codex / coding agents implementing the accessibility mod.  
**Goal:** build a clean, low-spam, movement-relative audio radar for D2R that works in narrow interiors and very open procedural areas, preserves a rear sensor, supports future exits/beacons, and ships as close as possible to **install mod → launch game → play**.  
**Last research pass:** 2026-09-14.

---

## 0. Non-negotiable product goals

1. The core terrain radar must work without TTS during movement.
2. It must have **Front / Left / Right / Back** awareness.
3. `OPEN` and `BLOCKED` are one-shot events, not looping sonar.
4. The radar may **sample frequently**, but must **emit sparsely**.
5. It must work in narrow dungeons, doors, crossroads, irregular outdoor areas and large open spaces.
6. A heading change must not cause four fake topology events.
7. No auto-walk/pathfinder as the primary accessibility model.
8. Destination guidance is a separate optional beacon layer.
9. Production must not require a user to install Node, Python, Docker, a map server or a second Diablo II installation.
10. Prefer a single plugin DLL (plus D2RLoader if required) and embedded/default config/resources.

For generic anti-spam rules, also read `RADAR_ANTISPAM_STABILITY_SPEC.md` in the project/docs.

---

# 1. Target player experience

The player should hear the environment as **topological changes**.

```text
OPEN    -> tin
BLOCKED -> tock
```

Directional encoding:

```text
Front:  center, normal pitch
Left:   left spatialization/pan
Right:  right spatialization/pan
Back:   center or binaural rear position, slightly lower pitch
```

Example: entering a crossroad while travelling forward.

```text
Before:
  Front OPEN
  Left  BLOCKED
  Right BLOCKED
  Back  OPEN

At intersection:
  Front OPEN
  Left  OPEN     -> tin left
  Right OPEN     -> tin right
  Back  OPEN
```

No TTS is necessary to understand the intersection.

A large outdoor area should simply baseline as OPEN in the relevant directions and remain quiet until meaningful topology changes.

---

# 2. Important reference behavior: A Hero's Call vs Kirandur

The desired D2R system should combine the strongest ideas from both references rather than copying either literally.

## 2.1 A Hero's Call `ReactiveRadar.cs` — what the supplied code actually does

The supplied decompiled `ReactiveRadar.cs` has three beams:

```text
Left
Front
Right
```

It contains two logically separate radar mechanisms.

### A. Short-range approach/proximity tones

For a beam aligned with the current travel direction, it obtains the first collision and floors the distance. If the distance is below `3` and the integer distance changes, it plays one of three wall-approach pitches.

Conceptually:

```text
far -> pitch 1
mid -> pitch 2
near -> pitch 3
```

This is a **proximity** feature, not the `OPEN/CLOSE` topology detector.

### B. Long-range Open/Close topology detection

The same code asks for the first wall/impassable/door collision with a reach of:

```text
1000f world units
```

It then tracks how the returned boundary point evolves while the player moves. It stores successive collision points and constructs boundary lines; when the inferred boundary geometry changes, it decides whether a lateral space opened or closed.

Crucial observations:

- `OPEN/CLOSE` is not simply “is the next tile empty?”.
- It can observe geometry **far beyond the player-adjacent cell**.
- The open/close logic is skipped for the beam aligned with travel and for the opposite direction; it is mainly a side-topology detector.
- If either current or previous collision is missing, it resets its line state rather than spamming.
- Exact collision points are meaningful in its tile-like map model; this same exact-point strategy becomes noisy in continuous meshes, hence our separate anti-spam spec.

### C. The 0.1-second value is an event cooldown, not necessarily the scan rate

`RunRadar()` only sets:

The original A-Hero's Call radar schedules its next evaluation 0.1 seconds after a cue, using total elapsed seconds. Reference: `ReactiveRadar.NextRadarTime` in the linked A-Hero's Call study.

when a beam actually plays a sound. If no event is produced, the radar may be evaluated again on the next call/update.

**Design lesson for D2R:**

> sample often; gate audio events.

Do not lower the sampling rate merely to avoid spam.

### D. Do not hardcode “30 tiles” from this code

The supplied file proves a long-range reach of `1000f` **game world units**. It does not, by itself, establish the world-unit-to-tile scale. If 1000 units happens to correspond to roughly 30 tiles in that engine, that conversion must be proven elsewhere.

For D2R use a configurable range expressed in D2R **tiles/subtiles**, not `1000` copied from A Hero's Call.

---

## 2.2 Kirandur — behavior worth preserving

From the supplied Kirandur reverse-engineering spec:

- four horizontal cardinal probes include **rear coverage**;
- there are additional vertical probes for elevation;
- nominal sonar evaluation interval is `0.1 s`;
- rays march to a maximum range;
- semantic states include Open/Wall/Door/etc.;
- it uses diff/movement gating and hysteresis to suppress unchanged output;
- rear presentation is intentionally perceptually different from front;
- positional HRTF is used for spatial presentation.

For D2R V1 preserve:

```text
Front / Left / Right / Back
state change gating
rear distinction
stable movement-relative axes
```

Do **not** copy Kirandur's numeric distances directly. D2R has its own coordinate scale and a deterministic subtile collision grid.

---

# 3. Recommended D2R radar model: dual-range, one audio state

Use **two sensing ranges** per direction, but do not create two competing sets of sounds.

```text
                   player
                      |
       +--------------+--------------+
       |                             |
  Near/Body Probe               Far/Topology Probe
  “can I move?”                 “what space continues?”
       |                             |
       +--------------+--------------+
                      |
                stable OPEN/BLOCKED
                      |
                   tin/tock
```

## 3.1 Near / body probe

Purpose:

```text
Can the player's body begin moving in this direction safely?
```

It should use player-width collision, not one infinitely thin ray.

Preferred representations:

1. D2R's own movement/collision query, if located and safely callable.
2. D2R live room/subtile collision data, if mapped reliably.
3. Cached generated collision grid.

Suggested effective depth after D2R coordinate calibration:

```text
~1 to 3 player-body lengths
```

This probe is authoritative for immediate `BLOCKED`.

## 3.2 Far / topology probe

Purpose:

```text
How far does usable/open space continue in this direction?
```

This is the A-Hero's-Call-inspired part.

Initial configurable target:

Proposed topology reach is 20–35 tiles; this is a tuning range in the design, not measured production behavior.

D2 collision uses 5x5 subtiles per map tile, so when operating on a raw subtile grid:

Convert the proposed topology reach to subtiles using five subtiles per tile, keeping the coordinate unit explicit.

Start testing around **30 tiles / 150 subtiles**, but treat this as a tuning value, not a recovered D2 constant.

Why long range helps:

- a one-tile doorway can reveal a room or corridor behind it;
- a side opening does not depend only on one adjacent collision cell;
- open outdoor terrain naturally yields a long clear run / no hit;
- indoor and outdoor logic can remain the same.

### Important: far range does not mean more audio

A 150-subtile scan may be evaluated often while producing **zero** sound if its stable semantic state did not change.

---

# 4. Far probe implementation on a collision grid

Do not port A Hero's Call's exact `LastCollisionPoint -> Line` algorithm blindly. D2's deterministic grid lets us use a cleaner boundary representation.

For each relative direction, cast a small **parallel fan**:

```text
left lane   ------------------------------>
center lane ------------------------------>
right lane  ------------------------------>
```

Use 3 lanes initially; optionally 5 later.

Record:

Proposed `TopologyProbe` records the left, center and right clear depths, their median, and whether a blocker was hit within range. This section defines the proposed probe contract; current implementation status is recorded separately in `radar-redesign.md`.

A lane marches until the first cell that blocks player walking, or until max topology range.

Do not fire events because an individual hit moved from cell 150 to 149.

Quantize/filter distances before state decisions.

## 4.1 Suggested derived values

The proposed body-clear depth is the minimum compatible with actual player width. Center depth is the center-lane run; space-clear depth is the median of the left, center and right runs. Reference: proposed `TopologyProbe` in this section.

Use near/body probe + far depth together.

Suggested semantic classifier:

Classify a failed near-body sweep as BLOCKED. When the body sweep passes, classify sustained far-depth evidence as OPEN; use the thresholds and stability rules below for ambiguous observations. This is the proposed near/far decision policy.

`MinOpenDepth` should be tuned in tiles. Start around a few player lengths, **not** the full 30-tile max range.

The max range is context; the state threshold is local navigability/topology.

---

# 5. A-Hero's-Call-style boundary jump as optional supporting evidence

For side directions, an opening often looks like:

```text
normal corridor wall distance:  1.0 tile
at doorway/side room:            6.0 tiles
past doorway:                    1.0 tile
```

This distance jump is useful evidence.

Maintain a slowly varying baseline per direction:

The proposed per-direction `baselineBoundaryDepth` retains the previous supporting boundary depth for comparison with the next observation; it is an optional evidence source in this design.

Potential transition evidence:

The optional boundary-jump detector treats a sufficient increase from the baseline clear depth as opening evidence and a sufficient decrease as closing evidence. The separate open/close thresholds provide hysteresis; this remains supporting evidence, not body-clearance truth.

Do **not** let this evidence bypass:

- player-width passability;
- temporal hysteresis;
- movement confirmation;
- state dead-band.

It is a confidence signal, not a direct sound trigger.

This preserves what makes A Hero's Call perceptually effective without importing the fragile exact-hit-line behavior that causes chatter in arbitrary geometry.

---

# 6. Open outdoor areas

A far probe that does not hit anything within range means:

A probe with no hit within range reports the full topology range and records that no blocker was hit. It must not manufacture a boundary at that distance.

Treat this as stable `OPEN` evidence.

Rules:

1. On initialization, baseline silently; do not play four `tin`s.
2. While it remains open, stay silent.
3. If a wall/corridor boundary later becomes meaningfully near and remains stable, emit one `tock`.
4. If that boundary later opens again, emit one `tin`.

Therefore a huge open Blood Moor should not spam simply because no wall exists nearby.

---

# 7. Rear radar

D2R should keep the Kirandur-style **Back** sensor even though the supplied A Hero's Call class has only Left/Front/Right.

Back uses the same near + far topology pipeline:

```text
Back OPEN    -> tin at rear / lower pitch
Back BLOCKED -> tock at rear / lower pitch
```

V1 stereo fallback:

```text
pan: center
pitch multiplier: ~0.82..0.90
```

If using proper 3D spatialization/HRTF later, place the cue physically behind the listener and reduce pitch differentiation if unnecessary.

Rear events must still be state changes only. In normal forward traversal the path behind often stays OPEN, so it should be nearly silent.

---

# 8. Optional proximity layer (separate from OPEN/BLOCKED)

A Hero's Call also has a useful short-distance wall-approach pitch system. Consider this **after** the topology radar is stable.

Only for the direction the player is actively travelling into:

```text
wall far enough -> no approach sound
threshold 3     -> low approach pitch
threshold 2     -> medium pitch
threshold 1     -> high pitch
```

Do not reuse the `tin/tock` samples.

Do not play on every frame; play only when crossing a quantized distance band.

This can communicate “you are about to hit the wall” while `tin/tock` remains reserved for topological changes.

---

# 9. Sampling cadence vs audio cadence

Recommended production model:

```text
sampling: 20 Hz initially (50 ms)
audio state confirmation: ~80..150 ms
per-direction safety cooldown: ~100 ms
```

30–60 Hz sampling is acceptable if cheap and useful for fast movement.

The collision grid is small enough that four directional scans of ~150 subtiles with a few lanes are trivial when cached.

Important:

> A high sample rate must not imply a high sound rate.

A Hero's Call demonstrates the useful pattern of checking aggressively but imposing a short cooldown after an actual audible event. Kirandur demonstrates stable diff gating. D2R should use both ideas.

---

# 10. Anti-spam state machine

Do not emit directly from raw collision results.

Per direction:

The proposed `SensorState` stores stable and candidate navigation states (Unknown/Open/Blocked), candidate start time and position, last emission time, baseline boundary depth and an initialization flag. Candidate stability and cooldown checks described below control emission. See `radar-redesign.md` for the implemented replacement and its limits.

Pipeline:

```text
raw near/far probes
      -> spatial filtering
      -> observed state
      -> temporal hysteresis
      -> movement-distance confirmation
      -> stable state
      -> event coalescing
      -> one-shot audio
```

Recommended starting values after scale calibration:

Proposed tuning: stable time 80–120 ms, stable travel a small fraction of a tile/player step, per-direction safety cooldown about 100 ms, and approach reminders only on entering a new nearer distance bucket. These are design values requiring validation.

Use the dedicated anti-spam document for the full portable design.

---

# 11. Heading / coordinate frame

The radar is **movement-relative**.

Use actual displacement, not camera angle:

Derive travel direction from the difference between the current and previous player positions. Keep that measured movement separate from facing or input intent; this is the heading contract proposed in this section.

Maintain the last stable heading while stationary.

Start by quantizing to 8 movement sectors.

Derived directions:

In the proposed facing-relative frame, Front follows heading, Back is opposite it, and Left/Right are the respective quarter turns. Verify the game's handedness before using the frame.

## Silent heading rebase

Rotating the relative axes changes what every sensor points at. That is not a world topology change.

On a stable heading-sector change:

1. recompute all four sensors;
2. silently set new left/right/back baselines;
3. do not emit a four-cue burst;
4. after actual travel into the new heading, allow a single useful Front confirmation if desired;
5. resume normal transitions.

---

# 12. Crossroads and doors

## Crossroads

Do not require a bespoke intersection detector.

A crossroad emerges as simultaneous state changes:

```text
Left  BLOCKED -> OPEN
Right BLOCKED -> OPEN
```

Batch events from the same scan/coalescing window so both cues are heard together.

Unlike the supplied A Hero's Call class, do **not** stop after the first audible beam if that would hide the other side of an intersection.

## Doors

Best semantics:

```text
closed door -> BLOCKED
open door   -> OPEN
```

If using static/generated collision, overlay runtime door state.

If a D2R-native movement/collision query is available, prefer it because it naturally reflects current dynamic passability.

During door animation, confirm the new state only when the player's footprint can actually traverse the doorway.

---

# 13. Runtime data from D2R

Primary current references:

- https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/d2r-memory-offsets.md
- https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/pointer-chains.md
- https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/patching-guide.md

The current `diablo2utils` documentation identifies its offsets as build-specific. Never assume these remain valid after a D2R patch.

## 13.1 Player UnitAny

Current reference fields include:

```text
UnitAny +0x00 type
UnitAny +0x04 txtFileNo
UnitAny +0x08 unitId
UnitAny +0x0C mode
UnitAny +0x10 pData
UnitAny +0x18 actId
UnitAny +0x20 pAct
UnitAny +0x38 pPath
UnitAny +0x150 pNext
```

## 13.2 Player position

Current documented chain:

```text
UnitAny
 -> pPath @ +0x38
    -> x @ +0x02
    -> y @ +0x06
```

## 13.3 Level ID

```text
UnitAny
 -> Path
 -> Room
 -> RoomEx
 -> Level
 -> levelId
```

Current documented offsets:

```text
Path.pRoom      +0x20
Room.pRoomExt   +0x18
RoomEx.pLevel   +0x90
Level.levelId   +0x1F8
```

## 13.4 Difficulty and map seed

Current reference:

```text
UnitAny.pAct +0x20
Act.pActMisc +0x70
ActMisc.difficulty +0x830
ActMisc.initSeedHash +0x840
ActMisc.endSeedHash +0x860
```

Use the documented seed derivation rather than guessing it. `Act.mapSeed +0x1C` is documented as a less reliable fallback.

## 13.5 Unit table

Current documented build uses:

```text
UNIT_TABLE_OFFSET = 0x1EAA3D0
```

Treat this as a reference value only.

Prefer pattern discovery + validation against the live decrypted module.

---

# 14. Preferred collision source order

For the production radar, investigate these options in this order.

## Option 1 — D2R native walkability/collision function (preferred)

Find and safely call/hook the same local collision/pathing query D2R uses to decide whether the player can traverse a destination.

Advantages:

- reflects doors and dynamic state;
- no procedural generator dependency for the core radar;
- no duplicated map logic;
- ideal for one-shot installation.

Requirements:

- identify by reverse engineering/pattern scanning, not a hardcoded absolute address;
- validate function signature/calling convention;
- call on the correct game thread if required;
- fail quiet after patches.

The classic D2 engine is known to use a subtile collision/pathing model, but **do not copy classic function addresses into D2R**. Use classic sources only as conceptual landmarks.

## Option 2 — live D2R room/subtile collision structures

If the loaded room contains a usable collision map in D2R memory, read it directly and build a local cache around the player.

This could provide the best compromise:

```text
no full procedural regeneration
+ local deterministic grid
+ runtime truth
```

Current public `diablo2utils` documentation exposes the path to `Room/RoomEx/Level` but does not document a complete D2R collision-map layout, so this needs targeted RE.

## Option 3 — generated collision map

Use seed + difficulty + level to generate/copy the full static grid, then overlay dynamic objects.

This is excellent as a development oracle and can power exits/beacons. Production packaging needs special care; see the libd2 section below.

---

# 15. `libd2`: use, limits and integration

Repository:

- https://github.com/jaenster/libd2

C++ usage:

- https://github.com/jaenster/libd2/blob/main/docs/usage/cpp.md

C ABI/header:

- https://github.com/jaenster/libd2/blob/main/packages/drlg/include/d2drlg.h

`libd2` is a clean-room reimplementation of the Diablo II 1.14d engine core. Its DRLG reproduces seed-driven maps and exposes collision, rooms, presets and adjacencies.

Its current native C ABI can build shared/static native libraries and exposes functions such as:

```text
d2drlg_ctx_create
d2drlg_gen_act
d2drlg_gen_level
d2drlg_act_level_collision
d2drlg_act_level_walk
d2drlg_act_level_adjacents
```

Useful current details from the header:

- level origin/size accessors exist;
- raw collision is per subtile;
- the walk grid is already reduced to `0 = blocked`, `1 = walkable` using walking flags;
- a full D2 tile corresponds to 5x5 subtiles;
- generating a whole act once and reading accessors avoids repeated regeneration;
- single-level generation omits some cross-level seam information and has documented Great Marsh / Flayer Jungle collision exceptions, so **whole-act generation is safer when correctness matters**.

## 15.1 Do not make the public one-shot installer depend on the hosted `drlg-server`

Network map generation is unacceptable for the core accessibility path:

```text
no internet dependency
no service outage dependency
no privacy/network requirement
```

## 15.2 Important redistribution issue

`libd2` source is MIT, but its README explicitly says the DRLG package embeds small pre-baked blobs/fixtures derived from Blizzard game data and describes those as **not redistributable game content**.

Therefore:

> Do not blindly ship a prebuilt libd2 DRLG binary inside the public mod release until the contents and redistribution path are legally/technically resolved.

For development, `libd2` is an excellent map oracle.

For release, prefer D2R runtime collision for the core radar. If a generator is still needed for beacons/full map, investigate a method that consumes data from the user's own D2R installation at runtime rather than redistributing derived game assets.

---

# 16. Other map-generation references

These are **research references**, not automatic dependencies.

## D2RMH

- https://github.com/soarqin/D2RMH

Its changelog documents that it moved to loading data from D2R CASC storage directly for some data/resource needs, and it uses a modified d2mapapi pipeline. This is worth studying for a possible “use the user's installed D2R data” architecture.

Caution: its README/build path still contains legacy-D2 map-generation requirements in places. Verify the exact current code path before adopting anything.

## d2mapapi_mod

- https://github.com/soarqin/d2mapapi_mod

Useful for:

- collision-map representation;
- exits/objects/NPC output;
- simple pathfinding reference.

It is still architecturally tied to legacy D2 map generation and 32-bit components, so it is not the preferred one-shot runtime dependency.

## MapAssist

- https://github.com/D2RLegit/MapAssist

Useful as a reference for:

- current D2R memory/unit interpretation;
- exits/POIs;
- map-tool architecture.

Do not copy GPL code into an incompatible-licensed project without complying with GPL obligations.

---

# 17. D2RLoader integration and one-shot packaging

Plugin SDK:

- https://github.com/D2RLoader/PluginSDK

License of PluginSDK: MIT.

The SDK supports a normal shared-library plugin and documents global/mod-scoped plugin locations.

Recommended role for this accessibility plugin:

```text
PluginFlags::Client
```

Add `NativeHooks` only if the radar genuinely requires low-level D2R calls/hooks.

Why Client role is desirable:

- accessibility behavior is local/client-side;
- D2RLoader documents that client-only plugins are not recorded in character plugin history;
- avoid touching server/gameplay rules unless necessary.

## 17.1 Embedded config

D2RLoader can embed a default TOML config in the DLL and creates the user config automatically when absent.

Use this. The user should not need to copy/edit a default config manually.

## 17.2 Release packaging

D2RLoader documents plugin packaging and can produce a plugin DLL + companion MPQ when needed.

Target UX:

```text
unzip/copy accessibility mod
launch D2R through configured loader/mod
play
```

Do not require:

```text
pip install
npm install
Docker
separate map server
manual classic-D2 install
manual environment variables
```

If an installer is later created, it should detect the D2R directory, install/check D2RLoader, place the plugin and preserve user config.

---

# 18. Audio dependency recommendation

If the project does not already have a robust audio backend, prefer a dependency that can compile directly into the plugin.

Recommended candidate:

- **miniaudio:** https://github.com/mackron/miniaudio

Reasons:

- tiny integration surface;
- no external runtime dependency on Windows;
- can be compiled into the plugin;
- high-level sound management and optional 3D spatialization;
- permissive public-domain / MIT-0 choice.

For the cleanest initial package, embed very small radar WAV assets directly in the DLL/resources:

```text
radar_open.wav
radar_blocked.wav
(optional approach tones)
```

Avoid requiring loose codec DLLs.

Back pitch can be generated by playback pitch/resampling rather than maintaining a duplicate rear sound file.

If the current mod already has a proven sound backend, do not replace it merely for architectural purity.

---

# 19. Coordinate normalization

Do not assume D2R runtime positions and generated collision grids use the same frame without testing.

Centralize conversions:

The proposed position contract separates floating-point runtime coordinates (`RuntimePos`) from integer collision subtiles (`SubtilePos`). `RuntimeToCollision` would apply the level frame explicitly. These are design names; the adjacent native layout references establish the real coordinate sources.

Calibration tests:

1. read player position in Rogue Encampment;
2. compare against generated/live level origin;
3. walk one known tile and measure delta;
4. verify blocked/open cells around a doorway;
5. verify an outdoor level boundary;
6. verify an exit location;
7. only then freeze the conversion.

`libd2`'s current C ABI explicitly exposes level origins/sizes and collision grids with a 5x subtile relationship, which is useful for validation even if it is not shipped in the final mod.

---

# 20. Dynamic objects and doors

Keep terrain and moving units separate.

Unit types from current community references:

```text
0 Player
1 NPC/Monster
2 Object
3 Missile
4 Item
```

Do not let ordinary monsters continuously flip terrain `OPEN/BLOCKED`.

Door handling priority:

1. native D2R collision result;
2. verified runtime object mode/state overlay;
3. static generated state as fallback.

Objects can be identified through `UnitAny.type`, `txtFileNo`, `mode` and position. Verify door IDs/modes against current D2R tables/code; do not invent them.

---

# 21. Event batching

Scan all four directions before playing sounds.

Evaluate all directions first, collect their `RadarEvent` results, coalesce compatible events and then emit the resulting batch. This is the event-order proposal in this section, not a pasted implementation.

Rules:

- Left OPEN + Right OPEN in the same window: allow both.
- 3–4 changes caused by teleport/rebase: normally suppress and silently baseline.
- same direction/state repeated within safety cooldown: drop duplicate.
- terrain and combat/loot cues use distinct sound families.

---

# 22. Failure handling

Fail quiet, never invent topology.

Suspend radar if:

- player pointer/handle invalid;
- current area changes and collision source is not ready;
- coordinate conversion places player outside expected bounds;
- collision generator/runtime query fails;
- version/pattern validation fails.

On area/act/teleport transition:

```text
suspend audio
refresh player/map identity
refresh collision source
silent baseline
resume
```

---

# 23. Performance targets

A cached collision grid makes the radar cheap.

At 20 Hz:

```text
4 directions
x 3 lanes
x ~150 subtiles maximum
= ~1800 simple cell checks per update worst case
```

That is trivial in native code.

Early-out each lane on first blocker.

Map generation, if used, occurs on seed/act changes, never per radar tick.

---

# 24. Recommended configuration surface

Keep sensible defaults so setup is unnecessary.

```toml
[radar]
enabled = true
sample_hz = 20
front = true
left = true
right = true
back = true

# expressed in D2 tiles, converted internally
far_range_tiles = 30
near_range_tiles = 2
min_open_depth_tiles = 3

stable_time_ms = 100
safety_cooldown_ms = 100

back_pitch = 0.86
left_pan = -0.9
right_pan = 0.9

proximity_enabled = false
```

Do not expose twenty obscure tuning knobs to normal users initially. Keep advanced diagnostics/tuning under a developer section.

---

# 25. Debugging required for coding agents

Log per direction:

```text
heading
nearPassable
left/center/right clearDepth
median clearDepth
observed state
candidate state
stable state
candidate age
player travel during candidate
boundary baseline
emitted/suppressed + reason
```

Provide an optional developer command/key that speaks the four stable states and current clear depths for blind debugging.

Also support dumping a local ASCII walk grid around the player.

---

# 26. Test matrix

At minimum:

1. Rogue Encampment — buildings/open town.
2. Blood Moor — broad outdoor procedural area.
3. Cold Plains — very open area + exits.
4. Cave — narrow winding interior.
5. Catacombs — doors and intersections.
6. Act II tombs — repeated rooms/doorways.
7. Arcane Sanctuary — narrow unusual traversal.
8. Act III jungle — complex outdoor edges.
9. Great Marsh / Flayer Jungle — important if validating generated collision.
10. Durance / Worldstone — dense dungeon topology.
11. closed/open door.
12. 90/180-degree heading changes.
13. joystick zig-zag.
14. waypoint/portal teleport.
15. new game / different procedural seed.

### Required perceptual tests

**Straight wall:** no repeated open/close while following it.  
**Single side door:** exactly one side `tin`, then one `tock` after passing it.  
**Crossroad:** left/right `tin` together, no serial spam.  
**Open field:** silent stable OPEN after baseline.  
**Rear:** rear state distinguishable but not chatty.  
**Turn in place:** no four-sound burst.  
**Fast movement:** no missed doorway due low sampling frequency.

---

# 27. Implementation order

## Phase A — D2R runtime identity

1. D2RLoader plugin lifecycle.
2. local-player readiness/event lifecycle.
3. player position.
4. area/level ID.
5. act/difficulty.
6. seed for validation/future full-map features.
7. pattern/offset validation and logging.

## Phase B — authoritative local collision

1. Investigate D2R native walkability/collision query.
2. Investigate live room collision structures.
3. Build a `CollisionProvider` interface.
4. Only if needed, plug generated collision behind the same interface.

The proposed `ICollisionProvider` exposes a read-only walkability query for a `SubtilePos`. Ownership and validity must remain with the provider; see this phase and the runtime-truth requirements above before adopting an implementation.

## Phase C — silent dual-range radar

1. stable movement heading;
2. four relative axes including Back;
3. near body probe;
4. 20–35-tile far topology fan;
5. clear-depth logging;
6. state/hysteresis with no sound.

## Phase D — one-shot audio

1. `tin/tock`;
2. left/right spatialization;
3. lower-pitch Back;
4. event batching;
5. silent heading rebase;
6. tune outdoors + interiors.

## Phase E — proximity feedback

Optional A-Hero's-Call-style approach pitches for the active movement direction.

## Phase F — semantic/map features

1. doors/runtime objects;
2. exits/waypoints;
3. optional destination beacon;
4. optional route-waypoint beacon, never auto-walk.

---

# 28. Distribution architecture decision

### Preferred production path

```text
D2R
 + D2RLoader
 + one accessibility client plugin
      - memory/native collision adapter
      - radar engine
      - embedded audio engine/assets
      - embedded default config
```

This gives the cleanest end-user experience.

### `libd2` role

```text
Development oracle / validation tool: YES
Optional full-map/beacon backend: MAYBE
Mandatory public runtime dependency: NO, unless redistribution/data issue is resolved
Hosted web service dependency: NO
```

### Avoid for production core radar

```text
external EXE map server
Docker
32-bit child process
classic Diablo II install
HTTP request for map on every game
manual map files per seed
```

---

# 29. Source links coding agents must inspect

## D2R plugin/runtime

- D2RLoader PluginSDK  
  https://github.com/D2RLoader/PluginSDK

- D2RLoader PluginSDK README / packaging/services  
  https://github.com/D2RLoader/PluginSDK/blob/master/README.md

## Current D2R memory research

- diablo2utils — memory offsets  
  https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/d2r-memory-offsets.md

- diablo2utils — pointer chains  
  https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/pointer-chains.md

- diablo2utils — patch/pattern scanning  
  https://github.com/ChrisTitusTech/diablo2utils/blob/master/docs/patching-guide.md

## Deterministic map/collision generation

- libd2  
  https://github.com/jaenster/libd2

- libd2 C++ usage  
  https://github.com/jaenster/libd2/blob/main/docs/usage/cpp.md

- libd2 DRLG C ABI  
  https://github.com/jaenster/libd2/blob/main/packages/drlg/include/d2drlg.h

## Other D2R map research

- D2RMH  
  https://github.com/soarqin/D2RMH

- d2mapapi_mod  
  https://github.com/soarqin/d2mapapi_mod

- MapAssist  
  https://github.com/D2RLegit/MapAssist

- @diablo2 tools (D2R memory + classic/resurrected map research)  
  https://github.com/blacha/diablo2

## Audio

- miniaudio  
  https://github.com/mackron/miniaudio

---

# 30. Final design rule

Do not optimize for “the ray found a wall”.

Optimize for the perceptual question:

> **What meaningful movement option has just appeared or disappeared relative to me?**

The far scan may inspect ~30 tiles. The near scan may inspect only a few subtiles. Both may run 20+ times per second. Yet if nothing meaningful changes, the correct output is:

```text
silence
```

That is the standard by which this radar should be judged.
