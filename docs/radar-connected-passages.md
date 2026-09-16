# Connected passage radar

> Superseded by the 2026-09-16 redesign (docs/radar-redesign.md); retained as history.

> Follow-up: [remembered wall references](radar-wall-memory.md) now prevent
> retreat from a known wall from sounding like a new exit. The original physical
> approach must clear the remembered edge as well as the short useful depth.

Implemented 2026-09-16 following the user's request for less clutter and more
precise navigation. This supersedes automatic ray-hit/surface-change cues in
[surface tracking](radar-surface-tracking.md). The underlying native collision
reader, fixed screen directions, object filter and audio timings remain.

## The behavior change

Previously, a wall moving from about ten to eighteen subtiles away could emit
an opening, then a closing when the near wall returned. Both positions already
allowed useful travel. Running past successive recesses produced repeated cues
without a new navigation decision.

The automatic radar now retains one passage state per direction. A new opening
means useful travel has become available; a closing means it has been lost near
a structural barrier. More distant changes in the same usable space remain
silent. This deliberately trades descriptions of distant room contours for
fewer, more actionable local cues. Ctrl+T still describes real movement and
straight physical clearances, including limited and occupied space.

## Native body connectivity and direction

`navigation_grid` is now linked into the plugin. Each 50 ms radar poll builds
fresh local body graphs from the structural view and the original movement
view. For the default cross body, each graph covers 25 by 25 collision subtiles.
There is no five-subtile coarsening, persistent room cache or new native call.
The underlying body tests retain the recovered point/cross/box patterns,
movement mask and native diagonal destination-step rules.

`radar_passage` queries each graph in the fixed up/left/right/down axes:

- The first step must follow the named axis. Later steps never retreat along it.
- Lateral adjustment is bounded by one native body span: three cells for a
  cross/box or one for a point. An arbitrary route around a distant wall cannot
  turn globally reachable space into a forward opening.
- Search stops once it proves the configured useful forward depth (eight
  subtiles by default). The logged connected depth is this bounded proof,
  not a route length or distance to the next room.
- Missing cells before sufficient reach leave the query unavailable. Unknown
  space is never classified as a wall. Distant missing rooms do not erase an
  already proved local route.

Connectivity can retain an opening around a slightly offset edge. **A new
opening still requires the original full-body straight sweep and lane checks
to reach eight subtiles.** A nearby offset door waits until the player aligns
with it. A creature or retained object blocking that turn prevents the sound.
The structural object filter never supplies physical passage clearance.

## State and delivery

The closing threshold is `max(blockedDepth, openDepth - bodySpan)`: five subtiles
for the usual cross body. This is an explicit navigation policy, not a recovered
game constant. A body span separates opening and closing thresholds so ordinary
raster fluctuations do not repeatedly change the state. A short known route
closes at this threshold; if sideways steps along an angled wall extend its
projection, the direct wall clearance also supplies the warning when no
connected route reaches eight subtiles. A still-usable offset route retains
state but never authorizes a physically blocked opening cue.

Changes require two actual observations, at least one 50 ms poll apart. Speed
and queue length do not change the geometry state. Initialization, lifecycle
rebases and local coverage recovery establish silent baselines. An occupied
opening waits for physical clearance; if it closes first, it cannot leave an
orphan closing. An unchanged opening stays consumed until a confirmed closure
rearms it. Each direction retains its own state, including the rear direction.

The existing 120 ms sounds, 50 ms gaps, serialized queue and unsubmitted-only
delivery retries remain. The service and delivery simulation share the same
audibility predicate. Queued openings are revalidated against current physical
clearance. The old `BoundaryTracker` remains in historical comparison tests;
`radar_geometry.cpp` is no longer linked into the plugin.

Generated events now carry `trigger=passage`, `connected`, `physical_connected`
and `known`. Ctrl+T additionally logs `passage_pending` and `passage_state`,
alongside the physical map snapshot. Logging remains on events or explicit
queries, never every frame.

## Validation and limits

All **27 suites / 1358 checks pass**. The new passage suite adds 74 checks:
six native body patterns, undersized holes, offset doors, globally reachable
space requiring a remote detour, disconnected space, blocked first steps,
unknown coverage, resident/unloaded room seams, native actor occupancy,
state hysteresis and the recorded camp post.

Twenty sustained far-wall changes produce zero automatic events, including
changes lasting longer than confirmation. The earlier running far-recess test
now also requires silence. Real near-wall openings still produce one opening
and closing per side on outbound and return trips at 50/150/200 ms polling.
The full engine/audio-queue simulations continue to verify both sides completing
their cues, and retry/partial-playback regressions still pass. Historical tests
that explicitly demanded far-recess sounds were updated to the new contract;
finite-opening delivery fixtures now use walls that actually limit useful travel.

A 35-room in-process timing sample measured about 0.97 ms median and 1.88 ms
maximum per engine tick on this machine. This excludes native snapshot reading
and does not measure live controller behavior or sound perception.

This is local passage interpretation, **not persistent doorway IDs, a complete
level map or destination guidance**. Very brief openings can still fall between
two polls. Closures are local warnings, not guaranteed reaction time at every
speed or with every sound queue. Native room coverage and ambiguous collision
flag ownership retain their existing limits. The user must still assess the
installed change with their controller and audio; automated fixtures do not
establish audible precision in all D2R maps. The next useful comparisons are
camp clutter, Blood Moor walls, Den of Evil passages and room/level transitions.
